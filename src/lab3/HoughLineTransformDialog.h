#pragma once
#include <QWidget>


struct HoughLineTransformDialog : QWidget
{
    static HoughLineTransformDialog* create(struct SubWindow* subwindow);
    ~HoughLineTransformDialog() override;

    struct Data;
private:
    std::unique_ptr<Data> m;
    explicit HoughLineTransformDialog(SubWindow* subwindow);
};
