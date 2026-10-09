#pragma once
#include <QWidget>


struct TwoStepFilterWindow : QWidget
{
    static TwoStepFilterWindow* create(struct SubWindow* subwindow);
    ~TwoStepFilterWindow() override;

    struct Data;
private:
    std::unique_ptr<Data> m;
    explicit TwoStepFilterWindow(SubWindow* subwindow);
};
