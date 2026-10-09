#pragma once
#include <QWidget>


struct EqualizeDialog : QWidget
{
    static EqualizeDialog* create(struct SubWindow* subwindow);
    ~EqualizeDialog() override;

    struct Data;
private:
    explicit EqualizeDialog(SubWindow* subwindow);
    std::unique_ptr<Data> m;
};
