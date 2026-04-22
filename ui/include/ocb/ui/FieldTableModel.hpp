#pragma once

#include "ocb/core/OcbField.hpp"
#include "ocb/core/OcbProfile.hpp"

#include <QAbstractTableModel>

#include <optional>
#include <vector>

namespace ocb::ui {

class FieldTableModel final : public QAbstractTableModel {
public:
    enum Column {
        PromptColumn = 0,
        VarStoreColumn,
        OffsetColumn,
        SizeColumn,
        ValueColumn,
        ColumnCount,
    };

    explicit FieldTableModel(QObject* parent = nullptr);

    void setFields(std::vector<core::OcbField> fields);
    void setProfile(const core::OcbProfile* profile);
    [[nodiscard]] const core::OcbField* fieldAt(const QModelIndex& index) const;

    [[nodiscard]] int rowCount(const QModelIndex& parent = {}) const override;
    [[nodiscard]] int columnCount(const QModelIndex& parent = {}) const override;
    [[nodiscard]] QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    [[nodiscard]] QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

private:
    std::vector<core::OcbField> fields_;
    const core::OcbProfile* profile_{};

    [[nodiscard]] QString valueText(const core::OcbField& field) const;
};

} // namespace ocb::ui
