#include "Lab3.h"
#include "../SubWindow.h"
#include "MorphologicalOperationsWindow.h"
#include "HoughLineTransformDialog.h"
#include "PyramidDialog.h"
#include "LineProfileWindow.h"


Lab3::Lab3()
    : QMenu("Lab3")
{
    addAction("Operacje morfologiczne", this, [] {
        MorphologicalOperationsWindow::create(SubWindowModel::get()->getCurrentSubWindow());
    });

    addAction("Detekcja linii", this, [] {
        HoughLineTransformDialog::create(SubWindowModel::get()->getCurrentSubWindow());
    });

    addAction("Piramida obrazów", this, [] {
        PyramidDialog::create(SubWindowModel::get()->getCurrentSubWindow());
    });

    addAction("Linia profilu", this, [] {
        LineProfileWindow::create(SubWindowModel::get()->getCurrentSubWindow());
    });
}
