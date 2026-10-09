#pragma once
#include <QWidget>


struct HistogramWindow : QWidget
{
    static HistogramWindow* create(struct SubWindow* init_subwindow = nullptr);
    ~HistogramWindow() override;

    struct Data;
private:
    explicit HistogramWindow(SubWindow* init_subwindow);
    std::unique_ptr<Data> m;
};
