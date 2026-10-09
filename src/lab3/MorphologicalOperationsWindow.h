#pragma once
#include <QWidget>


struct MorphologicalOperationsWindow : QWidget
{
    static MorphologicalOperationsWindow* create(struct SubWindow* subwindow);
    ~MorphologicalOperationsWindow() override;

    struct Data;
private:
    std::unique_ptr<Data> m;
    explicit MorphologicalOperationsWindow(SubWindow* subwindow);
};
