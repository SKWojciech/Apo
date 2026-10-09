#include "BinarizeDialog.h"
#include <QMessageBox>
#include <opencv2/imgproc.hpp>
#include "../SubWindow.h"



struct BinarizeDialog::Data
{
    BinarizeDialog* self;
    SubWindow* subwindow;
    cv::Mat mat;
};


BinarizeDialog* BinarizeDialog::create(SubWindow* subwindow)
{
    if (!subwindow) {
        QMessageBox::warning(nullptr, "Binaryzacja obrazu", "Brak wybranego obrazu");
        return nullptr;
    }
    return new BinarizeDialog(subwindow);
}


static void updatePreview(BinarizeDialog::Data& m, double thresh, bool apply = false)
{
    if (!m.subwindow)
        return;

    cv::Mat dst;
    cv::threshold(m.mat, dst, thresh, 255, cv::THRESH_BINARY);

    if (apply) {
        m.subwindow->setMat(dst, cv::COLOR_GRAY2BGR);
        m.subwindow->removePreview();
        m.subwindow->raise();
        return;
    }

    m.subwindow->setPreview(dst, cv::COLOR_GRAY2BGR);
}


static void updateMat(BinarizeDialog::Data& m)
{
    if (!m.subwindow)
        return;

    m.mat = m.subwindow->getMat();
    if (m.subwindow->getCvtToBgr() != -1)
        cv::cvtColor(m.mat, m.mat, m.subwindow->getCvtToBgr());
    cv::cvtColor(m.mat, m.mat, cv::COLOR_BGR2GRAY);
}


BinarizeDialog::BinarizeDialog(SubWindow* subwindow)
    : m(std::make_unique<Data>())
{
    assert(subwindow != nullptr);
    setAttribute(Qt::WA_DeleteOnClose);
    m->self = this;
    m->subwindow = subwindow;
    setWindowTitle("Podaj wartość progu");
    setLabelText("Próg:");
    setCancelButtonText("Anuluj");
    setOkButtonText("Zastosuj");
    setDoubleDecimals(0);
    setDoubleRange(0, 255);
    setDoubleStep(1);

    updateMat(*m);

    connect(this, &QInputDialog::doubleValueChanged, this, [this] (double value) {
        updatePreview(*m, value);
    });
    connect(m->subwindow, &SubWindow::closing, this, [this] {
        m->subwindow = nullptr;
        done(Rejected);
    });
    connect(m->subwindow, &SubWindow::matChanged, this, [this] {
        updateMat(*m);
        updatePreview(*m, doubleValue());
    });
    connect(this, &QInputDialog::finished, this, [this] (int result) {
        if (result == Rejected) {
            if (m->subwindow)
                m->subwindow->removePreview();
        }
        else {
            updatePreview(*m, doubleValue(), true);
        }
    });

    double default_thresh = cv::threshold(m->mat, m->mat, 0, 255, cv::THRESH_BINARY | cv::THRESH_OTSU | cv::THRESH_DRYRUN);
    setDoubleValue(default_thresh);
    show();
}


BinarizeDialog::~BinarizeDialog()
{
}


