#include "ocb/ui/FieldTableModel.hpp"

#include <QString>

#include <sstream>

namespace ocb::ui {
namespace {

QString hexOffset(std::uint32_t value) {
    return QString("0x%1").arg(value, 0, 16).toUpper();
}

QString optionText(const core::OcbField& field, std::uint64_t value) {
    for (const auto& option : field.options) {
        if (option.value == value) {
            return QString("%1 - %2").arg(QString::fromStdString(option.label)).arg(value);
        }
    }
    return QString::number(value);
}

} // namespace

FieldTableModel::FieldTableModel(QObject* parent)
    : QAbstractTableModel(parent) {}

void FieldTableModel::setFields(std::vector<core::OcbField> fields) {
    beginResetModel();
    fields_ = std::move(fields);
    endResetModel();
}

void FieldTableModel::setProfile(const core::OcbProfile* profile) {
    beginResetModel();
    profile_ = profile;
    endResetModel();
}

const core::OcbField* FieldTableModel::fieldAt(const QModelIndex& index) const {
    if (!index.isValid() || index.row() < 0 || index.row() >= static_cast<int>(fields_.size())) {
        return nullptr;
    }
    return &fields_.at(static_cast<std::size_t>(index.row()));
}

int FieldTableModel::rowCount(const QModelIndex& parent) const {
    return parent.isValid() ? 0 : static_cast<int>(fields_.size());
}

int FieldTableModel::columnCount(const QModelIndex& parent) const {
    return parent.isValid() ? 0 : ColumnCount;
}

QVariant FieldTableModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || role != Qt::DisplayRole) {
        return {};
    }

    const auto& field = fields_.at(static_cast<std::size_t>(index.row()));
    switch (index.column()) {
    case PromptColumn:
        return QString::fromStdString(field.prompt);
    case VarStoreColumn:
        return QString::fromStdString(field.varStore);
    case OffsetColumn:
        return hexOffset(field.varOffset);
    case SizeColumn:
        return QString("%1 bit").arg(field.sizeBits);
    case ValueColumn:
        return valueText(field);
    default:
        return {};
    }
}

QVariant FieldTableModel::headerData(int section, Qt::Orientation orientation, int role) const {
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole) {
        return {};
    }

    switch (section) {
    case PromptColumn:
        return "Prompt";
    case VarStoreColumn:
        return "Store";
    case OffsetColumn:
        return "Offset";
    case SizeColumn:
        return "Size";
    case ValueColumn:
        return "Current";
    default:
        return {};
    }
}

QString FieldTableModel::valueText(const core::OcbField& field) const {
    if (profile_ == nullptr) {
        return {};
    }
    try {
        return optionText(field, profile_->read(field));
    } catch (...) {
        return "error";
    }
}

} // namespace ocb::ui
