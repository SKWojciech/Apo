#pragma once
#include <QWidget>


struct HistogramStretchWindow : QWidget
{
    static HistogramStretchWindow* create(struct SubWindow* subwindow);
    ~HistogramStretchWindow() override;


    struct Data;
private:
    explicit HistogramStretchWindow(SubWindow* subwindow);
    std::unique_ptr<Data> m;

protected:
    void resizeEvent(QResizeEvent* event) override;
    void showEvent(QShowEvent* event) override;
    void closeEvent(QCloseEvent* event) override;
};
