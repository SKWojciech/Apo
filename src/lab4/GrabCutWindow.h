#pragma once
#include <QWidget>


struct GrabCutWindow : QWidget
{
    static GrabCutWindow* create(struct SubWindow* subwindow);
    ~GrabCutWindow() override;

    struct Data;
    struct Scene;
private:
    std::unique_ptr<Data> m;
    explicit GrabCutWindow(SubWindow* subwindow);
};
