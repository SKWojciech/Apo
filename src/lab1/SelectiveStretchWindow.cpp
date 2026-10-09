#include "SelectiveStretchWindow.h"
#include <QVBoxLayout>
#include <QComboBox>
#include <QPushButton>
#include <QGraphicsPolygonItem>
#include <QGraphicsSceneMouseEvent>
#include <QLabel>
#include <QMessageBox>
#include <opencv2/core/mat.hpp>
#include <opencv2/imgproc.hpp>
#include "../SubWindow.h"
#include "../MyGraphicsView.h"



////////////////////////////
/// SelectiveStretchScene
////////////////////////////

struct SelectiveStretchScene : QGraphicsScene
{
    Q_OBJECT

    QList<QPointF> m_point_list{};
    QGraphicsRectItem m_rect_right{};
    QGraphicsRectItem m_rect_bottom{};
    QGraphicsPathItem m_item_ident_path{};
    QGraphicsPathItem m_item_path{};

public:
    SelectiveStretchScene() {
        setBackgroundBrush(QColorConstants::White);
        setSceneRect(-4, 0, 260.0, 260.0);

        QLinearGradient gradient{};
        gradient.setColorAt(0, QColorConstants::Black);
        gradient.setColorAt(1, QColorConstants::White);
        m_rect_right.setRect(-4, 0, 4, 256);
        m_rect_right.setPen(Qt::NoPen);
        gradient.setStart(256, 256);
        gradient.setFinalStop(256, 0);
        m_rect_right.setBrush(gradient);
        addItem(&m_rect_right);

        m_rect_bottom.setPen(Qt::NoPen);
        m_rect_bottom.setRect(-4, 256, 256, 4);
        gradient.setStart(0, 256);
        gradient.setFinalStop(256, 256);
        m_rect_bottom.setBrush(gradient);
        addItem(&m_rect_bottom);

        m_item_ident_path.setBrush(Qt::NoBrush);
        m_item_ident_path.setPen(QPen(QColor(192, 192, 192, 150), 1, Qt::SolidLine, Qt::SquareCap, Qt::MiterJoin));
        QPainterPath path;
        path.moveTo(0.0, 255.0);
        path.lineTo(255.0, 0.0);
        m_item_ident_path.setPath(path);
        addItem(&m_item_ident_path);

        m_item_path.setBrush(Qt::NoBrush);
        m_item_path.setPen(QPen(QColorConstants::Gray, 1, Qt::SolidLine, Qt::SquareCap, Qt::MiterJoin));
        addItem(&m_item_path);
        reset();
    }

    void reset() {
        m_point_list.clear();
        m_point_list.append({0.0, 255.0});
        m_point_list.append({255.0, 0.0});
        updatePath();
        emit pointsChanged();
    }

    void getLUT(std::span<uint8_t, 256> dst) {
        int point_idx = -1;
        int range_end_idx = 0;
        qreal a, b;
        auto update_range = [&] {
            point_idx += 1;
            QPointF from = m_point_list[point_idx], to = m_point_list[point_idx + 1];
            a = ((255 - to.y()) - (255 - from.y())) / (to.x() - from.x());
            b = (255 - from.y()) - a * from.x();
            range_end_idx = (int)to.x() + 1;
        };
        update_range();

        for (int i = 0; i < 256; i++) {
            if (i == range_end_idx)
                update_range();

            qreal x = std::round(a * i + b);
            assert(x >= 0.0 && x <= 255.0);
            dst[i] = (uint8_t)x;
        }
    }

    void updatePath() {
        qreal ratio = 1;
        if (views().size() > 0) {
            const auto* view = views().first();
            ratio = (qreal)view->width() / (qreal)view->height();
        }
        qreal radiusX = ratio > 1 ? 1.5 / ratio : 1.5;
        qreal radiusY = ratio < 1 ? 1.5 * ratio : 1.5;
        QPainterPath path;
        path.moveTo(m_point_list.first());
        for (const auto& p : m_point_list) {
            path.lineTo(p);
            path.addEllipse(p, radiusX, radiusY);
            path.moveTo(p);
        }
        m_item_path.setPath(path);
    }

protected:
    void moveNearestPointTo(QPointF pos) {
        pos.setX(std::clamp(pos.x(), 0.0, 255.0));
        pos.setY(std::clamp(pos.y(), 0.0, 255.0));

        // Znajdź punkt najbliżej myszki
        int min_dist_idx = 0;
        qreal min_dist = std::abs(m_point_list.first().x() - pos.x());
        for (int i = 0; const auto& p : m_point_list) {
            qreal d = std::abs(p.x() - pos.x());
            if (d < min_dist)
                min_dist = d, min_dist_idx = i;
            i++;
        }

        QPointF new_pos(std::round(pos.x()), std::round(pos.y()));
        // Nie ruszaj bocznych punktów z ich osi X
        if (min_dist_idx == 0 || min_dist_idx == m_point_list.size() - 1)
            new_pos.setX(m_point_list[min_dist_idx].x());

        // Nie pozwalaj, aby punkty były na tej samej osi X
        for (int i = 0; i < m_point_list.size(); i++) {
            if (i == min_dist_idx) // Punkt, który ruszamy, może pozostać na swojej osi X
                continue;
            const auto& p = m_point_list[i];
            if (std::abs(p.x() - new_pos.x()) < 1)
                return;
        }

        m_point_list[min_dist_idx] = new_pos;

        updatePath();
        emit pointsChanged();
    }

    void addNewPoint(QPointF pos) {
        pos.setX(std::clamp(pos.x(), 0.0, 255.0));
        pos.setY(std::clamp(pos.y(), 0.0, 255.0));

        qreal x = std::round(pos.x()), y = std::round(pos.y());
        for (const auto& p : m_point_list) {
            // Nie dodawaj nowych punktów na tej samej osi X
            if (std::abs(p.x() - x) < 1)
                return;
        }
        int insertion_idx = 0;
        while (x > m_point_list[insertion_idx].x())
            insertion_idx++;
        m_point_list.insert(insertion_idx, QPointF(x, y));

        updatePath();
        emit pointsChanged();
    }

    void mousePressEvent(QGraphicsSceneMouseEvent* event) override {
        if (event->button() == Qt::LeftButton)
            moveNearestPointTo(event->scenePos());
        if (event->button() == Qt::RightButton)
            addNewPoint(event->scenePos());
    }

    void mouseMoveEvent(QGraphicsSceneMouseEvent* event) override {
        moveNearestPointTo(event->scenePos());
    }

Q_SIGNALS:
    void pointsChanged();
};



/////////////////////////////
/// SelectiveStretchWindow
/////////////////////////////

struct SelectiveStretchWindow::Data
{
    SelectiveStretchWindow* self;
    SubWindow* subwindow;
    std::array<size_t, 256> hist;
    std::array<uint8_t, 256> lut;

    QVBoxLayout layout;
    QComboBox combo_subwindow;
    SubWindowChannelModel model_channel;
    QComboBox combo_channel;
    QLabel label;
    std::array<QGraphicsRectItem*, 256> rects;
    SelectiveStretchScene scene;
    MyGraphicsView view;

    QHBoxLayout layout_buttons;
    QPushButton button_reset;
    QPushButton button_apply;
};


static void resetHist(SelectiveStretchWindow::Data& m)
{
    m.hist.fill(0);

    const int channel = m.combo_channel.currentIndex();
    if (m.subwindow == nullptr || channel == -1)
        return;

    const cv::Mat mat = m.subwindow->getMat();
    const int channels = mat.channels();
    assert(channel < channels);

    for (const auto* p = mat.data + channel; p < mat.datalimit; p += channels)
        m.hist[*p] += 1;

    const qreal maxh = std::ranges::max(m.hist);
    for (int i = 0; i < 256; i++)
        m.rects[i]->setRect(i, 256.0, 1.0, -(qreal)m.hist[i] / maxh * 256.0);
    m.scene.update();
}


static void resetLUT(SelectiveStretchWindow::Data& m)
{
    for (int i = 0; i < 256; i++)
        m.lut[i] = i;

    if (m.subwindow)
        m.subwindow->removePreview();

    m.scene.reset();
    resetHist(m);
}


static void updatePreview(SelectiveStretchWindow::Data& m, bool apply = false)
{
    const int channel = m.combo_channel.currentIndex();
    if (m.subwindow == nullptr || channel == -1)
        return;

    cv::Mat mat = m.subwindow->getMat().clone();
    const int channels = mat.channels();
    assert(channel < channels);

    m.hist.fill(0);
    for (auto* p = mat.data + channel; p < mat.datalimit; p += channels) {
        *p = m.lut[*p];
        m.hist[*p] += 1;
    }

    const qreal maxh = std::ranges::max(m.hist);
    for (int i = 0; i < 256; i++)
        m.rects[i]->setRect(i, 256.0, 1, -(qreal)m.hist[i] / maxh * 256.0);
    m.scene.update();

    if (apply) {
        m.subwindow->setMat(mat, m.subwindow->getCvtToBgr());
        m.subwindow->removePreview();
        return;
    }

    m.subwindow->setPreview(mat, m.subwindow->getCvtToBgr());
}


static void setSubwindow(SelectiveStretchWindow::Data& m, SubWindow* subwindow)
{
    SubWindow* old_subwindow = m.subwindow;
    m.subwindow = subwindow;
    if (old_subwindow != nullptr && m.subwindow != old_subwindow)
        QWidget::disconnect(old_subwindow, nullptr, m.self, nullptr);

    if (m.subwindow != old_subwindow) {
        QWidget::connect(m.subwindow, &SubWindow::closing, m.self, [&m] {
            m.subwindow = nullptr;
            m.self->close();
        });
        QWidget::connect(m.subwindow, &SubWindow::matChanged, m.self, [&m] {
            updatePreview(m);
        });
    }

    m.model_channel.setSubWindow(m.subwindow);
}


SelectiveStretchWindow* SelectiveStretchWindow::create(SubWindow* subwindow)
{
    if (subwindow == nullptr) {
        QMessageBox::warning(nullptr, "Rozciąganie selektywne", "Brak wybranego obrazu");
        return nullptr;
    }
    return new SelectiveStretchWindow(subwindow);
}


SelectiveStretchWindow::SelectiveStretchWindow(SubWindow* subwindow)
    : m(std::make_unique<Data>())
{
    setAttribute(Qt::WA_DeleteOnClose, true);
    setWindowTitle("Rozciąganie selektywne");
    m->self = this;
    assert(subwindow != nullptr);
    assert(subwindow->getMat().channels() > 0);

    for (int i = 0; i < 256; i++)
        m->lut[i] = i;

    for (int i = 0; i < 256; i++)
        m->rects[i] = m->scene.addRect({}, QPen(Qt::NoPen), QColorConstants::Black);

    m->combo_subwindow.setModel(SubWindowModel::get());
    m->combo_subwindow.setCurrentIndex(SubWindowModel::get()->getIndexOfSubWindow(subwindow));
    m->layout.addWidget(&m->combo_subwindow);

    m->combo_channel.setModel(&m->model_channel);
    m->layout.addWidget(&m->combo_channel);

    m->label.setText("Lewy Przycisk Myszki: Przesuń punkt\nPrawy Przycisk Myszki: Dodaj punkt");
    m->layout.addWidget(&m->label);

    m->view.setScene(&m->scene);
    m->view.setSceneRect(m->scene.sceneRect());
    m->view.setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m->view.setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m->layout.addWidget(&m->view, 1);

    m->layout_buttons.addStretch(1);
    m->button_reset.setText("Reset");
    m->layout_buttons.addWidget(&m->button_reset, 0);
    m->button_apply.setText("Zastosuj");
    m->layout_buttons.addWidget(&m->button_apply, 0);
    m->layout.addLayout(&m->layout_buttons);

    setLayout(&m->layout);

    connect(&m->combo_subwindow, &QComboBox::currentIndexChanged, this, [this] (int idx) {
        if (idx == -1)
            close();
        else
            setSubwindow(*m, SubWindowModel::get()->getSubWindowAt(idx));
    });
    connect(&m->combo_channel, &QComboBox::currentIndexChanged, this, [this] {
        resetHist(*m);
        updatePreview(*m);
    });
    connect(&m->button_apply, &QPushButton::clicked, this, [this] {
        updatePreview(*m, true);
        m->subwindow = nullptr;
        close();
    });
    connect(&m->button_reset, &QPushButton::clicked, this, [this] {
        resetLUT(*m);
    });
    connect(&m->scene, &SelectiveStretchScene::pointsChanged, this, [this] {
        m->scene.getLUT(m->lut);
        updatePreview(*m);
    });

    setSubwindow(*m, subwindow);
    resize(500, 600);
    resetLUT(*m);
    show();
}

void SelectiveStretchWindow::closeEvent(QCloseEvent* event)
{
    if (m->subwindow)
        m->subwindow->removePreview();
    QWidget::closeEvent(event);
}


void SelectiveStretchWindow::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    m->view.fitInView(m->scene.sceneRect(), Qt::IgnoreAspectRatio);
    m->scene.updatePath();
}


void SelectiveStretchWindow::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);
    m->view.fitInView(m->scene.sceneRect(), Qt::IgnoreAspectRatio);
    m->scene.updatePath();
}


SelectiveStretchWindow::~SelectiveStretchWindow()
{
}


#include "SelectiveStretchWindow.moc"
