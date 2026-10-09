#include "PyramidDialog.h"
#include <QMessageBox>
#include <QVBoxLayout>
#include <QSpinBox>
#include <QPushButton>
#include <opencv2/imgproc.hpp>
#include "../SubWindow.h"



struct PyramidDialog::Data
{
    PyramidDialog* self;
    SubWindow* subwindow;

    QVBoxLayout layout;
    QSpinBox spinbox;

    QHBoxLayout layout_buttons;
    QPushButton button_apply;
    QPushButton button_cancel;
};


static void updatePreview(PyramidDialog::Data& m, bool apply = false)
{
    assert(m.subwindow != nullptr);

    const int steps = m.spinbox.value();
    if (steps == 0)
        return;

    cv::Mat mat = m.subwindow->getMat().clone();
    const int cvt_to_bgr = m.subwindow->getCvtToBgr();

    for (int i = 0; i < steps; i++) {
        cv::pyrUp(mat, mat);
    }
    for (int i = steps; i < 0; i++) {
        cv::pyrDown(mat, mat);
    }

    if (apply) {
        m.subwindow->setMat(mat, cvt_to_bgr);
        m.subwindow->removePreview();
        return;
    }

    m.subwindow->setPreview(mat, cvt_to_bgr);
}


PyramidDialog* PyramidDialog::create(SubWindow* subwindow)
{
    if (subwindow == nullptr) {
        QMessageBox::warning(nullptr, "Piramida obrazów", "Brak wybranego obrazu");
        return nullptr;
    }
    return new PyramidDialog(subwindow);
}


PyramidDialog::PyramidDialog(SubWindow* subwindow)
    : m(std::make_unique<Data>())
{
    assert(subwindow != nullptr);
    setAttribute(Qt::WA_DeleteOnClose);
    setWindowTitle("Piramida obrazów");
    m->self = this;
    m->subwindow = subwindow;

    m->spinbox.setRange(-2, 2);
    m->layout.addWidget(&m->spinbox);

    {
        m->layout_buttons.addStretch(1);

        m->button_apply.setText("Zastosuj");
        m->layout_buttons.addWidget(&m->button_apply);

        m->button_cancel.setText("Anuluj");
        m->layout_buttons.addWidget(&m->button_cancel);
    }
    m->layout.addLayout(&m->layout_buttons);

    setLayout(&m->layout);

    connect(m->subwindow, &SubWindow::closing, this, [this] {
        m->subwindow = nullptr;
        close();
    });
    connect(m->subwindow, &SubWindow::matChanged, this, [this] {
        updatePreview(*m);
    });
    connect(&m->spinbox, &QSpinBox::valueChanged, this, [this] {
        updatePreview(*m);
    });
    connect(&m->button_apply, &QPushButton::clicked, this, [this] {
        updatePreview(*m, true);
        close();
    });
    connect(&m->button_cancel, &QPushButton::clicked, this, [this] {
        close();
    });

    updatePreview(*m);
    show();
}


PyramidDialog::~PyramidDialog()
{
    if (m->subwindow)
        m->subwindow->removePreview();
}
