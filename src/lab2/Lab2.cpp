#include "Lab2.h"
#include "../SubWindow.h"
#include "PosterizeDialog.h"
#include "OneStepFilterWindow.h"
#include "FilterDialog.h"
#include "TwoArgOperatorWindow.h"
#include "TwoStepFilterWindow.h"


Lab2::Lab2()
    : QMenu("Lab2")
{
    addAction("Posteryzacja", this, [] {
        PosterizeDialog::create(SubWindowModel::get()->getCurrentSubWindow());
    });
    addAction("Filtry", this, [] {
        FilterDialog::create(SubWindowModel::get()->getCurrentSubWindow());
    });
    addAction("Filtracja jednoetapowa", this, [] {
        OneStepFilterWindow::create(SubWindowModel::get()->getCurrentSubWindow());
    });
    addAction("Filtracja dwuetapowa", this, [] {
        TwoStepFilterWindow::create(SubWindowModel::get()->getCurrentSubWindow());
    });
    addAction("Operacje dwuargumentowe", this, [] {
        TwoArgOperatorWindow::create(SubWindowModel::get()->getCurrentSubWindow());
    });
}
