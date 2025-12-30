#pragma once

#include <QMainWindow>

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

private:
    void setup_ui();
    void setup_menus();
};
