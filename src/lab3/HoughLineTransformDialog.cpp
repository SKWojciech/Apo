#include "HoughLineTransformDialog.h"
#include <QMessageBox>
#include <QVBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QSlider>
#include <QPushButton>
#include <opencv2/imgproc.hpp>
#include "../SubWindow.h"


struct HoughLineTransformDialog::Data
{
    HoughLineTransformDialog* self;
    SubWindow* subwindow;

    QVBoxLayout layout;
    QGridLayout layout_settings;
    QLabel label_rho;
    QSlider slider_rho;
    QLabel label_theta;
    QSlider slider_theta;
    QLabel label_thresh;
    QSlider slider_thresh;

    QHBoxLayout layout_buttons;
    QPushButton button_apply;
    QPushButton button_cancel;
};


static void updatePreview(HoughLineTransformDialog::Data& m, bool apply = false)
{
    assert(m.subwindow != nullptr);

    cv::Mat mat = m.subwindow->getMat().clone();
    const double rho = m.slider_rho.value();
    const double theta = (m.slider_theta.value() / 100.0) * (CV_PI / 180.0);
    const double thresh = m.slider_thresh.value();

    if (m.subwindow->getCvtToBgr() != -1)
        cv::cvtColor(mat, mat, m.subwindow->getCvtToBgr());
    cv::cvtColor(mat, mat, cv::COLOR_BGR2GRAY);

    // Detekcja linii
    cv::Canny(mat, mat, 50, 200, 3);
    std::vector<cv::Vec2f> lines;
    cv::HoughLines(mat, lines, rho, theta, thresh);

    cv::Mat out_mat = m.subwindow->getMat().clone();
    int out_mat_cvt_to_bgr = m.subwindow->getCvtToBgr();

    // Chcemy kolorowy obraz, aby linie były czerwone
    if (out_mat_cvt_to_bgr == cv::COLOR_GRAY2BGR) {
        cv::cvtColor(out_mat, out_mat, cv::COLOR_GRAY2BGR);
        out_mat_cvt_to_bgr = -1;
    }

    for (const auto& line : lines) {
        float r = line[0], t = line[1];
        cv::Point p1, p2;
        double a = std::cos(t), b = std::sin(t);
        double x0 = a * r, y0 = b * r;
        p1.x = cvRound(x0 + 1000 * -b);
        p1.y = cvRound(y0 + 1000 * a);
        p2.x = cvRound(x0 - 1000 * -b);
        p2.y = cvRound(y0 - 1000 * a);
        cv::line(out_mat, p1, p2, cv::Scalar(0, 0, 255), 3);
    }

    if (apply) {
        m.subwindow->setMat(out_mat, out_mat_cvt_to_bgr);
        m.subwindow->removePreview();
        return;
    }

    m.subwindow->setPreview(out_mat, out_mat_cvt_to_bgr);
}


HoughLineTransformDialog* HoughLineTransformDialog::create(SubWindow* subwindow)
{
    if (subwindow == nullptr) {
        QMessageBox::warning(nullptr, "Detekcja linii", "Brak wybranego obrazu");
        return nullptr;
    }
    return new HoughLineTransformDialog(subwindow);
}


HoughLineTransformDialog::HoughLineTransformDialog(SubWindow* subwindow)
    : m(std::make_unique<Data>())
{
    assert(subwindow != nullptr);
    setAttribute(Qt::WA_DeleteOnClose);
    setWindowTitle("Detekcja linii");
    m->self = this;
    m->subwindow = subwindow;

    {
        m->label_rho.setText("Rho");
        m->layout_settings.addWidget(&m->label_rho, 0, 0, Qt::AlignRight);

        m->slider_rho.setOrientation(Qt::Horizontal);
        m->slider_rho.setRange(1, 100);
        m->slider_rho.setValue(1);
        m->layout_settings.addWidget(&m->slider_rho, 0, 1, 2, 1);

        m->label_theta.setText("Theta");
        m->layout_settings.addWidget(&m->label_theta, 1, 0, Qt::AlignRight);

        m->slider_theta.setOrientation(Qt::Horizontal);
        m->slider_theta.setRange(1, 100);
        m->slider_theta.setValue(100);
        m->layout_settings.addWidget(&m->slider_theta, 1, 1, 2, 1);

        m->label_thresh.setText("Próg");
        m->layout_settings.addWidget(&m->label_thresh, 2, 0, Qt::AlignRight);

        m->slider_thresh.setOrientation(Qt::Horizontal);
        m->slider_thresh.setRange(50, 1000);
        m->slider_thresh.setValue(150);
        m->layout_settings.addWidget(&m->slider_thresh, 2, 1, 2, 1);
    }
    m->layout.addLayout(&m->layout_settings);

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
    connect(&m->slider_rho, &QSlider::valueChanged, this, [this] {
        updatePreview(*m);
    });
    connect(&m->slider_theta, &QSlider::valueChanged, this, [this] {
        updatePreview(*m);
    });
    connect(&m->slider_thresh, &QSlider::valueChanged, this, [this] {
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


HoughLineTransformDialog::~HoughLineTransformDialog()
{
    if (m->subwindow)
        m->subwindow->removePreview();
}
