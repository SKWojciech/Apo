#pragma once
#include <QInputDialog>


struct BinarizeDialog : QInputDialog
{
    static BinarizeDialog* create(struct SubWindow* subwindow);
    ~BinarizeDialog() override;

    struct Data;
private:
    BinarizeDialog(SubWindow* subwindow);
    std::unique_ptr<Data> m;
};
