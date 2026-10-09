#include "MorphologicalOperationsWindow.h"
#include <QMessageBox>
#include <QVBoxLayout>
#include <QComboBox>
#include <QPushButton>
#include <QLabel>
#include <QSlider>
#include <opencv2/imgproc.hpp>
#include "../SubWindow.h"


static constexpr std::array g_sizes = {
    3, 5, 7
};
static constexpr std::array g_size_labels = {
    "3x3", "5x5", "7x7"
};

enum MorphOperation
{
    OPERATION_ERODE,
    OPERATION_DILATE,
    OPERATION_OPEN,
    OPERATION_CLOSE,
    OPERATION_SKELETONIZE
};
static constexpr std::array g_operation_labels = {
    "Erozja", "Dylacja", "Otwarcie", "Zamknięcie", "Szkieletyzacja"
};

enum MorphShape
{
    SHAPE_ROMB = cv::MORPH_DIAMOND,
    SHAPE_SQUARE = cv::MORPH_RECT,
    SHAPE_CROSS = cv::MORPH_CROSS
};
static constexpr std::array g_shape_labels = {
    "Romb", "Kwadrat", "Krzyż"
};

static constexpr std::array g_borders = {
    cv::BORDER_ISOLATED, cv::BORDER_REFLECT, cv::BORDER_REPLICATE
};
static constexpr std::array g_border_labels = {
    "Bez zmian (Isolated)", "Odbicie lustrzane (Reflect)", "Powtórz wartość (Replicate)"
};


struct MorphologicalOperationsWindow::Data
{
    MorphologicalOperationsWindow* self;
    SubWindow* subwindow;

    QVBoxLayout layout;
    QComboBox combo_subwindow;
    QComboBox combo_shape;
    QComboBox combo_border;
    QComboBox combo_size;
    QComboBox combo_operation;
    QHBoxLayout layout_iterations;
    QLabel label_iterations;
    QSlider slider_iterations;

    QHBoxLayout layout_buttons;
    QPushButton button_apply;
    QPushButton button_cancel;
};


static void updatePreview(MorphologicalOperationsWindow::Data& m, bool apply = false)
{
    if (m.subwindow == nullptr)
        return;

    cv::Mat mat = m.subwindow->getMat().clone();
    const int cvt_to_bgr = m.subwindow->getCvtToBgr();
    const auto operation = (MorphOperation)m.combo_operation.currentIndex();
    const auto shape = (MorphShape)m.combo_shape.currentIndex();
    const cv::BorderTypes border = g_borders[m.combo_border.currentIndex()];
    const int size = g_sizes[m.combo_size.currentIndex()];
    const int iterations = m.slider_iterations.value();

    cv::Mat kernel = cv::getStructuringElement(shape, cv::Size(size, size));

    switch (operation) {
    case OPERATION_ERODE:
        cv::erode(mat, mat, kernel, cv::Point(-1, -1), iterations, border);
        break;
    case OPERATION_DILATE:
        cv::dilate(mat, mat, kernel, cv::Point(-1, -1), iterations, border);
        break;
    case OPERATION_OPEN:
        cv::morphologyEx(mat, mat, cv::MORPH_OPEN, kernel, cv::Point(-1, -1), iterations, border);
        break;
    case OPERATION_CLOSE:
        cv::morphologyEx(mat, mat, cv::MORPH_CLOSE, kernel, cv::Point(-1, -1), iterations, border);
        break;
    case OPERATION_SKELETONIZE: {
        if (cvt_to_bgr != cv::COLOR_GRAY2BGR) {
            QMessageBox::warning(nullptr, "Operacje morfologiczne", "Obraz musi być binarny dla szkieletyzacji");
            return;
        }
        assert(mat.depth() == CV_8U);
        std::atomic<bool> is_binary = true;
        mat.forEach<uint8_t>([&](uint8_t& value, const int*) {
            is_binary = is_binary & (value == 0 || value == 255);
        });
        if (!is_binary) {
            QMessageBox::warning(nullptr, "Operacje morfologiczne", "Obraz musi być binarny dla szkieletyzacji");
            return;
        }

        cv::Mat skel(mat.size(), mat.type(), cv::Scalar(0));
        cv::Mat temp(mat.size(), mat.type());
        int iter = 0;
        do {
            cv::morphologyEx(mat, temp, cv::MORPH_OPEN, kernel);
            cv::bitwise_not(temp, temp);
            cv::bitwise_and(mat, temp, temp);
            cv::bitwise_or(skel, temp, skel);
            cv::erode(mat, mat, kernel);
        } while (cv::countNonZero(mat) > 0 && iter++ < 1000);

        mat = skel;
        break;
    }
    }

    if (apply) {
        m.subwindow->setMat(mat, cvt_to_bgr);
        m.subwindow->removePreview();
        return;
    }

    m.subwindow->setPreview(mat, cvt_to_bgr);
}


static void setSubwindow(MorphologicalOperationsWindow::Data& m, SubWindow* subwindow)
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


MorphologicalOperationsWindow* MorphologicalOperationsWindow::create(SubWindow* subwindow)
{
    if (subwindow == nullptr) {
        QMessageBox::warning(nullptr, "Operacje morfologiczne", "Brak wybranego obrazu");
        return nullptr;
    }
    return new MorphologicalOperationsWindow(subwindow);
}


MorphologicalOperationsWindow::MorphologicalOperationsWindow(SubWindow* subwindow)
    : m(std::make_unique<Data>())
{
    assert(subwindow != nullptr);
    setAttribute(Qt::WA_DeleteOnClose);
    setWindowTitle("Operacje morfologiczne");
    m->self = this;

    m->combo_subwindow.setModel(SubWindowModel::get());
    m->combo_subwindow.setCurrentIndex(SubWindowModel::get()->getIndexOfSubWindow(subwindow));
    m->layout.addWidget(&m->combo_subwindow);

    m->combo_shape.addItems(QStringList(g_shape_labels.begin(), g_shape_labels.end()));
    m->layout.addWidget(&m->combo_shape);

    m->combo_border.addItems(QStringList(g_border_labels.begin(), g_border_labels.end()));
    m->combo_border.setCurrentIndex(1);
    m->layout.addWidget(&m->combo_border);

    m->combo_size.addItems(QStringList(g_size_labels.begin(), g_size_labels.end()));
    m->layout.addWidget(&m->combo_size);

    m->combo_operation.addItems(QStringList(g_operation_labels.begin(), g_operation_labels.end()));
    m->layout.addWidget(&m->combo_operation);

    {
        m->label_iterations.setText("Iteracje: ");
        m->layout_iterations.addWidget(&m->label_iterations, 0, Qt::AlignRight);

        m->slider_iterations.setOrientation(Qt::Horizontal);
        m->slider_iterations.setRange(1, 100);
        m->layout_iterations.addWidget(&m->slider_iterations, 1);
    }
    m->layout.addLayout(&m->layout_iterations);

    {
        m->layout_buttons.addStretch(1);

        m->button_apply.setText("Zastosuj");
        m->layout_buttons.addWidget(&m->button_apply);

        m->button_cancel.setText("Anuluj");
        m->layout_buttons.addWidget(&m->button_cancel);
    }
    m->layout.addLayout(&m->layout_buttons);

    setLayout(&m->layout);

    connect(&m->combo_subwindow, &QComboBox::currentIndexChanged, this, [this] (int idx) {
        if (idx != -1)
            setSubwindow(*m, SubWindowModel::get()->getSubWindowAt(idx));
        else
            setSubwindow(*m, nullptr);
    });
    connect(&m->combo_shape, &QComboBox::currentIndexChanged, this, [this] {
        updatePreview(*m);
    });
    connect(&m->combo_border, &QComboBox::currentIndexChanged, this, [this] {
        updatePreview(*m);
    });
    connect(&m->combo_size, &QComboBox::currentIndexChanged, this, [this] {
        updatePreview(*m);
    });
    connect(&m->combo_operation, &QComboBox::currentIndexChanged, this, [this] {
        updatePreview(*m);
    });
    connect(&m->slider_iterations, &QSlider::valueChanged, this, [this] {
        updatePreview(*m);
    });
    connect(&m->button_apply, &QPushButton::clicked, this, [this] {
        updatePreview(*m, true);
        close();
    });
    connect(&m->button_cancel, &QPushButton::clicked, this, [this] {
        close();
    });


    setSubwindow(*m, subwindow);
    show();
}


MorphologicalOperationsWindow::~MorphologicalOperationsWindow()
{
    if (m->subwindow)
        m->subwindow->removePreview();
}
