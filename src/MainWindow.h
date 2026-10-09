#pragma once
#include <QMainWindow>


struct MainWindow : QMainWindow
{
    MainWindow();
    ~MainWindow() override;

    struct Data;

protected:
    void closeEvent(QCloseEvent* event) override;
    void focusInEvent(QFocusEvent* event) override;
};
