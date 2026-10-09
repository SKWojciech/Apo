#include "HistogramStretchWindow.h"
#include <QGridLayout>
#include <QComboBox>
#include <QSpinBox>
#include <QLabel>
#include <QGraphicsRectItem>
#include <QPushButton>
#include <QGraphicsSceneMouseEvent>
#include <QMouseEvent>
#include <QLineEdit>
#include <QMessageBox>
#include <opencv2/opencv.hpp>

#include "../SubWindow.h"
#include "../MyGraphicsView.h"



struct HistogramStretchScene : QGraphicsScene
{
    Q_OBJECT

Q_SIGNALS:
    void mousePressed(QGraphicsSceneMouseEvent* event);
    void mouseMoved(QGraphicsSceneMouseEvent* event);

protected:
    void mousePressEvent(QGraphicsSceneMouseEvent* event) override {
        emit mousePressed(event);
    }
    void mouseMoveEvent(QGraphicsSceneMouseEvent* event) override {
        emit mouseMoved(event);
    }
};



////////////////////////////
/// HistogramStretchWindow
////////////////////////////

struct HistogramStretchWindow::Data
{
    HistogramStretchWindow* self{};

    SubWindow* subwindow{};
    std::array<size_t, 256> hist_src{};
    int hist_src_channel = -1;
    size_t hist_src_max{};
    struct SavedSettings {
        int p1 = -1, p2 = -1, q1 = -1, q2 = -1;
    } saved_settings{};

    QVBoxLayout layout{};
    QComboBox combo_subwindow{};
    SubWindowChannelModel model_channel{};
    QComboBox combo_channel{};
    QPushButton button_stretch{};
    QPushButton button_negate{};
    QPushButton button_apply{};
    QGridLayout layout_grid{};
    QLabel label_p{};
    QSpinBox spinbox_p1{};
    QSpinBox spinbox_p2{};
    QLabel label_q{};
    QSpinBox spinbox_q1{};
    QSpinBox spinbox_q2{};
    QLabel label;
    HistogramStretchScene scene{};
    MyGraphicsView view{};
    std::vector<QGraphicsPolygonItem*> guides{};
    std::vector<QGraphicsRectItem*> lines_src{};
    std::vector<QGraphicsRectItem*> rects_src{};
    std::vector<QGraphicsRectItem*> lines_dst{};
    std::vector<QGraphicsRectItem*> rects_dst{};
};


static void updateSourceHistogram(HistogramStretchWindow::Data& m);
static void autoSetRange(HistogramStretchWindow::Data& m);


static void setSubWindow(HistogramStretchWindow::Data& m, SubWindow* subwindow)
{
    SubWindow* old_subwindow = m.subwindow;
    m.subwindow = subwindow;
    if (old_subwindow != nullptr && old_subwindow != m.subwindow) {
        QWidget::disconnect(old_subwindow, nullptr, m.self, nullptr);
    }

    if (m.subwindow == nullptr)
        return;

    assert(m.subwindow->getMat().channels() > 0);

    m.hist_src_channel = -1;
    // TODO: Update m.model_channel
    m.combo_channel.setCurrentIndex(0);

    if (m.subwindow != old_subwindow) {
        QWidget::connect(subwindow, &SubWindow::closing, m.self, [&m] {
            m.subwindow = nullptr;
            m.self->close();
        });
        QWidget::connect(subwindow, &SubWindow::matChanged, m.self, [&m] {
            setSubWindow(m, m.subwindow);
            m.combo_channel.setCurrentIndex(0);
        });
    }
}


static void updateSourceHistogram(HistogramStretchWindow::Data& m)
{
    const int channel = m.combo_channel.currentIndex();
    if (channel == -1 || m.hist_src_channel == channel)
        return;

    const cv::Mat mat = m.subwindow->getMat();
    const int channels = mat.channels();
    assert(channel >= 0 && channel < channels);

    m.hist_src.fill(0);

    for (const uchar* p = mat.data + channel; p < mat.dataend; p += channels) {
        assert(*p <= 255);
        m.hist_src[*p] += 1;
    }
    m.hist_src_max = std::ranges::max(m.hist_src);

    for (int i = 0; size_t x : m.hist_src)
        m.rects_src[i]->setRect(i++, 100, 1, -float(x) / float(m.hist_src_max) * 100.0f);

    m.scene.update();
}


static void updateDestHistogram(HistogramStretchWindow::Data& m, bool apply = false)
{
    if (m.combo_channel.currentIndex() == -1)
        return;

    const int channel = m.combo_channel.currentIndex();

    const int a = m.spinbox_p1.value(), b = m.spinbox_p2.value();
    const int c = m.spinbox_q1.value(), d = m.spinbox_q2.value();
    float p1 = a / 255.f, p2 = b / 255.f;
    float q1 = c / 255.f, q2 = d / 255.f;

    {
        const auto [sa, sb, sc, sd] = m.saved_settings;
        if (a == sa && b == sb && c == sc && d == sd)
            return;
    }

    bool swapped = false;
    if (p1 > p2) {
        std::swap(p1, p2);
        swapped = !swapped;
    }
    if (q1 > q2) {
        std::swap(q1, q2);
        swapped = !swapped;
    }

    if (p1 == p2)
        return;

    // Wyjmij kanał z obrazu
    cv::Mat mat = m.subwindow->getMat().clone();
    assert(mat.depth() == CV_8U);
    cv::Mat mat_cn;
    cv::extractChannel(mat, mat_cn, channel);

    // Rozciągnij histogram
    mat_cn.forEach<uint8_t>([p1, p2, q1, q2, swapped](uint8_t& p, const int[]) {
        float x = p / 255.0f;
        x = (x - p1) * ((q2 - q1) / (p2 - p1)) + q1;
        x = swapped ? q2 - x + q1 : x;
        x = std::clamp(x, q1, q2);
        x *= 255.f;
        x = std::clamp(x, 0.f, 255.f);
        p = (uint8_t)std::round(x);
    });

    // Oblicz nowy histogram
    std::array<size_t, 256> hist{};
    for (const uint8_t* p = mat_cn.datastart; p < mat_cn.dataend; p++) {
        assert(*p <= 255);
        hist[*p] += 1;
    }

    // Zaktualizuj prostokąty dla podglądu
    size_t maxh = std::ranges::max(hist);
    for (int i = 0; size_t x : hist)
        m.rects_dst[i]->setRect(i++, 205, 1, -float(x) / float(maxh) * 100.0f);
    m.scene.update();

    cv::insertChannel(mat_cn, mat, channel);

    if (apply)
        m.subwindow->setMat(mat, m.subwindow->getCvtToBgr());
    else
        m.subwindow->setPreview(mat, m.subwindow->getCvtToBgr());
}


static void autoSetRange(HistogramStretchWindow::Data& m)
{
    if (m.combo_channel.currentIndex() == -1)
        return;

    const size_t thresh = m.hist_src_max * 0.01;

    int i;
    for (i = 0; i < 256; i++) {
        if (m.hist_src[i] > thresh)
            break;
    }
    m.spinbox_p1.setValue(i);

    for (i = 255; i >= 0; i--) {
        if (m.hist_src[i] > thresh)
            break;
    }
    m.spinbox_p2.setValue(i);

    m.spinbox_q1.setValue(0);
    m.spinbox_q2.setValue(255);
}


static void updateGuides(HistogramStretchWindow::Data& m)
{
    const int a = m.spinbox_p1.value(), b = m.spinbox_p2.value();
    const int c = m.spinbox_q1.value(), d = m.spinbox_q2.value();

    {
        QPointF p1(a, 100), p2(a + 1, 100), p3(c + 1, 105), p4(c, 105);
        m.guides[0]->setPolygon({p1, p2, p3, p4, p1});
    }
    {
        QPointF p1(b, 100), p2(b + 1, 100), p3(d + 1, 105), p4(d, 105);
        m.guides[1]->setPolygon({p1, p2, p3, p4, p1});
    }
}


static void mouseEvent(HistogramStretchWindow::Data& m, QGraphicsSceneMouseEvent* event)
{
    float x = event->scenePos().x(), y = event->scenePos().y();

    if (y < 100) {
        int a = m.spinbox_p1.value(), b = m.spinbox_p2.value();
        float d1 = std::abs(a - x), d2 = std::abs(b - x);
        float d = std::min(d1, d2);
        if (d < 0.25)
            return;
        d1 <= d2 ? m.spinbox_p1.setValue(x) : m.spinbox_p2.setValue(x);
    } else {
        int a = m.spinbox_q1.value(), b = m.spinbox_q2.value();
        float d1 = std::abs(a - x), d2 = std::abs(b - x);
        float d = std::min(d1, d2);
        if (d < 0.25)
            return;
        d1 <= d2 ? m.spinbox_q1.setValue(x) : m.spinbox_q2.setValue(x);
    }
}


HistogramStretchWindow* HistogramStretchWindow::create(SubWindow* subwindow)
{
    if (subwindow == nullptr) {
        QMessageBox::warning(nullptr, "Rozciąganie histogramu", "Brak wybranego obrazu");
        return nullptr;
    }
    return new HistogramStretchWindow(subwindow);
}


HistogramStretchWindow::HistogramStretchWindow(SubWindow* subwindow)
    : m(std::make_unique<Data>())
{
    m->self = this;
    setSubWindow(*m, subwindow);

    setAttribute(Qt::WA_DeleteOnClose, true);
    setWindowTitle(QString("Rozciągnij histogram"));
    resize(800, 400);

    m->combo_subwindow.setModel(SubWindowModel::get());
    m->combo_subwindow.setCurrentIndex(SubWindowModel::get()->getIndexOfSubWindow(m->subwindow));
    m->layout.addWidget(&m->combo_subwindow, 0, Qt::AlignTop);
    m->model_channel.setSubWindow(m->subwindow);
    m->combo_channel.setModel(&m->model_channel);
    m->layout.addWidget(&m->combo_channel, 0, Qt::AlignTop);

    m->label_p.setText("Od (p1, p2): ");
    m->spinbox_p1.setRange(0, 255);
    m->spinbox_p2.setRange(0, 255);
    m->layout_grid.addWidget(&m->label_p, 0, 0);
    m->layout_grid.addWidget(&m->spinbox_p1, 0, 1, Qt::AlignLeft);
    m->layout_grid.addWidget(&m->spinbox_p2, 0, 2, Qt::AlignLeft);

    m->label_q.setText("Do (q1, q2): ");
    m->spinbox_q1.setRange(0, 255);
    m->spinbox_q2.setRange(0, 255);
    m->layout_grid.addWidget(&m->label_q, 1, 0);
    m->layout_grid.addWidget(&m->spinbox_q1, 1, 1, Qt::AlignLeft);
    m->layout_grid.addWidget(&m->spinbox_q2, 1, 2, Qt::AlignLeft);

    m->button_stretch.setText("Rozciągnij");
    m->layout_grid.addWidget(&m->button_stretch, 0, 3, Qt::AlignLeft);

    m->button_negate.setText("Odwróć");
    m->layout_grid.addWidget(&m->button_negate, 1, 3, Qt::AlignLeft);

    m->layout_grid.setColumnStretch(3, 1);
    m->layout.addLayout(&m->layout_grid);

    m->label.setText("Możesz przesuwać przedziały poniżej myszką!");
    m->layout.addWidget(&m->label);

    m->scene.setSceneRect(0, 0, 256, 205);
    m->scene.setBackgroundBrush(QBrush(QColorConstants::White));
    m->scene.addRect(0, 100, 256, 5, QPen(Qt::NoPen), QColor(255, 255, 255));
    m->guides.resize(2);
    m->guides[0] = m->scene.addPolygon({}, QPen(Qt::NoPen), QColor(0, 255, 0, 120));
    m->guides[1] = m->scene.addPolygon({}, QPen(Qt::NoPen), QColor(0, 255, 0, 120));

    m->rects_src.resize(256);
    for (auto& rect : m->rects_src)
        rect = m->scene.addRect({}, QPen(Qt::NoPen), QBrush(QColorConstants::Black));

    m->lines_src.resize(2);
    for (auto& item : m->lines_src)
        item = m->scene.addRect({0, 0, 1, 100}, QPen(Qt::NoPen), QColor(0, 255, 0, 120));

    m->rects_dst.resize(256);
    for (auto& rect : m->rects_dst)
        rect = m->scene.addRect({}, QPen(Qt::NoPen), QBrush(QColorConstants::Black));

    m->lines_dst.resize(2);
    for (auto& item : m->lines_dst)
        item = m->scene.addRect({0, 105, 1, 100}, QPen(Qt::NoPen), QColor(0, 255, 0, 120));

    m->view.setScene(&m->scene);
    m->view.setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m->view.setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m->view.setRenderHint(QPainter::Antialiasing, false);
    m->layout.addWidget(&m->view);

    m->button_apply.setText("Zastosuj");
    m->layout.addWidget(&m->button_apply, 0, Qt::AlignBottom | Qt::AlignRight);

    setLayout(&m->layout);


    connect(&m->combo_subwindow, &QComboBox::currentIndexChanged, this, [this] (int idx) {
        if (idx == -1)
            return;
        setSubWindow(*m, SubWindowModel::get()->getSubWindowAt(idx));
        updateSourceHistogram(*m);
        autoSetRange(*m);
    });
    connect(&m->combo_channel, &QComboBox::currentIndexChanged, this, [this] {
        updateSourceHistogram(*m);
        autoSetRange(*m);
    });
    connect(&m->spinbox_p1, &QSpinBox::valueChanged, this, [this] (int value) {
        m->lines_src[0]->setRect(value, 0, 1, 100);
        updateGuides(*m);
        updateDestHistogram(*m);
    });
    connect(&m->spinbox_p2, &QSpinBox::valueChanged, this, [this] (int value) {
        m->lines_src[1]->setRect(value, 0, 1, 100);
        updateGuides(*m);
        updateDestHistogram(*m);
    });
    connect(&m->spinbox_q1, &QSpinBox::valueChanged, this, [this] (int value) {
        m->lines_dst[0]->setRect(value, 105, 1, 100);
        updateGuides(*m);
        updateDestHistogram(*m);
    });
    connect(&m->spinbox_q2, &QSpinBox::valueChanged, this, [this] (int value) {
        m->lines_dst[1]->setRect(value, 105, 1, 100);
        updateGuides(*m);
        updateDestHistogram(*m);
    });
    connect(&m->button_stretch, &QPushButton::clicked, this, [this] {
        autoSetRange(*m);
    });
    connect(&m->button_negate, &QPushButton::clicked, this, [this] {
        m->spinbox_p1.setValue(0);
        m->spinbox_p2.setValue(255);
        m->spinbox_q1.setValue(255);
        m->spinbox_q2.setValue(0);
    });
    connect(&m->button_apply, &QPushButton::clicked, this, [this] {
        updateDestHistogram(*m, true);
        close();
    });
    connect(&m->scene, &HistogramStretchScene::mousePressed, this, [this] (QGraphicsSceneMouseEvent* event) {
        mouseEvent(*m, event);
    });
    connect(&m->scene, &HistogramStretchScene::mouseMoved, this, [this] (QGraphicsSceneMouseEvent* event) {
        mouseEvent(*m, event);
    });

    updateSourceHistogram(*m);
    autoSetRange(*m);
    show();
}


HistogramStretchWindow::~HistogramStretchWindow()
{
}


void HistogramStretchWindow::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    m->view.fitInView(m->scene.sceneRect(), Qt::IgnoreAspectRatio);
}


void HistogramStretchWindow::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);
    m->view.fitInView(m->scene.sceneRect(), Qt::IgnoreAspectRatio);
}

void HistogramStretchWindow::closeEvent(QCloseEvent* event)
{
    if (m->subwindow)
        m->subwindow->removePreview();
    QWidget::closeEvent(event);
}


#include "HistogramStretchWindow.moc"
