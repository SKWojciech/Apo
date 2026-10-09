#include "FilterDialog.h"
#include <QMessageBox>
#include <QVBoxLayout>
#include <QComboBox>
#include <QPushButton>
#include <QGridLayout>
#include <QLabel>
#include <QSlider>
#include <opencv2/imgproc.hpp>
#include "../SubWindow.h"


static constexpr int MAX_MASK_LENGTH = 7;
static constexpr std::array g_mask_lengths = {3, 5, 7};
static constexpr std::array g_mask_size_labels = {"3x3", "5x5", "7x7"};
enum FilterType : int
{
    FILTER_BOX_BLUR,
    FILTER_GAUSS_BLUR,
    FILTER_MEDIAN,
    FILTER_SOBEL,
    FILTER_LAPLACIAN,
    FILTER_CANNY
};
static constexpr std::array g_filter_labels = {
    "Wygładzanie liniowe", "Wygładzanie Gaussa", "Filtracja medianowa", "Operator Sobela", "Operator Laplaciana", "Operator Canny'ego"
};
static constexpr std::array g_borders = {
    cv::BORDER_ISOLATED, cv::BORDER_REFLECT, cv::BORDER_REPLICATE
};
static constexpr std::array g_border_labels = {
    "Bez zmian (Isolated)", "Odbicie lustrzane (Reflect)", "Powtórz wartość (Replicate)"
};


struct FilterDialog::Data
{
    FilterDialog* self;
    SubWindow* subwindow;

    QVBoxLayout layout;
    QComboBox combo_border;
    QComboBox combo_filter;
    QGridLayout layout_grid;
    QLabel label_combo_mask_size;
    QComboBox combo_mask_size;
    QLabel label_a;
    QSlider slider_a;
    QLabel label_b;
    QSlider slider_b;
    QHBoxLayout layout_buttons;
    QPushButton button_apply;
    QPushButton button_cancel;
};


static void setSliders(FilterDialog::Data& m, bool enable_a, const char* text_a, bool enable_b, const char* text_b)
{
    m.label_a.setVisible(enable_a);
    m.slider_a.setVisible(enable_a);
    if (enable_a)
        m.label_a.setText(text_a);
    m.label_b.setVisible(enable_b);
    m.slider_b.setVisible(enable_b);
    if (enable_b)
        m.label_b.setText(text_b);
}


static void updatePreview(FilterDialog::Data& m, bool apply = false)
{
    if (m.subwindow == nullptr)
        return;

    const FilterType selection = (FilterType)m.combo_filter.currentIndex();
    const cv::BorderTypes border = g_borders[m.combo_border.currentIndex()];
    const int ksize = g_mask_lengths[m.combo_mask_size.currentIndex()];
    const int value_a = m.slider_a.value();
    const int value_b = m.slider_b.value();
    cv::Mat mat = m.subwindow->getMat().clone();
    assert(mat.depth() == CV_8U);
    int cvt_to_bgr = m.subwindow->getCvtToBgr();

    switch (selection) {
    case FILTER_BOX_BLUR: {
        setSliders(m, false, nullptr, false, nullptr);
        cv::blur(mat, mat, cv::Size(ksize, ksize), cv::Point(-1, -1), border);
        break;
    }
    case FILTER_GAUSS_BLUR: {
        setSliders(m, true, "SigmaX:", true, "SigmaY:");
        cv::GaussianBlur(mat, mat, cv::Size(ksize, ksize), value_a / 50.0, value_b / 50.0, border);
        break;
    }
    case FILTER_MEDIAN: {
        setSliders(m, false, nullptr, false, nullptr);

        assert(mat.isContinuous());

        auto kernel = [d = ksize / 2, border](const cv::Mat mat, int x, int y) {
            std::array<uint8_t, 7*7> stack;
            int stack_idx = 0;
            for (int j = y - d; j <= y + d; j++) {
                for (int i = x - d; i <= x + d; i++) {
                    if (i >= 0 && i < mat.cols && j >= 0 && j < mat.rows)
                        stack[stack_idx++] = mat.at<uint8_t>(j, i);
                    else {
                        if (border == cv::BORDER_ISOLATED)
                            return mat.at<uint8_t>(y, x);
                        else if (border == cv::BORDER_REFLECT)
                            stack[stack_idx++] = mat.at<uint8_t>(
                                j < 0 ? std::abs(j) : j >= mat.rows ? mat.rows - (j - mat.rows + 1) : j,
                                i < 0 ? std::abs(i) : i >= mat.cols ? mat.cols - (i - mat.cols + 1) : i);
                        else // border == cv::BORDER_REPLICATE
                            stack[stack_idx++] = mat.at<uint8_t>(
                                std::clamp(j, 0, mat.rows - 1),
                                std::clamp(i, 0, mat.cols - 1));
                    }
                }
            }
            assert(stack_idx > 0);
            std::sort(stack.begin(), stack.begin() + stack_idx);
            return stack[stack_idx / 2];
        };

        std::vector<cv::Mat> mats(mat.channels());
        cv::split(mat, mats);
        for (auto& dst : mats) {
            cv::Mat orig = dst.clone();
            dst.forEach<uint8_t>([&kernel, &orig] (uint8_t& value, const int* p) {
                value = kernel(orig, p[1], p[0]);
            });
        }

        cv::merge(mats, mat);

        break;
    }
    case FILTER_SOBEL: {
        setSliders(m, false, nullptr, false, nullptr);
        cv::GaussianBlur(mat, mat, cv::Size(3, 3), 0, 0, border);
        if (cvt_to_bgr != -1)
            cv::cvtColor(mat, mat, cvt_to_bgr);
        cv::cvtColor(mat, mat, cv::COLOR_BGR2GRAY);

        cv::Mat grad_x;
        cv::Sobel(mat, grad_x, CV_16S, 1, 0, ksize, 1, 0, border);
        cv::convertScaleAbs(grad_x, grad_x);
        cv::Mat grad_y;
        cv::Sobel(mat, grad_y, CV_16S, 0, 1, ksize, 1, 0, border);
        cv::convertScaleAbs(grad_y, grad_y);
        cv::addWeighted(grad_x, 0.5, grad_y, 0.5, 0, mat);

        cvt_to_bgr = cv::COLOR_GRAY2BGR;
        break;
    }
    case FILTER_LAPLACIAN: {
        setSliders(m, false, nullptr, false, nullptr);
        cv::GaussianBlur(mat, mat, cv::Size(3, 3), 0, 0, border);
        if (cvt_to_bgr != -1)
            cv::cvtColor(mat, mat, cvt_to_bgr);
        cv::cvtColor(mat, mat, cv::COLOR_BGR2GRAY);

        cv::Mat tmp;
        cv::Laplacian(mat, tmp, CV_16S, ksize, 1, 0, border);
        mat = tmp;

        cv::convertScaleAbs(mat, mat);

        cvt_to_bgr = cv::COLOR_GRAY2BGR;
        break;
    }
    case FILTER_CANNY: {
        setSliders(m, true, "Próg:", true, "Odcięcie:");
        cv::GaussianBlur(mat, mat, cv::Size(3, 3), 0, 0, border);
        if (cvt_to_bgr != -1)
            cv::cvtColor(mat, mat, cvt_to_bgr);
        cv::cvtColor(mat, mat, cv::COLOR_BGR2GRAY);

        cv::Mat tmp;
        cv::Canny(mat, tmp, value_a, value_a * value_b, ksize);
        mat = tmp;

        cvt_to_bgr = cv::COLOR_GRAY2BGR;
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


FilterDialog* FilterDialog::create(SubWindow* subwindow)
{
    if (subwindow == nullptr) {
        QMessageBox::warning(nullptr, "Filtry", "Brak wybranego obrazu");
        return nullptr;
    }
    return new FilterDialog(subwindow);
}


FilterDialog::FilterDialog(SubWindow* subwindow)
    : m(std::make_unique<Data>())
{
    assert(subwindow != nullptr);
    setAttribute(Qt::WA_DeleteOnClose);
    setWindowTitle("Filtry");
    m->self = this;
    m->subwindow = subwindow;

    m->combo_border.addItems(QStringList(g_border_labels.begin(), g_border_labels.end()));
    m->layout.addWidget(&m->combo_border);

    m->combo_filter.addItems(QStringList(g_filter_labels.begin(), g_filter_labels.end()));
    m->layout.addWidget(&m->combo_filter);

    {
        m->label_combo_mask_size.setText("Rozmiar maski: ");
        m->layout_grid.addWidget(&m->label_combo_mask_size, 0, 0, Qt::AlignRight);

        m->combo_mask_size.addItems(QStringList(g_mask_size_labels.begin(), g_mask_size_labels.end()));
        m->combo_mask_size.setCurrentIndex(0);
        m->layout_grid.addWidget(&m->combo_mask_size, 0, 1, 1, 2);

        m->label_a.setText("A:");
        m->layout_grid.addWidget(&m->label_a, 1, 0, Qt::AlignRight);

        m->slider_a.setOrientation(Qt::Horizontal);
        m->slider_a.setRange(0, 100);
        m->layout_grid.addWidget(&m->slider_a, 1, 1, 1, 2);

        m->label_b.setText("B:");
        m->layout_grid.addWidget(&m->label_b, 2, 0, Qt::AlignRight);

        m->slider_b.setOrientation(Qt::Horizontal);
        m->slider_b.setRange(0, 100);
        m->layout_grid.addWidget(&m->slider_b, 2, 1, 1, 2);
    }
    m->layout.addLayout(&m->layout_grid);

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
    connect(&m->combo_border, &QComboBox::currentIndexChanged, this, [this] {
        updatePreview(*m);
    });
    connect(&m->combo_filter, &QComboBox::currentIndexChanged, this, [this] {
        updatePreview(*m);
    });
    connect(&m->combo_mask_size, &QComboBox::currentIndexChanged, this, [this] {
        updatePreview(*m);
    });
    connect(&m->slider_a, &QSlider::valueChanged, this, [this] {
        updatePreview(*m);
    });
    connect(&m->slider_b, &QSlider::valueChanged, this, [this] {
        updatePreview(*m);
    });
    connect(&m->button_cancel, &QPushButton::clicked, this, [this] {
        close();
    });
    connect(&m->button_apply, &QPushButton::clicked, this, [this] {
        updatePreview(*m, true);
        close();
    });

    updatePreview(*m);
    show();
}


FilterDialog::~FilterDialog()
{
    if (m->subwindow)
        m->subwindow->removePreview();
}
