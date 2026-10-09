#include "HistogramWindow.h"
#include "../SubWindow.h"
#include <QBarSeries>
#include <QChart>
#include <QBarSet>
#include <QValueAxis>
#include <QVBoxLayout>
#include <QComboBox>
#include <QGraphicsView>
#include <QGraphicsLinearLayout>
#include <opencv2/opencv.hpp>



////////////////////
/// MultiChartView
////////////////////

struct MultiChartView : QGraphicsView
{
    explicit MultiChartView(QWidget* parent = nullptr);
    void addChart(QChart* chart);
    void clear();
    void resize();

    QGraphicsScene m_scene{};
    std::vector<QChart*> m_charts{};

protected:
    void resizeEvent(QResizeEvent* event) override;
};


MultiChartView::MultiChartView(QWidget* parent)
    : QGraphicsView(parent)
{
    setFrameShape(QFrame::NoFrame);
    setBackgroundRole(QPalette::Window);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
    setScene(&m_scene);
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
}


void MultiChartView::addChart(QChart* chart)
{
    assert(chart != nullptr);
    float y = 0, h = 0;
    if (m_charts.size() > 0) {
        y = m_charts.back()->scenePos().y();
        h = m_charts.back()->size().height();
    }
    chart->setPos(0, y + h);
    m_charts.push_back(chart);
    m_scene.addItem(chart);
    resize();
}


void MultiChartView::clear()
{
    m_charts.clear();
    m_scene.clear();
    resize();
}


void MultiChartView::resize()
{
    resetTransform();
    int w = width();
    qreal min_h = INFINITY;
    for (auto chart : m_charts) {
        chart->resize(w, 200);
        min_h = std::min(chart->minimumHeight(), min_h);
    }
    if (min_h != INFINITY)
        setMinimumHeight(min_h);
    setSceneRect(0, 0, w, 200 * m_charts.size());
}


void MultiChartView::resizeEvent(QResizeEvent* event)
{
    QGraphicsView::resizeEvent(event);
    resize();
}



/////////////////////////
/// HistogramWindow
/////////////////////////

struct HistogramWindow::Data
{
    HistogramWindow* self;
    QVBoxLayout layout;
    QComboBox combo;
    MultiChartView view;
    SubWindow* subwindow;
    int cvt_to_bgr;
};


static void setSubWindow(HistogramWindow::Data& m, SubWindow* subwindow);


HistogramWindow* HistogramWindow::create(SubWindow* init_subwindow)
{
    return new HistogramWindow(init_subwindow);
}


HistogramWindow::HistogramWindow(SubWindow* init_subwindow)
    : m(std::make_unique<Data>())
{
    m->self = this;
    m->cvt_to_bgr = cv::COLOR_COLORCVT_MAX;
    setAttribute(Qt::WA_DeleteOnClose);
    setWindowTitle("Histogramy");
    resize(700, 400);

    m->combo.setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);
    m->layout.addWidget(&m->combo, 0, Qt::AlignTop);
    //m->view.setViewport(new QOpenGLWidget());
    m->layout.addWidget(&m->view, 1);
    setLayout(&m->layout);

    connect(&m->combo, &QComboBox::currentIndexChanged, this, [this](int idx) {
        if (idx >= 0)
            setSubWindow(*m, SubWindowModel::get()->getSubWindowAt(idx));
        else
            setSubWindow(*m, nullptr);
    });
    m->combo.setModel(SubWindowModel::get());

    if (init_subwindow != nullptr)
        m->combo.setCurrentIndex(SubWindowModel::get()->getIndexOfSubWindow(init_subwindow));

    show();
}


HistogramWindow::~HistogramWindow()
{
}


struct InitChartEntry {
    QString label;
    QColor color;
};
static void initCharts(MultiChartView* view, std::span<InitChartEntry> chart_init)
{
    view->clear();

    for (auto& entry : chart_init) {
        auto* set = new QBarSet(entry.label);
        set->setPen(QPen(Qt::NoPen));
        set->setColor(entry.color);
        auto* series = new QBarSeries();
        series->append(set);
        series->setBarWidth(1);
        auto* chart = new QChart();
        chart->addSeries(series);
        chart->layout()->setContentsMargins(0, 0, 0, 0);
        chart->setBackgroundRoundness(0);

        auto axisX = new QValueAxis();
        axisX->setLabelFormat("%.0f");
        axisX->setRange(0, 255);
        axisX->setGridLineVisible(false);
        chart->addAxis(axisX, Qt::AlignBottom);
        series->attachAxis(axisX);

        auto axisY = new QValueAxis();
        axisY->setLabelFormat("%.0f");
        axisY->setRange(0, 100);
        chart->addAxis(axisY, Qt::AlignLeft);
        series->attachAxis(axisY);

        view->addChart(chart);
    }

    view->resize();
}


struct MeanAndStdDev {
    [[maybe_unused]] qreal mean;
    [[maybe_unused]] qreal std_dev;
};
static MeanAndStdDev calcMeanAndStdDev(cv::Mat mat)
{
    assert(mat.channels() == 1 && mat.elemSize() == 1);

    double mean = 0;
    for (const unsigned char* p = mat.datastart; p != mat.dataend; p++)
        mean += *p;
    mean /= (double)mat.total();

    std::vector<qreal> vec(mat.total());
    for (int i = 0; qreal& val : vec)
        val = std::pow(mean - mat.at<unsigned char>(i++), 2);

    double std_dev = 0;
    for (const qreal x : vec)
        std_dev += x;
    std_dev = std::sqrt(std_dev / (double)mat.total());

    return {mean, std_dev};
}


static void updateHistograms(HistogramWindow::Data& m)
{
    cv::Mat image = m.subwindow->getMat();

    if (image.channels() != m.view.m_charts.size() || m.cvt_to_bgr != m.subwindow->getCvtToBgr()) {
        m.cvt_to_bgr = m.subwindow->getCvtToBgr();
        if (m.cvt_to_bgr == cv::COLOR_GRAY2BGR) {
            InitChartEntry arr[] = {{"Jasność", QColorConstants::Black}};
            initCharts(&m.view, arr);
        }
        else if (m.cvt_to_bgr == -1) {
            InitChartEntry arr[] = {{"B", QColorConstants::Blue}, {"G", QColorConstants::Green}, {"R", QColorConstants::Red}};
            initCharts(&m.view, arr);
        }
        else {
            std::vector<InitChartEntry> entries(image.channels());
            int i = 1;
            for (auto& entry : entries) {
                entry.label = QString("Kanał %1").arg(i++);
                entry.color = QColorConstants::Black;
            }
            initCharts(&m.view, entries);
        }
    }

    std::vector<cv::Mat> mats(image.channels());
    cv::split(image, mats.data());

    for (int i = 0; i < mats.size(); i++) {
        if (i > m.view.m_charts.size())
            continue;
        cv::Mat& mat = mats[i];
        QList<qreal> hist(256, 0);
        for (unsigned char* it = mat.data; it != mat.dataend; it++)
            hist[*it] += 1;
        qreal y_max = 0;
        for (qreal value : hist)
            y_max = value > y_max ? value : y_max;

        QChart* chart = m.view.m_charts[i];
        assert(chart != nullptr);

        auto [mean, std_dev] = calcMeanAndStdDev(mat);
        chart->setTitle(QString("Średnia: %1, Odchylenie std: %2").arg(mean).arg(std_dev));

        QBarSet* set = static_cast<QBarSeries*>(chart->series().first())->barSets().first();
        set->remove(0, 256);
        set->append(hist);
        chart->axes(Qt::Vertical).first()->setRange(0, y_max);
    }

    m.view.resize();
}


static void setSubWindow(HistogramWindow::Data& m, SubWindow* subwindow)
{
    SubWindow* old_subwindow = m.subwindow;
    m.subwindow = subwindow;

    if (old_subwindow != nullptr && old_subwindow != m.subwindow)
        QWidget::disconnect(old_subwindow, nullptr, m.self, nullptr);

    if (m.subwindow == nullptr) {
        m.view.clear();
        return;
    }

    if (old_subwindow != m.subwindow)
        QWidget::connect(m.subwindow, &SubWindow::matChanged, m.self, [&m] { updateHistograms(m); });

    updateHistograms(m);
}
