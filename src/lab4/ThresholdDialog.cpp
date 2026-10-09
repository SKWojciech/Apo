#include "ThresholdDialog.h"

#include "../SubWindow.h"

#include <QVBoxLayout>
#include <QPushButton>
#include <QSlider>
#include <QMessageBox>

#include <opencv2/imgproc.hpp>


enum class ThresholdMethod
{
    MANUAL, OTSU, ADAPT_MEAN, ADAPT_GAUSS
};


struct ThresholdDialog::Data
{
    ThresholdDialog* self;
    SubWindow* subwindow;
    ThresholdMethod method;

    QVBoxLayout layout;
    QSlider slider_thresh;
    QPushButton button_otsu;
    QPushButton button_adapt_mean;
    QPushButton button_adapt_gauss;

    QHBoxLayout layout_buttons;
    QPushButton button_apply;
    QPushButton button_cancel;
};


static void updatePreview(ThresholdDialog::Data& m, bool apply = false)
{
    assert(m.subwindow != nullptr);

    cv::Mat mat = m.subwindow->getMat().clone();
    int cvt_to_bgr = m.subwindow->getCvtToBgr();

    if (cvt_to_bgr != cv::COLOR_BGR2GRAY) {
        if (cvt_to_bgr != -1)
            cv::cvtColor(mat, mat, cvt_to_bgr);
        cv::cvtColor(mat, mat, cv::COLOR_BGR2GRAY);
        cvt_to_bgr = cv::COLOR_GRAY2BGR;
    }

    switch (m.method) {
        using enum ThresholdMethod;
    case MANUAL:
        cv::threshold(mat, mat, m.slider_thresh.value(), 255, cv::THRESH_BINARY);
        break;
    case OTSU: {
        const double t = cv::threshold(mat, mat, 0, 255, cv::THRESH_BINARY | cv::THRESH_OTSU);
        m.slider_thresh.setValue(t);
        break;
    }
    case ADAPT_MEAN:
        cv::adaptiveThreshold(mat, mat, 255, cv::ADAPTIVE_THRESH_MEAN_C, cv::THRESH_BINARY, 11, 2);
        break;
    case ADAPT_GAUSS:
        cv::adaptiveThreshold(mat, mat, 255, cv::ADAPTIVE_THRESH_GAUSSIAN_C, cv::THRESH_BINARY, 11, 2);
        break;
    }

    if (apply) {
        m.subwindow->setMat(mat, cvt_to_bgr);
        m.subwindow->removePreview();
        return;
    }

    m.subwindow->setPreview(mat, cvt_to_bgr);
}


ThresholdDialog* ThresholdDialog::create(SubWindow* subwindow)
{
    if (subwindow == nullptr) {
        QMessageBox::warning(nullptr, "Progowanie", "Brak wybranego obrazu");
        return nullptr;
    }
    return new ThresholdDialog(subwindow);
}


ThresholdDialog::ThresholdDialog(SubWindow* subwindow)
    : m(std::make_unique<Data>())
{
    assert(subwindow != nullptr);
    setWindowTitle("Progowanie");
    setAttribute(Qt::WA_DeleteOnClose);
    m->self = this;
    m->subwindow = subwindow;

    m->slider_thresh.setOrientation(Qt::Horizontal);
    m->slider_thresh.setRange(0, 255);
    m->slider_thresh.setValue(128);
    m->layout.addWidget(&m->slider_thresh);

    m->button_otsu.setText("Metoda Otsu");
    m->layout.addWidget(&m->button_otsu);

    m->button_adapt_mean.setText("Metoda adaptacyjna (Mediana)");
    m->layout.addWidget(&m->button_adapt_mean);

    m->button_adapt_gauss.setText("Metoda adaptacyjna (Gauss)");
    m->layout.addWidget(&m->button_adapt_gauss);

    {
        m->layout_buttons.addStretch(1);

        m->button_apply.setText("Zastosuj");
        m->layout_buttons.addWidget(&m->button_apply);

        m->button_cancel.setText("Anuluj");
        m->layout_buttons.addWidget(&m->button_cancel);
    }
    m->layout.addLayout(&m->layout_buttons);

    setLayout(&m->layout);

    connect(subwindow, &SubWindow::matChanged, this, [this] {
        updatePreview(*m);
    });
    connect(subwindow, &SubWindow::closing, this, [this] {
        m->subwindow = nullptr;
        close();
    });
    connect(&m->slider_thresh, &QSlider::valueChanged, this, [this] {
        m->method = ThresholdMethod::MANUAL;
        updatePreview(*m);
    });
    connect(&m->button_otsu, &QPushButton::clicked, this, [this] {
        m->method = ThresholdMethod::OTSU;
        updatePreview(*m);
    });
    connect(&m->button_adapt_mean, &QPushButton::clicked, this, [this] {
        m->method = ThresholdMethod::ADAPT_MEAN;
        updatePreview(*m);
    });
    connect(&m->button_adapt_gauss, &QPushButton::clicked, this, [this] {
        m->method = ThresholdMethod::ADAPT_GAUSS;
        updatePreview(*m);
    });
    connect(&m->button_apply, &QPushButton::clicked, this, [this] {
        updatePreview(*m, true);
        m->subwindow = nullptr;
        close();
    });
    connect(&m->button_cancel, &QPushButton::clicked, this, [this] {
        close();
    });

    updatePreview(*m);
    show();
}


ThresholdDialog::~ThresholdDialog()
{
    if (m->subwindow != nullptr)
        m->subwindow->removePreview();
}
