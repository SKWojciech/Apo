#pragma once
#include <QWidget>


struct FilterDialog : QWidget
{
    static FilterDialog* create(struct SubWindow* subwindow);
    ~FilterDialog() override;

    struct Data;
private:
    std::unique_ptr<Data> m;
    explicit FilterDialog(SubWindow* subwindow);
};
