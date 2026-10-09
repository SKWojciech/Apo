#include "LineProfileWindow.h"
#include <QMessageBox>
#include <QVBoxLayout>
#include <QComboBox>
#include <QLineSeries>
#include <QChart>
#include <QValueAxis>
#include <QGraphicsLineItem>
#include <QGraphicsLayout>
#include <QGraphicsSceneMouseEvent>
#include <opencv2/imgproc.hpp>
#include "../MyGraphicsView.h"
#include "../SubWindow.h"


static float dist(const QPointF& a, const QPointF& b)
{
    return std::sqrt(std::pow(a.x() - b.x(), 2) + std::pow(a.y() - b.y(), 2));
}


///////////////////////
/// InteractiveScene
///////////////////////

struct InteractiveScene : QGraphicsScene
{
    Q_OBJECT
public:
    QPointF m_p1, m_p2;
    QGraphicsPixmapItem m_image;
    QGraphicsLineItem m_line;
    
    InteractiveScene() {
        addItem(&m_image);
        m_line.setPen(QPen(QColorConstants::Red));
        addItem(&m_line);
    }
    void setPoints(QPointF a, QPointF b) {
        qreal max_w = m_image.pixmap().width() - 1;
        qreal max_h = m_image.pixmap().height() - 1;
        if (max_w > 0 && max_h > 0) {
            a.rx() = std::clamp(a.rx(), 0.0, max_w);
            a.ry() = std::clamp(a.ry(), 0.0, max_h);
            b.rx() = std::clamp(b.rx(), 0.0, max_w);
            b.ry() = std::clamp(b.ry(), 0.0, max_h);
            m_p1 = a;
            m_p2 = b;
        } else {
            m_p1 = m_p2 = {0, 0};
        }
        m_line.setLine(m_p1.x(), m_p1.y(), m_p2.x(), m_p2.y());
        emit pointsChanged();
    }
    std::array<QPointF, 2> getPoints() const {
        return {m_p1, m_p2};
    }
    void setPixmap(const QPixmap& pixmap) {
        const QRectF old_rect = m_image.pixmap().rect();
        m_image.setPixmap(pixmap);
        setSceneRect(m_image.boundingRect());
        if (m_p1 == m_p2 && m_p1 == QPointF{0, 0})
            setPoints({0, (qreal)pixmap.height() - 1}, {(qreal)pixmap.width() - 1, 0});
        else if (old_rect != pixmap.rect())
            setPoints(m_p1, m_p2);
    }
    QPixmap getPixmap() const {
        return m_image.pixmap();
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



////////////////////////
/// LineProfileWindow
////////////////////////

struct LineProfileWindow::Data
{
    LineProfileWindow* self;
    SubWindow* subwindow;
    int cvt_to_bgr = cv::COLOR_COLORCVT_MAX;

    QVBoxLayout layout;
    QComboBox combo_subwindow;

    InteractiveScene scene_image;
    MyGraphicsView view_image;

    QValueAxis* axis_x;
    QValueAxis* axis_y;
    QChart* chart;
    QGraphicsScene scene_chart;
    MyGraphicsView view_chart;
};


static void recreateSeries(LineProfileWindow::Data& m, int channels)
{
    assert(channels > 0);
    static constexpr std::array bgr_labels = {"Kanał B", "Kanał G", "Kanał R"};
    static constexpr std::array bgr_colors = {QColorConstants::Blue, QColorConstants::Green, QColorConstants::Red};

    m.chart->removeAllSeries();

    for (int i = 0; i < channels; i++) {
        auto* series = new QLineSeries();
        if (m.cvt_to_bgr == -1) {
            series->setName(bgr_labels[i]);
            series->setColor(bgr_colors[i]);
        } else if (m.cvt_to_bgr == cv::COLOR_GRAY2BGR) {
            series->setName("Jasność");
            series->setColor(QColorConstants::Black);
        } else {
            series->setName(QString("Kanał %1").arg(i));
        }
        m.chart->addSeries(series);
        series->attachAxis(m.axis_x);
        series->attachAxis(m.axis_y);
    }
}


static void updateView(LineProfileWindow::Data& m)
{
    if (m.subwindow == nullptr)
        return;

    const cv::Mat mat = m.subwindow->getMat();
    assert(mat.depth() == CV_8U);
    const int cvt_to_bgr = m.subwindow->getCvtToBgr();

    if (cvt_to_bgr != m.cvt_to_bgr) {
        m.cvt_to_bgr = cvt_to_bgr;
        recreateSeries(m, mat.channels());
    }

    std::vector<cv::Mat> channels;
    cv::split(mat, channels);
    std::vector<QList<QPointF>> points_list(mat.channels());

    auto [p1, p2] = m.scene_image.getPoints();
    auto [x0, y0] = p1.toPoint();
    auto [x1, y1] = p2.toPoint();

    int dx = std::abs(x1 - x0);
    int sx = x0 > x1 ? -1 : 1;
    int dy = std::abs(y1 - y0);
    int sy = y0 > y1 ? -1 : 1;
    int x = x0, y = y0;
    int i = 0;
    if (dx > dy) {
        float err = dx / 2.0;
        while (x != x1) {
            for (int cn = 0; cn < mat.channels(); cn++)
                points_list[cn].append({(qreal)i++, (qreal)channels[cn].at<uint8_t>(y, x)});
            err -= dy;
            if (err < 0) {
                y += sy;
                err += dx;
            }
            x += sx;
        }
    } else {
        float err = dy / 2.0;
        while (y != y1) {
            for (int cn = 0; cn < mat.channels(); cn++)
                points_list[cn].append({(qreal)i++, (qreal)channels[cn].at<uint8_t>(y, x)});
            err -= dx;
            if (err < 0) {
                x += sx;
                err += dy;
            }
            y += sy;
        }
    }

    for (int cn = 0; cn < mat.channels(); cn++)
        static_cast<QLineSeries*>(m.chart->series()[cn])->replace(points_list[cn]);
    m.axis_x->setRange(0, i - 1);
}


static void setSubwindow(LineProfileWindow::Data& m, SubWindow* subwindow)
{
    SubWindow* old_subwindow = m.subwindow;
    m.subwindow = subwindow;
    if (old_subwindow != nullptr && m.subwindow != old_subwindow)
        QWidget::disconnect(old_subwindow, nullptr, m.self, nullptr);

    if (m.subwindow == nullptr) {
        m.scene_image.setPixmap(QPixmap());
        m.chart->removeAllSeries();
        m.cvt_to_bgr = cv::COLOR_COLORCVT_MAX;
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

/*
    m.chart->resize(m.view_chart.rect().size());
    m.scene_chart.setSceneRect(m.chart->boundingRect());
    m.view_chart.setSceneRect(m.chart->boundingRect());
    m.view_chart.fitInView(m.chart, Qt::IgnoreAspectRatio);*/
}


LineProfileWindow* LineProfileWindow::create(SubWindow* subwindow)
{
    return new LineProfileWindow(subwindow);
}


LineProfileWindow::LineProfileWindow(SubWindow* subwindow)
    : m(std::make_unique<Data>())
{
    setAttribute(Qt::WA_DeleteOnClose);
    setWindowTitle("Linia profilu");
    m->self = this;

    m->combo_subwindow.setModel(SubWindowModel::get());
    m->layout.addWidget(&m->combo_subwindow);

    m->view_image.setScene(&m->scene_image);
    m->view_image.setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m->view_image.setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m->layout.addWidget(&m->view_image);

    m->chart = new QChart();
    m->chart->layout()->setContentsMargins(0, 0, 0, 0);
    m->chart->setBackgroundRoundness(0);
    m->axis_x = new QValueAxis();
    m->axis_x->setLabelFormat("%.0f");
    m->chart->addAxis(m->axis_x, Qt::AlignBottom);
    m->axis_y = new QValueAxis();
    m->axis_y->setLabelFormat("%.0f");
    m->axis_y->setRange(0, 255);
    m->chart->addAxis(m->axis_y, Qt::AlignLeft);
    m->scene_chart.addItem(m->chart);
    m->view_chart.setScene(&m->scene_chart);
    m->view_chart.fitInView(m->chart, Qt::IgnoreAspectRatio);
    m->view_chart.setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m->view_chart.setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m->view_chart.setMinimumHeight(250);
    m->layout.addWidget(&m->view_chart);

    setLayout(&m->layout);

    connect(&m->combo_subwindow, &QComboBox::currentIndexChanged, this, [this] (int idx) {
        if (idx == -1)
            setSubwindow(*m, nullptr);
        else
            setSubwindow(*m, SubWindowModel::get()->getSubWindowAt(idx));
    });
    connect(&m->scene_image, &InteractiveScene::pointsChanged, this, [this] {
        updateView(*m);
    });


    if (subwindow != nullptr) {
        m->combo_subwindow.setCurrentIndex(SubWindowModel::get()->getIndexOfSubWindow(subwindow));
        setSubwindow(*m, subwindow);
    }
    show();
    resize(500, 700);
}


void LineProfileWindow::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    m->chart->resize(m->view_chart.rect().size());
    m->scene_chart.setSceneRect(m->chart->boundingRect());
    m->view_chart.setSceneRect(m->chart->boundingRect());
    m->view_chart.fitInView(m->chart, Qt::IgnoreAspectRatio);

    m->view_image.fitInView(&m->scene_image.m_image, Qt::KeepAspectRatio);
}


LineProfileWindow::~LineProfileWindow()
{
}


#include "LineProfileWindow.moc"
