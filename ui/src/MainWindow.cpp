#include "ocb/ui/MainWindow.hpp"

#include "AppController.hpp"
#include "ocb/core/OcbException.hpp"
#include "ocb/core/FieldValidation.hpp"
#include "ocb/core/Preset.hpp"

#include <QCheckBox>
#include <QComboBox>
#include <QFileDialog>
#include <QHeaderView>
#include <QAbstractItemView>
#include <QItemSelectionModel>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QRegularExpression>
#include <QRegularExpressionValidator>
#include <QSignalBlocker>
#include <QSplitter>
#include <QStackedWidget>
#include <QTableView>
#include <QVBoxLayout>
#include <QVariant>
#include <QWidget>

#include <algorithm>
#include <charconv>
#include <QStringList>

namespace ocb::ui {
namespace {

std::uint64_t parseValue(const QString& text) {
    auto value = text.trimmed().toStdString();
    const auto split = value.rfind(" - ");
    if (split != std::string::npos) {
        value = value.substr(split + 3);
    }

    std::uint64_t parsed{};
    const int base = value.rfind("0x", 0) == 0 || value.rfind("0X", 0) == 0 ? 16 : 10;
    const auto input = base == 16 ? std::string_view(value).substr(2) : std::string_view(value);
    const auto result = std::from_chars(input.data(), input.data() + input.size(), parsed, base);
    if (result.ec != std::errc{} || result.ptr != input.data() + input.size()) {
        throw core::OcbException("Некорректное числовое значение: " + value);
    }
    return parsed;
}

QString optionLabel(const core::OcbOption& option) {
    return QString("%1 - %2").arg(QString::fromStdString(option.label)).arg(option.value);
}

QString optionLabelForValue(const core::OcbField& field, std::uint64_t value) {
    const auto option = std::find_if(field.options.begin(), field.options.end(), [&](const core::OcbOption& candidate) {
        return candidate.value == value;
    });
    if (option != field.options.end()) {
        return optionLabel(*option);
    }
    return QString::number(value);
}

QString formatBytes(std::uint64_t size) {
    if (size >= 1024ULL * 1024ULL) {
        return QString("%1 MiB").arg(static_cast<double>(size) / (1024.0 * 1024.0), 0, 'f', 2);
    }
    if (size >= 1024ULL) {
        return QString("%1 KiB").arg(static_cast<double>(size) / 1024.0, 0, 'f', 1);
    }
    return QString("%1 B").arg(size);
}

QString valueOrUnknown(const std::string& value) {
    return value.empty() ? QString::fromUtf8("не определено") : QString::fromStdString(value);
}

QString offsetText(std::uint64_t offset) {
    return QString("0x%1").arg(offset, 0, 16).toUpper();
}

} // namespace

MainWindow::MainWindow(AppController& controller, QWidget* parent)
    : QMainWindow(parent),
      controller_(controller) {
    buildUi();
    refreshFields();
    refreshStatus();
    refreshMetadata();
}

void MainWindow::buildUi() {
    auto* central = new QWidget(this);
    auto* root = new QVBoxLayout(central);

    auto* toolbar = new QHBoxLayout();
    auto* openOcbButton = new QPushButton("Открыть OCB", central);
    auto* openBiosButton = new QPushButton("Открыть BIOS", central);
    auto* openIfrButton = new QPushButton("Открыть IFR", central);
    auto* saveButton = new QPushButton("Сохранить MsOcFile.ocb", central);
    auto* resetButton = new QPushButton("Сбросить", central);

    presetCombo_ = new QComboBox(central);
    presetCombo_->addItem("Выберите пресет...");
    for (const auto& preset : core::builtinPresets()) {
        presetCombo_->addItem(QString::fromStdString(preset.name));
    }
    auto* applyPresetButton = new QPushButton("Применить пресет", central);

    compensateCheck_ = new QCheckBox("Компенсация контрольной суммы", central);
    compensateCheck_->setChecked(true);

    toolbar->addWidget(openOcbButton);
    toolbar->addWidget(openBiosButton);
    toolbar->addWidget(openIfrButton);
    toolbar->addWidget(saveButton);
    toolbar->addWidget(resetButton);
    toolbar->addSpacing(20);
    toolbar->addWidget(presetCombo_);
    toolbar->addWidget(applyPresetButton);
    toolbar->addStretch();
    toolbar->addWidget(compensateCheck_);

    searchEdit_ = new QLineEdit(central);
    searchEdit_->setPlaceholderText("Поиск полей: CEP, Lite Load, Current, Turbo...");

    fieldModel_ = new FieldTableModel(this);
    fieldTable_ = new QTableView(central);
    fieldTable_->setModel(fieldModel_);
    fieldTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
    fieldTable_->setSelectionMode(QAbstractItemView::SingleSelection);
    fieldTable_->horizontalHeader()->setStretchLastSection(true);
    fieldTable_->verticalHeader()->hide();

    auto* editor = new QHBoxLayout();
    selectionLabel_ = new QLabel("Поле не выбрано", central);
    valueEdit_ = new QLineEdit(central);
    valueEdit_->setPlaceholderText("Новое значение");
    valueEdit_->setValidator(new QRegularExpressionValidator(QRegularExpression(R"(\s*(0[xX][0-9A-Fa-f]+|[0-9]+)\s*)"), valueEdit_));
    valueCombo_ = new QComboBox(central);
    valueCheck_ = new QCheckBox(central);
    valueStack_ = new QStackedWidget(central);
    valueStack_->addWidget(valueEdit_);
    valueStack_->addWidget(valueCombo_);
    valueStack_->addWidget(valueCheck_);
    auto* writeValueButton = new QPushButton("Записать значение", central);
    editor->addWidget(selectionLabel_, 2);
    editor->addWidget(valueStack_, 1);
    editor->addWidget(writeValueButton);

    statusLabel_ = new QLabel(central);
    metadataLabel_ = new QLabel(central);
    metadataLabel_->setWordWrap(true);

    root->addLayout(toolbar);
    root->addWidget(searchEdit_);
    root->addWidget(fieldTable_, 1);
    root->addLayout(editor);
    root->addWidget(statusLabel_);
    root->addWidget(metadataLabel_);

    setCentralWidget(central);
    setWindowTitle("OCB Studio Native");

    connect(openOcbButton, &QPushButton::clicked, this, &MainWindow::openOcb);
    connect(openBiosButton, &QPushButton::clicked, this, &MainWindow::openBios);
    connect(openIfrButton, &QPushButton::clicked, this, &MainWindow::openIfr);
    connect(saveButton, &QPushButton::clicked, this, &MainWindow::saveOcb);
    connect(resetButton, &QPushButton::clicked, this, &MainWindow::resetProfile);
    connect(applyPresetButton, &QPushButton::clicked, this, &MainWindow::applyPreset);
    connect(writeValueButton, &QPushButton::clicked, this, &MainWindow::writeSelectedValue);
    connect(searchEdit_, &QLineEdit::textChanged, this, [this] { refreshFields(); });
    connect(fieldTable_->selectionModel(), &QItemSelectionModel::selectionChanged, this, &MainWindow::updateSelection);
    connect(valueCheck_, &QCheckBox::toggled, this, [this](bool checked) {
        const auto* field = selectedField();
        if (field != nullptr) {
            valueCheck_->setText(optionLabelForValue(*field, checked ? 1U : 0U));
        }
    });
}

void MainWindow::refreshFields() {
    const auto query = searchEdit_ == nullptr ? QString{} : searchEdit_->text();
    auto fields = query.isEmpty()
        ? controller_.catalog().fields()
        : controller_.catalog().search(query.toStdString());
    fieldModel_->setProfile(controller_.hasProfile() ? &controller_.profile() : nullptr);
    fieldModel_->setFields(std::move(fields));
}

void MainWindow::refreshStatus() {
    const auto ocb = controller_.ocbPath().empty() ? QString("нет") : QString::fromStdWString(controller_.ocbPath().wstring());
    const auto bios = controller_.biosPath().empty() ? QString("нет") : QString::fromStdWString(controller_.biosPath().wstring());
    const auto ifr = controller_.ifrPath().empty() ? QString("нет") : QString::fromStdWString(controller_.ifrPath().wstring());
    statusLabel_->setText(QString("OCB: %1 | BIOS: %2 | IFR: %3 | Полей: %4")
        .arg(ocb)
        .arg(bios)
        .arg(ifr)
        .arg(controller_.catalog().fields().size()));
}

void MainWindow::refreshMetadata() {
    QStringList lines;

    if (controller_.hasProfile()) {
        const auto& metadata = controller_.profile().metadata();
        QStringList profileParts;
        profileParts << QString::fromUtf8("формат: %1").arg(QString::fromStdString(metadata.format));
        profileParts << QString::fromUtf8("размер: %1").arg(formatBytes(metadata.fileSize));
        profileParts << QString::fromUtf8("$OCI$: %1").arg(metadata.hasOciSection ? offsetText(metadata.ociOffset) : QString::fromUtf8("не найден"));
        profileParts << QString::fromUtf8("плата: %1").arg(valueOrUnknown(metadata.boardName));
        profileParts << QString::fromUtf8("BIOS: %1").arg(valueOrUnknown(metadata.biosVersion));
        profileParts << QString::fromUtf8("профиль: %1").arg(valueOrUnknown(metadata.profileName));
        lines << QString::fromUtf8("OCB-метаданные - %1").arg(profileParts.join(QString::fromUtf8(" | ")));
    } else {
        lines << QString::fromUtf8("OCB-метаданные - профиль не загружен");
    }

    if (controller_.biosMetadata().has_value()) {
        const auto& metadata = *controller_.biosMetadata();
        QStringList biosParts;
        biosParts << QString::fromUtf8("плата: %1").arg(valueOrUnknown(metadata.boardName));
        biosParts << QString::fromUtf8("BIOS: %1").arg(valueOrUnknown(metadata.biosVersion));
        biosParts << QString::fromUtf8("образ: %1").arg(formatBytes(metadata.imageSize));
        biosParts << QString::fromUtf8("Setup PE32: %1").arg(formatBytes(metadata.setupPe32Size));
        biosParts << QString::fromUtf8("IFR: %1 вопросов, %2 полей").arg(metadata.questionCount).arg(metadata.fieldCount);
        if (!metadata.setupPath.empty()) {
            biosParts << QString::fromUtf8("модуль: %1").arg(QString::fromStdString(metadata.setupPath));
        }
        lines << QString::fromUtf8("BIOS-метаданные - %1").arg(biosParts.join(QString::fromUtf8(" | ")));
    } else {
        lines << QString::fromUtf8("BIOS-метаданные - образ BIOS не загружен");
    }

    metadataLabel_->setText(lines.join('\n'));
}

void MainWindow::showError(const QString& title, const std::exception& error) {
    QMessageBox::critical(this, title, QString::fromStdString(error.what()));
}

const core::OcbField* MainWindow::selectedField() const {
    const auto rows = fieldTable_->selectionModel()->selectedRows();
    if (rows.empty()) {
        return nullptr;
    }
    return fieldModel_->fieldAt(rows.first());
}

std::uint64_t MainWindow::editorValue(const core::OcbField& field) const {
    switch (core::valueEditorKind(field)) {
    case core::ValueEditorKind::Boolean:
        return valueCheck_->isChecked() ? 1U : 0U;
    case core::ValueEditorKind::Enumeration:
        return valueCombo_->currentData().toULongLong();
    case core::ValueEditorKind::Numeric:
        return parseValue(valueEdit_->text());
    }
    return parseValue(valueEdit_->text());
}

void MainWindow::openOcb() {
    try {
        const auto path = QFileDialog::getOpenFileName(this, "Открыть OCB-профиль", {}, "Профиль MSI OC (*.ocb);;Все файлы (*.*)");
        if (path.isEmpty()) {
            return;
        }
        controller_.openOcb(path.toStdWString());
        refreshFields();
        refreshStatus();
        refreshMetadata();
    } catch (const std::exception& error) {
        showError("Не удалось открыть OCB", error);
    }
}

void MainWindow::openIfr() {
    try {
        const auto path = QFileDialog::getOpenFileName(this, "Открыть текст IFR", {}, "Текст IFR (*.txt);;Все файлы (*.*)");
        if (path.isEmpty()) {
            return;
        }
        controller_.openIfrText(path.toStdWString());
        refreshFields();
        refreshStatus();
        refreshMetadata();
    } catch (const std::exception& error) {
        showError("Не удалось открыть IFR", error);
    }
}

void MainWindow::openBios() {
    try {
        const auto path = QFileDialog::getOpenFileName(
            this,
            "Открыть образ BIOS",
            {},
            "Образы BIOS (*.bin *.rom *.cap *.fd *.a* *.A*);;Все файлы (*.*)");
        if (path.isEmpty()) {
            return;
        }
        controller_.openBiosImage(path.toStdWString());
        refreshFields();
        refreshStatus();
        refreshMetadata();
    } catch (const std::exception& error) {
        showError("Не удалось открыть BIOS", error);
    }
}

void MainWindow::saveOcb() {
    try {
        const auto path = QFileDialog::getSaveFileName(this, "Сохранить OCB-профиль", "MsOcFile.ocb", "Профиль MSI OC (*.ocb);;Все файлы (*.*)");
        if (path.isEmpty()) {
            return;
        }
        controller_.saveOcb(path.toStdWString(), compensateCheck_->isChecked());
        QMessageBox::information(this, "Сохранено", "OCB-профиль сохранен.");
    } catch (const std::exception& error) {
        showError("Не удалось сохранить", error);
    }
}

void MainWindow::applyPreset() {
    try {
        if (presetCombo_->currentIndex() <= 0) {
            return;
        }
        controller_.applyPreset(presetCombo_->currentText().toStdString());
        refreshFields();
    } catch (const std::exception& error) {
        showError("Не удалось применить пресет", error);
    }
}

void MainWindow::writeSelectedValue() {
    try {
        const auto* field = selectedField();
        if (field == nullptr) {
            return;
        }
        controller_.writeField(field->id(), editorValue(*field));
        refreshFields();
    } catch (const std::exception& error) {
        showError("Не удалось записать значение", error);
    }
}

void MainWindow::resetProfile() {
    controller_.resetProfile();
    refreshFields();
}

void MainWindow::configureValueEditor(const core::OcbField& field) {
    std::uint64_t currentValue = 0;
    if (controller_.hasProfile()) {
        currentValue = controller_.profile().read(field);
    }

    switch (core::valueEditorKind(field)) {
    case core::ValueEditorKind::Boolean: {
        const QSignalBlocker blocker(valueCheck_);
        valueCheck_->setChecked(currentValue != 0);
        valueCheck_->setText(optionLabelForValue(field, valueCheck_->isChecked() ? 1U : 0U));
        valueStack_->setCurrentWidget(valueCheck_);
        break;
    }
    case core::ValueEditorKind::Enumeration: {
        const QSignalBlocker blocker(valueCombo_);
        valueCombo_->clear();
        int currentIndex = 0;
        for (const auto& option : field.options) {
            valueCombo_->addItem(optionLabel(option), QVariant::fromValue<qulonglong>(option.value));
            if (option.value == currentValue) {
                currentIndex = valueCombo_->count() - 1;
            }
        }
        valueCombo_->setCurrentIndex(currentIndex);
        valueStack_->setCurrentWidget(valueCombo_);
        break;
    }
    case core::ValueEditorKind::Numeric:
        valueEdit_->setPlaceholderText(QString("0..%1").arg(core::fieldMaxValue(field)));
        valueEdit_->setText(controller_.hasProfile() ? QString::number(currentValue) : QString{});
        valueStack_->setCurrentWidget(valueEdit_);
        break;
    }
}

void MainWindow::updateSelection() {
    const auto* field = selectedField();
    if (field == nullptr) {
        selectionLabel_->setText("Поле не выбрано");
        valueEdit_->clear();
        valueCombo_->clear();
        valueCheck_->setText({});
        valueStack_->setCurrentWidget(valueEdit_);
        return;
    }
    selectionLabel_->setText(QString("%1 (%2+0x%3)")
        .arg(QString::fromStdString(field->prompt))
        .arg(QString::fromStdString(field->varStore))
        .arg(field->varOffset, 0, 16));
    configureValueEditor(*field);
}

} // namespace ocb::ui
