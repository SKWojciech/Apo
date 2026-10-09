#include "InpaintWindow.h"

#include "../SubWindow.h"
#include "../MyGraphicsView.h"

#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>
#include <QGraphicsView>
#include <QGraphicsItem>
#include <QGraphicsSceneEvent>

#include <opencv2/photo.hpp>


struct InpaintWindow::Scene : QGraphicsScene
{
    Q_OBJECT
public:
    SubWindow* m_subwindow;
    QGraphicsPixmapItem m_item_input;
    cv::Mat m_input;
    QGraphicsPixmapItem m_item_mask;
    cv::Mat m_mask;
    QPoint m_last_point;
    bool is_drawing = false;

    Scene() {
        m_item_input.setPixmap({});
        m_item_input.setPos(0, 0);
        addItem(&m_item_input);
        m_item_mask.setPixmap({});
        m_item_mask.setPos(0, 0);
        addItem(&m_item_mask);
    }

    ~Scene() override {
        if (m_subwindow)
            m_subwindow->removePreview();
    }

    void updateOutput(bool apply = false) {
        if (m_item_input.pixmap().isNull())
            return;

        cv::Mat output;
        cv::inpaint(m_input, m_mask, output, 3, cv::INPAINT_TELEA);

        if (apply) {
            m_subwindow->setMat(output, -1);
            return;
        }

        m_subwindow->setPreview(output, -1);
    }

    void updateMat() {
        m_input = m_subwindow->getMat();
        if (m_subwindow->getCvtToBgr() != -1)
            cv::cvtColor(m_input, m_input, m_subwindow->getCvtToBgr());
        m_item_input.setPixmap(QPixmap::fromImage(QImage(m_input.data, m_input.cols, m_input.rows, m_input.step, QImage::Format_BGR888)));
        m_mask = cv::Mat::zeros(m_input.rows, m_input.cols, CV_8UC1);
        m_item_mask.setPixmap({});
        updateOutput();
        emit matChanged();
    }

    void setSubwindow(SubWindow* subwindow) {
        SubWindow* old_subwindow = m_subwindow;
        m_subwindow = subwindow;

        if (old_subwindow != nullptr && old_subwindow != m_subwindow)
            disconnect(old_subwindow, nullptr, this, nullptr);

        if (m_subwindow == nullptr) {
            m_item_input.setPixmap({});
            m_item_mask.setPixmap({});
            return;
        }

        if (old_subwindow != m_subwindow) {
            connect(m_subwindow, &SubWindow::matChanged, this, &Scene::updateMat);
        }

        updateMat();
    }

    void updateLine(QPoint from, QPoint to) {
        if (m_input.empty())
            return;
        auto [x0, y0] = from;
        auto [x1, y1] = to;
        x0 = std::clamp(x0, 0, m_input.cols - 1);
        x1 = std::clamp(x1, 0, m_input.cols - 1);
        y0 = std::clamp(y0, 0, m_input.rows - 1);
        y1 = std::clamp(y1, 0, m_input.rows - 1);
        cv::line(m_mask, cv::Point(x0, y0), cv::Point(x1, y1), cv::Scalar(255), 3);
        m_item_mask.setPixmap(QPixmap::fromImage(QImage(m_mask.data, m_mask.cols, m_mask.rows, m_mask.step, QImage::Format_Alpha8)));
        updateOutput();
    }

protected:
    void mousePressEvent(QGraphicsSceneMouseEvent* event) override {
        if (event->button() == Qt::LeftButton) {
            m_last_point = event->scenePos().toPoint();
            is_drawing = true;
        }
    }

    void mouseMoveEvent(QGraphicsSceneMouseEvent* event) override {
        if ((event->buttons() & Qt::LeftButton) && is_drawing) {
            updateLine(m_last_point, event->scenePos().toPoint());
            if (not (event->buttons() & Qt::RightButton))
                m_last_point = event->scenePos().toPoint();
        }
    }

    void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override {
        if ((event->buttons() & Qt::LeftButton) && is_drawing) {
            updateLine(m_last_point, event->scenePos().toPoint());
            is_drawing = false;
        }
    }

Q_SIGNALS:
    void matChanged();
};



struct InpaintWindow::Data
{
    QVBoxLayout layout;
    Scene scene;
    MyGraphicsView view_image;

    QHBoxLayout layout_buttons;
    QPushButton button_apply;
    QPushButton button_cancel;
};


InpaintWindow* InpaintWindow::create(SubWindow* subwindow)
{
    if (subwindow == nullptr) {
        QMessageBox::warning(nullptr, "Inpaint", "Brak wybranego obrazu");
        return nullptr;
    }
    return new InpaintWindow(subwindow);
}


InpaintWindow::InpaintWindow(SubWindow* subwindow)
    : m(std::make_unique<Data>())
{
    assert(subwindow != nullptr);
    setWindowTitle("Inpaint");
    setAttribute(Qt::WA_DeleteOnClose);

    m->scene.setSubwindow(subwindow);
    m->view_image.setScene(&m->scene);
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

    connect(&m->scene, &Scene::matChanged, this, [this] {
        m->view_image.fitInView(&m->scene.m_item_input, Qt::KeepAspectRatio);
    });
    connect(subwindow, &SubWindow::closing, this, [this] {
        m->scene.setSubwindow(nullptr);
        close();
    });
    connect(&m->button_apply, &QPushButton::clicked, this, [this] {
        m->scene.updateOutput(true);
        close();
    });
    connect(&m->button_cancel, &QPushButton::clicked, this, [this] {
        close();
    });

    m->view_image.fitInView(&m->scene.m_item_input, Qt::KeepAspectRatio);
    show();
}


InpaintWindow::~InpaintWindow()
{
}


#include "InpaintWindow.moc"
