#pragma once
#include <QWidget>


struct SelectiveStretchWindow : QWidget
{
    static SelectiveStretchWindow* create(struct SubWindow* subwindow);
    ~SelectiveStretchWindow() override;


    struct Data;
private:
    std::unique_ptr<Data> m;
    explicit SelectiveStretchWindow(SubWindow* subwindow);

protected:
    void closeEvent(QCloseEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void showEvent(QShowEvent* event) override;
};
