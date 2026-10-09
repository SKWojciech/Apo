#pragma once
#include <QWidget>


struct OneStepFilterWindow : QWidget
{
    static OneStepFilterWindow* create(struct SubWindow* subwindow);
    ~OneStepFilterWindow() override;

    struct Data;
private:
    std::unique_ptr<Data> m;
    explicit OneStepFilterWindow(SubWindow* subwindow);
};
