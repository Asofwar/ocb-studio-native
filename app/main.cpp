#include "AppController.hpp"
#include "ocb/ui/MainWindow.hpp"

#include <QApplication>

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    QApplication::setApplicationName("OCB Studio");
    QApplication::setOrganizationName("OCB Studio");

    ocb::AppController controller;
    ocb::ui::MainWindow window(controller);
    window.resize(1280, 820);
    window.show();

    return QApplication::exec();
}
