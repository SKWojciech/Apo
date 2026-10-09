#include "GrabCutWindow.h"

#include "../SubWindow.h"
#include "../MyGraphicsView.h"

#include <QVBoxLayout>
#include <QMessageBox>
#include <QComboBox>
#include <QGraphicsItem>
#include <QGraphicsView>
#include <QGraphicsSceneEvent>
#include <QPushButton>

#include <opencv2/imgproc.hpp>



static float dist(const QPointF& a, const QPointF& b)
{
    return std::sqrt(std::pow(a.x() - b.x(), 2) + std::pow(a.y() - b.y(), 2));
}



///////////////////////////
/// GrabCutWindow::Scene
///////////////////////////

struct GrabCutWindow::Scene : QGraphicsScene
{
    Q_OBJECT
public:
    QGraphicsPixmapItem m_image;
    QPointF m_p1, m_p2;
    QGraphicsRectItem m_rect;

    Scene() {
        addItem(&m_image);
        m_rect.setPen(QPen(QColorConstants::Red));
        addItem(&m_rect);
    }
    void setPoints(QPointF a, QPointF b) {
        qreal max_w = m_image.pixmap().width() - 1;
        qreal max_h = m_image.pixmap().height() - 1;
        if (max_w > 0 && max_h > 0) {
            a.rx() = std::clamp(a.rx(), 0.0, max_w);
            a.ry() = std::clamp(a.ry(), 0.0, max_h);
            b.rx() = std::clamp(b.rx(), 0.0, max_w);
            b.ry() = std::clamp(b.ry(), 0.0, max_h);
            if (a.y() > b.y())
                std::swap(a.ry(), b.ry());
            if (a.x() > b.x())
                std::swap(a.rx(), b.rx());

            m_p1 = a;
            m_p2 = b;
        } else {
            m_p1 = m_p2 = {0, 0};
        }
        m_rect.setRect(QRectF(m_p1, m_p2));
        emit pointsChanged();
    }
    QRectF getRect() const {
        return QRectF(m_p1, m_p2);
    }
    void setPixmap(const QPixmap& pixmap) {
        const QRectF old_rect = m_image.pixmap().rect();
        m_image.setPixmap(pixmap);
        setSceneRect(m_image.boundingRect());
        if (m_p1 == m_p2 && m_p1 == QPointF{0, 0})
            setPoints({0, 0}, {(qreal)pixmap.width() - 1, (qreal)pixmap.height() - 1});
        else if (old_rect != pixmap.rect())
            setPoints(m_p1, m_p2);
    }
protected:
    void mousePressEvent(QGraphicsSceneMouseEvent* event) override {
        QPointF point = event->scenePos();
        if (dist(point, m_p1) < dist(point, m_p2))
            setPoints(point, m_p2);
        else
            setPoints(m_p1, point);
    }

    void mouseMoveEvent(QGraphicsSceneMouseEvent* event) override {
        if (not event->buttons().testAnyFlags(Qt::LeftButton | Qt::RightButton))
            return;
        QPointF point = event->scenePos();
        if (dist(point, m_p1) < dist(point, m_p2))
            setPoints(point, m_p2);
        else
            setPoints(m_p1, point);
    }
Q_SIGNALS:
    void pointsChanged();
};



struct GrabCutWindow::Data
{
    GrabCutWindow* self;
    SubWindow* subwindow;

    QVBoxLayout layout;
    QComboBox combo_subwindow;
    Scene scene_image;
    MyGraphicsView view_image;

    QHBoxLayout layout_buttons;
    QPushButton button_apply;
    QPushButton button_cancel;
};


static void updateView(GrabCutWindow::Data& m, bool apply = false)
{
    if (m.subwindow == nullptr)
        return;

    cv::Mat mat = m.subwindow->getMat().clone();
    assert(mat.depth() == CV_8U);
    const int cvt_to_bgr = m.subwindow->getCvtToBgr();

    QRectF rectf = m.scene_image.getRect();
    cv::Rect rect(rectf.x(), rectf.y(), rectf.width(), rectf.height());
    if (rect.width == 0 || rect.height == 0)
        return;

    cv::Mat mask(mat.size(), CV_8UC1);
    mask.setTo(cv::GC_BGD);
    mask(rect).setTo(cv::Scalar(cv::GC_PR_FGD));
    cv::Mat bg_model, fg_model;
    cv::grabCut(mat, mask, rect, bg_model, fg_model, 1, cv::GC_INIT_WITH_RECT);

    cv::Mat bw_mask(mask.rows, mask.cols, mat.type(), cv::Scalar::all(0));
    bw_mask.setTo(cv::Scalar::all(255), mask & cv::GC_FGD);

    if (apply) {
        cv::bitwise_and(mat, bw_mask, mat);
        m.subwindow->setMat(mat, cvt_to_bgr);
        m.subwindow->removePreview();
        return;
    }

    addWeighted(bw_mask, 0.5, mat, 0.5, 0.0, mat);
    if (cvt_to_bgr != -1)
        cv::cvtColor(mat, mat, cvt_to_bgr);
    m.scene_image.setPixmap(QPixmap::fromImage(QImage(mat.data, mat.cols, mat.rows, mat.step, QImage::Format_BGR888)).copy());
}


static void setSubwindow(GrabCutWindow::Data& m, SubWindow* subwindow)
{
    SubWindow* old_subwindow = m.subwindow;
    m.subwindow = subwindow;
    if (old_subwindow != nullptr && m.subwindow != old_subwindow)
        QWidget::disconnect(old_subwindow, nullptr, m.self, nullptr);

    if (m.subwindow == nullptr) {
        m.scene_image.setPixmap(QPixmap());
        return;
    }

    if (m.subwindow != old_subwindow) {
        QWidget::connect(m.subwindow, &SubWindow::closing, m.self, [&m] {
            setSubwindow(m, nullptr);
        });
        QWidget::connect(m.subwindow, &SubWindow::matChanged, m.self, [&m] {
            cv::Mat mat = m.subwindow->getMat().clone();
            const int cvt_to_bgr = m.subwindow->getCvtToBgr();
            if (cvt_to_bgr != -1)
                cv::cvtColor(mat, mat, cvt_to_bgr);
            m.scene_image.setPixmap(QPixmap::fromImage(QImage(mat.data, mat.cols, mat.rows, mat.step, QImage::Format_BGR888)));
            m.view_image.fitInView(&m.scene_image.m_image, Qt::KeepAspectRatio);
        });
    }

    cv::Mat mat = m.subwindow->getMat().clone();
    const int cvt_to_bgr = m.subwindow->getCvtToBgr();
    if (cvt_to_bgr != -1)
        cv::cvtColor(mat, mat, cvt_to_bgr);
    m.scene_image.setPixmap(QPixmap::fromImage(QImage(mat.data, mat.cols, mat.rows, mat.step, QImage::Format_BGR888)));
    m.view_image.fitInView(&m.scene_image.m_image, Qt::KeepAspectRatio);
}


GrabCutWindow* GrabCutWindow::create(SubWindow* subwindow)
{
    if (subwindow == nullptr) {
        QMessageBox::warning(nullptr, "GrabCut", "Brak wybranego obrazu");
        return nullptr;
    }
    return new GrabCutWindow(subwindow);
}


GrabCutWindow::GrabCutWindow(SubWindow* subwindow)
    : m(std::make_unique<Data>())
{
    setAttribute(Qt::WA_DeleteOnClose);
    setWindowTitle("GrabCut");
    m->self = this;

    m->combo_subwindow.setModel(SubWindowModel::get());
    m->layout.addWidget(&m->combo_subwindow);

    m->view_image.setScene(&m->scene_image);
    m->view_image.setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m->view_image.setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m->layout.addWidget(&m->view_image);

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
        if (idx == -1)
            setSubwindow(*m, nullptr);
        else
            setSubwindow(*m, SubWindowModel::get()->getSubWindowAt(idx));
    });
    connect(&m->scene_image, &Scene::pointsChanged, this, [this] {
        updateView(*m);
    });
    connect(&m->button_apply, &QPushButton::clicked, this, [this] {
        updateView(*m, true);
        close();
    });
    connect(&m->button_cancel, &QPushButton::clicked, this, [this] {
        close();
    });

    if (subwindow != nullptr) {
        m->combo_subwindow.setCurrentIndex(SubWindowModel::get()->getIndexOfSubWindow(subwindow));
        setSubwindow(*m, subwindow);
    }
    show();
}


GrabCutWindow::~GrabCutWindow()
{
}


#include "GrabCutWindow.moc"
