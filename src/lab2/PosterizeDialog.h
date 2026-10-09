#pragma once
#include <QWidget>


struct PosterizeDialog : QWidget
{
    static PosterizeDialog* create(struct SubWindow* subwindow);
    ~PosterizeDialog() override;

    struct Data;
private:
    explicit PosterizeDialog(SubWindow* subwindow);
    std::unique_ptr<Data> m;
};
