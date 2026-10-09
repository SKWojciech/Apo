#pragma once
#include <QWidget>


struct LineProfileWindow : QWidget
{
    static LineProfileWindow* create(struct SubWindow* subwindow);
    ~LineProfileWindow() override;

    struct Data;
private:
    std::unique_ptr<Data> m;
    explicit LineProfileWindow(SubWindow* subwindow);

protected:
    void resizeEvent(QResizeEvent* event) override;
};
