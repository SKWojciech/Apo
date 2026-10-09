#pragma once
#include <QWidget>


struct HistogramTableWindow : QWidget
{
    static HistogramTableWindow* create(struct SubWindow* init_subwindow = nullptr);
    ~HistogramTableWindow() override;

    struct Data;
private:
    explicit HistogramTableWindow(SubWindow* init_subwindow);
    std::unique_ptr<Data> m;
};
