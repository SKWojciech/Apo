#pragma once
#include <QWidget>


struct InpaintWindow : QWidget
{
    static InpaintWindow* create(struct SubWindow* subwindow);
    ~InpaintWindow() override;

    struct Data;
    struct Scene;
private:
    std::unique_ptr<Data> m;
    explicit InpaintWindow(SubWindow* subwindow);
};
