#pragma once
#include <QWidget>


struct PyramidDialog : QWidget
{
    static PyramidDialog* create(struct SubWindow* subwindow);
    ~PyramidDialog() override;

    struct Data;
private:
    std::unique_ptr<Data> m;
    explicit PyramidDialog(SubWindow* subwindow);
};
