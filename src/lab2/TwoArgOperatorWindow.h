#pragma once
#include <QWidget>


struct TwoArgOperatorWindow : QWidget
{
    static TwoArgOperatorWindow* create(struct SubWindow* subwindow);
    ~TwoArgOperatorWindow() override;

    struct Data;
private:
    std::unique_ptr<Data> m;
    explicit TwoArgOperatorWindow(SubWindow* subwindow);
};
