#include "TwoStepFilterWindow.h"
#include <QMessageBox>
#include <QVBoxLayout>
#include <QGridLayout>
#include <QComboBox>
#include <QSpinBox>
#include <QPushButton>
#include <QLabel>
#include <QInputDialog>
#include <QFileDialog>
#include <opencv2/imgproc.hpp>
#include "../SubWindow.h"



static constexpr std::array g_borders = {
    cv::BORDER_ISOLATED, cv::BORDER_REFLECT, cv::BORDER_REPLICATE
};
static constexpr std::array g_border_labels = {
    "Bez zmian (Isolated)", "Odbicie lustrzane (Reflect)", "Powtórz wartość (Replicate)"
};


struct TwoStepFilterWindow::Data
{
    TwoStepFilterWindow* self;
    SubWindow* subwindow;

    QVBoxLayout layout;
    QComboBox combo_subwindow;
    QHBoxLayout layout_settings;
    QLabel label_combo_border;
    QComboBox combo_border;

    QGridLayout layout_masks;
    QGridLayout layout_grid_a;
    std::array<QDoubleSpinBox, 3*3> spinboxes_a;
    QGridLayout layout_grid_b;
    std::array<QDoubleSpinBox, 3*3> spinboxes_b;
    QGridLayout layout_grid_out;
    std::array<QDoubleSpinBox, 5*5> spinboxes_out;

    QHBoxLayout layout_h_buttons;
    QPushButton button_apply;
    QPushButton button_cancel;
};


static void setDefaultMask(TwoStepFilterWindow::Data& m)
{
    for (int row = 0; row < 3; row++)
        for (int col = 0; col < 3; col++)
            m.spinboxes_a[row*3 + col].setValue(row == col && row == 1 ? 1.0 : 0.0);
    for (int row = 0; row < 3; row++)
        for (int col = 0; col < 3; col++)
            m.spinboxes_b[row*3 + col].setValue(row == col && row == 1 ? 1.0 : 0.0);
}


static void updatePreview(TwoStepFilterWindow::Data& m, bool apply = false)
{
    if (m.subwindow == nullptr)
        return;

    cv::Mat mat = m.subwindow->getMat().clone();
    const int cvt_to_bgr = m.subwindow->getCvtToBgr();
    const cv::BorderTypes border = g_borders[m.combo_border.currentIndex()];

    cv::Mat kernel(5, 5, CV_64F);
    for (int row = 0; row < 5; row++)
        for (int col = 0; col < 5; col++)
            kernel.at<double>(row, col) = m.spinboxes_out[row*5 + col].value();

    cv::filter2D(mat, mat, mat.depth(), kernel, cv::Point(-1, -1), 0, border);

    if (apply) {
        m.subwindow->setMat(mat, cvt_to_bgr);
        m.subwindow->removePreview();
        return;
    }

    m.subwindow->setPreview(mat, cvt_to_bgr);
}


static void updateMasks(TwoStepFilterWindow::Data& m)
{
    cv::Mat a(3, 3, CV_64F);
    double sum = 0;
    for (int row = 0; row < 3; row++)
        for (int col = 0; col < 3; col++)
            sum += a.at<double>(row, col) = m.spinboxes_a[row*3 + col].value();

    if (sum != 0)
        a /= sum;

    cv::Mat b(3, 3, CV_64F);
    sum = 0;
    for (int row = 0; row < 3; row++)
        for (int col = 0; col < 3; col++)
            sum += b.at<double>(row, col) = m.spinboxes_b[row*3 + col].value();

    if (sum != 0)
        b /= sum;

    cv::flip(b, b, -1);

    cv::Mat out = cv::Mat::zeros(5, 5, CV_64F);
    sum = 0;
    for (int row = 0; row < 3; row++)
        for (int col = 0; col < 3; col++)
            out.at<double>(row + 1, col + 1) = a.at<double>(row, col);
    cv::filter2D(out, out, CV_64F, b, cv::Point(-1, -1), 0, cv::BORDER_CONSTANT);

    if (sum != 0)
        out /= sum;

    for (int row = 0; row < 5; row++)
        for (int col = 0; col < 5; col++)
            m.spinboxes_out[row*5 + col].setValue(out.at<double>(row, col));

    updatePreview(m);
}


static void setSubwindow(TwoStepFilterWindow::Data& m, SubWindow* subwindow)
{
    SubWindow* old_subwindow = m.subwindow;
    m.subwindow = subwindow;

    if (old_subwindow != nullptr && m.subwindow != old_subwindow)
        QWidget::disconnect(old_subwindow, nullptr, m.self, nullptr);

    if (m.subwindow == nullptr)
        return;

    if (m.subwindow != old_subwindow) {
        QWidget::connect(m.subwindow, &SubWindow::closing, m.self, [&m] {
            m.subwindow = nullptr;
            m.self->close();
        });
        QWidget::connect(m.subwindow, &SubWindow::matChanged, m.self, [&m] {
            updatePreview(m);
        });
    }

    updatePreview(m);
}


TwoStepFilterWindow* TwoStepFilterWindow::create(SubWindow* subwindow)
{
    if (subwindow == nullptr) {
        QMessageBox::warning(nullptr, "Filtracja dwuetapowa", "Brak wybranego obrazu");
        return nullptr;
    }
    return new TwoStepFilterWindow(subwindow);
}


TwoStepFilterWindow::TwoStepFilterWindow(SubWindow* subwindow)
    : m(std::make_unique<Data>())
{
    assert(subwindow != nullptr);
    setAttribute(Qt::WA_DeleteOnClose);
    setWindowTitle("Filtracja dwuetapowa");
    m->self = this;

    m->combo_subwindow.setModel(SubWindowModel::get());
    m->combo_subwindow.setCurrentIndex(SubWindowModel::get()->getIndexOfSubWindow(subwindow));
    m->layout.addWidget(&m->combo_subwindow);

    {
        m->label_combo_border.setText("Wartości brzegowe: ");
        m->layout_settings.addWidget(&m->label_combo_border, 0, Qt::AlignRight);

        m->combo_border.addItems(QStringList(g_border_labels.begin(), g_border_labels.end()));
        m->combo_border.setCurrentIndex(1);
        m->layout_settings.addWidget(&m->combo_border, 1);
    }
    m->layout.addLayout(&m->layout_settings);


    {
        m->layout_masks.setVerticalSpacing(5);
        for (int row = 0; row < 3; row++) {
            for (int col = 0; col < 3; col++) {
                auto& spinbox = m->spinboxes_a[row*3 + col];
                spinbox.setRange(-99, 99);
                spinbox.setDecimals(3);
                m->layout_grid_a.addWidget(&spinbox, row, col, Qt::AlignVCenter);
            }
        }
        m->layout_masks.addLayout(&m->layout_grid_a, 0, 0);
        for (int row = 0; row < 3; row++) {
            for (int col = 0; col < 3; col++) {
                auto& spinbox = m->spinboxes_b[row*3 + col];
                spinbox.setRange(-99, 99);
                spinbox.setDecimals(3);
                m->layout_grid_b.addWidget(&spinbox, row, col, Qt::AlignVCenter);
            }
        }
        m->layout_masks.addLayout(&m->layout_grid_b, 1, 0);
        for (int row = 0; row < 5; row++) {
            for (int col = 0; col < 5; col++) {
                auto& spinbox = m->spinboxes_out[row*5 + col];
                spinbox.setRange(-99, 99);
                spinbox.setEnabled(false);
                spinbox.setAlignment(Qt::AlignCenter);
                spinbox.setButtonSymbols(QAbstractSpinBox::NoButtons);
                spinbox.setDecimals(3);
                m->layout_grid_out.addWidget(&spinbox, row, col, Qt::AlignVCenter);
            }
        }
        m->layout_masks.addLayout(&m->layout_grid_out, 0, 1, 2, 1);
        setDefaultMask(*m);
    }
    m->layout.addStretch(1);
    m->layout.addLayout(&m->layout_masks, 0);
    m->layout.addStretch(1);

    {
        m->layout_h_buttons.addStretch(1);

        m->button_apply.setText("Zastosuj");
        m->layout_h_buttons.addWidget(&m->button_apply);

        m->button_cancel.setText("Anuluj");
        m->layout_h_buttons.addWidget(&m->button_cancel);
    }
    m->layout.addLayout(&m->layout_h_buttons);

    setLayout(&m->layout);

    connect(&m->combo_subwindow, &QComboBox::currentIndexChanged, this, [this] (int idx) {
        if (idx != -1)
            setSubwindow(*m, SubWindowModel::get()->getSubWindowAt(idx));
        else
            setSubwindow(*m, nullptr);
    });
    connect(&m->combo_border, &QComboBox::currentIndexChanged, this, [this] {
        updatePreview(*m);
    });
    connect(&m->button_apply, &QPushButton::clicked, this, [this] {
        updatePreview(*m, true);
        close();
    });
    connect(&m->button_cancel, &QPushButton::clicked, this, [this] {
        close();
    });
    for (auto& spinbox : m->spinboxes_a)
        connect(&spinbox, &QDoubleSpinBox::valueChanged, this, [this] { updateMasks(*m); });
    for (auto& spinbox : m->spinboxes_b)
        connect(&spinbox, &QDoubleSpinBox::valueChanged, this, [this] { updateMasks(*m); });

    setSubwindow(*m, subwindow);
    updateMasks(*m);
    show();
}


TwoStepFilterWindow::~TwoStepFilterWindow()
{
    if (m->subwindow)
        m->subwindow->removePreview();
}
