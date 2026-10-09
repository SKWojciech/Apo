#pragma once
#include <QWidget>


struct ThresholdDialog : QWidget
{
    static ThresholdDialog* create(struct SubWindow* subwindow);
    ~ThresholdDialog() override;

    struct Data;
private:
    explicit ThresholdDialog(SubWindow* subwindow);
    std::unique_ptr<Data> m;
};
