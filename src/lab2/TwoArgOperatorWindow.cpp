#include "TwoArgOperatorWindow.h"
#include <QMessageBox>
#include <QVBoxLayout>
#include <QComboBox>
#include <QSlider>
#include <QPushButton>
#include <opencv2/imgproc.hpp>
#include "../SubWindow.h"



enum TwoArgOperators
{
    OPERATOR_ADD,
    OPERATOR_SUBTRACT,
    OPERATOR_BLEND,
    OPERATOR_AND,
    OPERATOR_OR,
    OPERATOR_NOT, // To nie jest operator dwuargumentowy, ale nie wiem, gdzie indziej go wsadzić.
    OPERATOR_XOR
};
static constexpr std::array g_operator_labels = {
    "Dodawanie", "Odejmowanie", "Mieszanie", "AND", "OR", "NOT", "XOR"
};


struct TwoArgOperatorWindow::Data
{
    TwoArgOperatorWindow* self;
    SubWindow* managed_subwindow;
    SubWindow* lhs_window;
    SubWindow* rhs_window;

    QVBoxLayout layout;
    QHBoxLayout layout_inputs;
    QComboBox combo_lhs;
    QComboBox combo_rhs;
    QComboBox combo_operator;
    QSlider slider_alpha;

    QHBoxLayout layout_buttons;
    QPushButton button_apply;
    QPushButton button_cancel;
};


static void initPreviewSubwindow(TwoArgOperatorWindow::Data& m, const cv::Mat& init_mat, const int cvt_to_bgr)
{
    assert(m.managed_subwindow == nullptr);
    m.managed_subwindow = new SubWindow(init_mat, cvt_to_bgr, "Wyjście");
    m.managed_subwindow->show();

    QWidget::connect(m.managed_subwindow, &SubWindow::closing, m.self, [&m] {
        m.managed_subwindow = nullptr;
        m.self->close();
    });
}


static void updatePreview(TwoArgOperatorWindow::Data& m)
{
    if (m.combo_lhs.currentIndex() < 0 || m.combo_rhs.currentIndex() < 0)
        return;

    const SubWindow* lhs_window = SubWindowModel::get()->getSubWindowAt(m.combo_lhs.currentIndex());
    const cv::Mat lhs = lhs_window->getMat();
    const int lhs_cvt_to_bgr = lhs_window->getCvtToBgr();
    assert(lhs.depth() == CV_8U);

    const SubWindow* rhs_window = SubWindowModel::get()->getSubWindowAt(m.combo_rhs.currentIndex());
    const cv::Mat rhs = rhs_window->getMat();
    const int rhs_cvt_to_bgr = rhs_window->getCvtToBgr();
    assert(rhs.depth() == CV_8U);

    if (lhs.size() != rhs.size()) {
        QMessageBox::warning(nullptr, "Operacja dwuargumentowa", "Obrazy muszą mieć ten sam rozmiar");
        m.combo_lhs.setVisible(true);
        m.combo_rhs.setVisible(true);
        return;
    }
    if (lhs_cvt_to_bgr != rhs_cvt_to_bgr) {
        QMessageBox::warning(nullptr, "Operacja dwuargumentowa", "Obrazy muszą mieć ten sam format");
        m.combo_lhs.setVisible(true);
        m.combo_rhs.setVisible(true);
        return;
    }

    int cvt_to_bgr = lhs_cvt_to_bgr;
    cv::Mat output;
    const TwoArgOperators selection = (TwoArgOperators)m.combo_operator.currentIndex();

    switch (selection) {
    case OPERATOR_ADD: {
        m.slider_alpha.setVisible(false);
        m.combo_rhs.setVisible(true);
        cv::add(lhs, rhs, output);
        break;
    }
    case OPERATOR_SUBTRACT: {
        m.slider_alpha.setVisible(false);
        m.combo_rhs.setVisible(true);
        cv::subtract(lhs, rhs, output);
        break;
    }
    case OPERATOR_BLEND: {
        m.slider_alpha.setVisible(true);
        m.combo_rhs.setVisible(true);
        const double alpha = m.slider_alpha.value() / 100.0;
        cv::addWeighted(lhs, 1.0 - alpha, rhs, alpha, 0, output);
        break;
    }
    case OPERATOR_AND: {
        m.slider_alpha.setVisible(false);
        m.combo_rhs.setVisible(true);
        cv::bitwise_and(lhs, rhs, output);
        break;
    }
    case OPERATOR_OR: {
        m.slider_alpha.setVisible(false);
        m.combo_rhs.setVisible(true);
        cv::bitwise_or(lhs, rhs, output);
        break;
    }
    case OPERATOR_NOT: {
        m.slider_alpha.setVisible(false);
        m.combo_rhs.setVisible(false);
        cv::bitwise_not(lhs, output);
        break;
    }
    case OPERATOR_XOR: {
        m.slider_alpha.setVisible(false);
        m.combo_rhs.setVisible(true);
        cv::bitwise_xor(lhs, rhs, output);
        break;
    }
    }

    if (!m.managed_subwindow)
        initPreviewSubwindow(m, output, cvt_to_bgr);
    else
        m.managed_subwindow->setMat(output, cvt_to_bgr);
}


enum ConnectSubWindowType { LHS_SUBWINDOW, RHS_SUBWINDOW };
static void connectSubWindow(TwoArgOperatorWindow::Data& m, SubWindow* subwindow, ConnectSubWindowType type)
{
    SubWindow*& ref = (type == LHS_SUBWINDOW ? m.lhs_window : m.rhs_window);
    SubWindow* old_subwindow = ref;
    ref = subwindow;

    SubWindow* other_subwindow = (type == LHS_SUBWINDOW ? m.rhs_window : m.lhs_window);

    if (old_subwindow == subwindow
        || subwindow == other_subwindow
        || (m.managed_subwindow != nullptr && subwindow == m.managed_subwindow))
        return;

    if (old_subwindow != nullptr && old_subwindow != m.managed_subwindow) {
        QWidget::disconnect(old_subwindow, nullptr, m.self, nullptr);
    }

    if (subwindow != nullptr) {
        QWidget::connect(subwindow, &SubWindow::matChanged, m.self, [&m] { updatePreview(m); });
    }
}


TwoArgOperatorWindow* TwoArgOperatorWindow::create(SubWindow* subwindow)
{
    return new TwoArgOperatorWindow(subwindow);
}


TwoArgOperatorWindow::TwoArgOperatorWindow(SubWindow* subwindow)
    : m(std::make_unique<Data>())
{
    setAttribute(Qt::WA_DeleteOnClose);
    setWindowTitle("Operacje dwuargumentowe");
    m->self = this;

    {
        m->combo_lhs.setModel(SubWindowModel::get());
        m->combo_lhs.setCurrentIndex(SubWindowModel::get()->getIndexOfSubWindow(subwindow));
        m->layout_inputs.addWidget(&m->combo_lhs);

        m->combo_rhs.setModel(SubWindowModel::get());
        m->combo_rhs.setCurrentIndex(SubWindowModel::get()->getIndexOfSubWindow(subwindow));
        m->layout_inputs.addWidget(&m->combo_rhs);
    }
    m->layout.addLayout(&m->layout_inputs);

    m->combo_operator.addItems(QStringList(g_operator_labels.begin(), g_operator_labels.end()));
    m->combo_operator.setCurrentIndex(0);
    m->layout.addWidget(&m->combo_operator);

    m->slider_alpha.setOrientation(Qt::Horizontal);
    m->slider_alpha.setRange(0, 100);
    m->layout.addWidget(&m->slider_alpha);

    {
        m->layout_buttons.addStretch(1);

        m->button_apply.setText("Zastosuj");
        m->layout_buttons.addWidget(&m->button_apply);

        m->button_cancel.setText("Anuluj");
        m->layout_buttons.addWidget(&m->button_cancel);
    }
    m->layout.addLayout(&m->layout_buttons);

    setLayout(&m->layout);

    connect(&m->combo_lhs, &QComboBox::currentIndexChanged, this, [this] (int idx) {
        connectSubWindow(*m, idx == -1 ? nullptr : SubWindowModel::get()->getSubWindowAt(idx), LHS_SUBWINDOW);
        updatePreview(*m);
    });
    connect(&m->combo_rhs, &QComboBox::currentIndexChanged, this, [this] (int idx) {
        connectSubWindow(*m, idx == -1 ? nullptr : SubWindowModel::get()->getSubWindowAt(idx), RHS_SUBWINDOW);
        updatePreview(*m);
    });
    connect(&m->combo_operator, &QComboBox::currentIndexChanged, this, [this] {
        updatePreview(*m);
    });
    connect(&m->slider_alpha, &QSlider::valueChanged, this, [this] {
        updatePreview(*m);
    });
    connect(&m->button_cancel, &QPushButton::clicked, this, [this] {
        close();
    });
    connect(&m->button_apply, &QPushButton::clicked, this, [this] {
        updatePreview(*m);
        m->managed_subwindow = nullptr;
        close();
    });

    updatePreview(*m);
    show();
}


TwoArgOperatorWindow::~TwoArgOperatorWindow()
{
    if (m->managed_subwindow != nullptr) {
        disconnect(m->managed_subwindow, nullptr, this, nullptr);
        m->managed_subwindow->close();
    }
}
