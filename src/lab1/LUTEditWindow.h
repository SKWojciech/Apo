#pragma once
#include <QWidget>


struct LUTEditWindow : QWidget
{
    static LUTEditWindow* create(struct SubWindow* subwindow);
    ~LUTEditWindow() override;


    struct Data;
private:
    std::unique_ptr<Data> m;
    explicit LUTEditWindow(SubWindow* subwindow);

protected:
    void closeEvent(QCloseEvent* event) override;
};
