#include "main_window.hpp"
#include <QApplication>

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    app.setApplicationName("Engine Editor");
    app.setOrganizationName("Engine");

    MainWindow window;
    window.show();

    return app.exec();
}
