#pragma once
#include <QWidget>


struct ObjectCharacteristicsWindow : QWidget
{
    static ObjectCharacteristicsWindow* create(struct SubWindow* subwindow);
    ~ObjectCharacteristicsWindow() override;

    struct Data;
private:
    std::unique_ptr<Data> m;
    explicit ObjectCharacteristicsWindow(SubWindow* init_subwindow);
};