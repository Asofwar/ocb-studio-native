#include "ocb/ui/MainWindow.hpp"

#include "AppController.hpp"
#include "ocb/core/OcbException.hpp"
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
#include <QSplitter>
#include <QTableView>
#include <QVBoxLayout>
#include <QWidget>

#include <charconv>

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
    if (result.ec != std::errc{}) {
        throw core::OcbException("Некорректное числовое значение: " + value);
    }
    return parsed;
}

} // namespace

MainWindow::MainWindow(AppController& controller, QWidget* parent)
    : QMainWindow(parent),
      controller_(controller) {
    buildUi();
    refreshFields();
    refreshStatus();
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
    auto* writeValueButton = new QPushButton("Записать значение", central);
    editor->addWidget(selectionLabel_, 2);
    editor->addWidget(valueEdit_, 1);
    editor->addWidget(writeValueButton);

    statusLabel_ = new QLabel(central);

    root->addLayout(toolbar);
    root->addWidget(searchEdit_);
    root->addWidget(fieldTable_, 1);
    root->addLayout(editor);
    root->addWidget(statusLabel_);

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

void MainWindow::openOcb() {
    try {
        const auto path = QFileDialog::getOpenFileName(this, "Открыть OCB-профиль", {}, "Профиль MSI OC (*.ocb);;Все файлы (*.*)");
        if (path.isEmpty()) {
            return;
        }
        controller_.openOcb(path.toStdWString());
        refreshFields();
        refreshStatus();
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
        controller_.writeField(field->id(), parseValue(valueEdit_->text()));
        refreshFields();
    } catch (const std::exception& error) {
        showError("Не удалось записать значение", error);
    }
}

void MainWindow::resetProfile() {
    controller_.resetProfile();
    refreshFields();
}

void MainWindow::updateSelection() {
    const auto* field = selectedField();
    if (field == nullptr) {
        selectionLabel_->setText("Поле не выбрано");
        valueEdit_->clear();
        return;
    }
    selectionLabel_->setText(QString("%1 (%2+0x%3)")
        .arg(QString::fromStdString(field->prompt))
        .arg(QString::fromStdString(field->varStore))
        .arg(field->varOffset, 0, 16));
    if (controller_.hasProfile()) {
        valueEdit_->setText(QString::number(controller_.profile().read(*field)));
    }
}

} // namespace ocb::ui
