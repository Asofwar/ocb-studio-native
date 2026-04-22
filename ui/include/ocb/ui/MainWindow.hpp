#pragma once

#include "FieldTableModel.hpp"

#include <QMainWindow>

class QCheckBox;
class QComboBox;
class QLineEdit;
class QLabel;
class QPushButton;
class QTableView;

namespace ocb {
class AppController;
}

namespace ocb::ui {

class MainWindow final : public QMainWindow {
public:
    explicit MainWindow(AppController& controller, QWidget* parent = nullptr);

private:
    AppController& controller_;
    FieldTableModel* fieldModel_{};
    QTableView* fieldTable_{};
    QLineEdit* searchEdit_{};
    QLineEdit* valueEdit_{};
    QComboBox* presetCombo_{};
    QCheckBox* compensateCheck_{};
    QLabel* statusLabel_{};
    QLabel* selectionLabel_{};

    void buildUi();
    void refreshFields();
    void refreshStatus();
    void showError(const QString& title, const std::exception& error);
    [[nodiscard]] const core::OcbField* selectedField() const;

private:
    void openOcb();
    void openIfr();
    void openBios();
    void saveOcb();
    void applyPreset();
    void writeSelectedValue();
    void resetProfile();
    void updateSelection();
};

} // namespace ocb::ui
