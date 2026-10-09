#include "SubWindow.h"

#include "MyGraphicsView.h"

#include <QGraphicsPixmapItem>
#include <QGridLayout>
#include <QAbstractListModel>
#include <QApplication>
#include <QStyleHints>

#include <opencv2/imgproc.hpp>

#include <numbers>


///////////////////
/// SubWindowModel
///////////////////

static QIcon g_default_icon{};
static QIcon g_selected_icon{};



SubWindowModel* SubWindowModel::get()
{
    static SubWindowModel model{};
    return &model;
}


SubWindowModel::SubWindowModel()
{
    QImage default_img(":/icon.png");
    QImage selected_img(":/selected.png");
    if (qApp->styleHints()->colorScheme() == Qt::ColorScheme::Dark) {
        default_img.invertPixels();
        selected_img.invertPixels();
    }
    g_default_icon = QIcon(QPixmap::fromImage(std::move(default_img)));
    g_selected_icon = QIcon(QPixmap::fromImage(std::move(selected_img)));
}


QVariant SubWindowModel::data(const QModelIndex& index, int role) const
{
    if (index.column() != 0 || role != Qt::DisplayRole)
        return {};
    int i = index.row();
    if (i < 0 || i >= m_subwindows.size())
        return {};
    return m_subwindows[i].title;
}


int SubWindowModel::rowCount(const QModelIndex&) const
{
    return (int)m_subwindows.size();
}


void SubWindowModel::setCurrentSubWindow(SubWindow* window)
{
    SubWindow* old_subwindow = m_current_subwindow;
    m_current_subwindow = window;

    if (old_subwindow == m_current_subwindow)
        return;

    if (old_subwindow != nullptr)
        old_subwindow->setWindowIcon(g_default_icon);

    if (m_current_subwindow != nullptr)
        m_current_subwindow->setWindowIcon(g_selected_icon);

    emit currentSubWindowChanged(m_current_subwindow);
}


int SubWindowModel::getIndexOfSubWindow(const SubWindow* window)
{
    if (window == nullptr)
        return -1;
    for (int i = 0; i < m_subwindows.size(); i++)
        if (m_subwindows[i].window == window)
            return i;
    return -1;
}


SubWindow* SubWindowModel::getSubWindowAt(int index)
{
    assert(index >= 0 && index < m_subwindows.size());
    return m_subwindows[index].window;
}


SubWindow* SubWindowModel::getSubWindowWithTitle(const QString& title)
{
    auto it = std::ranges::find(m_subwindows, title, [](const auto& entry) { return entry.title; });
    if (it == m_subwindows.end())
        return nullptr;
    return it->window;
}


SubWindow* SubWindowModel::getCurrentSubWindow()
{
    return m_current_subwindow;
}


void SubWindowModel::addSubWindowAndModifyTitle(SubWindow* window, QString& title)
{
    if (title.isEmpty())
        title = "Bez nazwy";
    size_t duplicate_number_idx = title.size() + 2;
    int duplicate_number = 0;
    for (const auto& e : m_subwindows) {
        if (e.title != title)
            continue;
        title.truncate(duplicate_number_idx);
        if (duplicate_number == 0)
            title.append(" (");
        title.append(QString::number(++duplicate_number)).append(')');
    }

    // Tytuły muszą być posortowane, inaczej algorytm szukania duplikatów powyżej nie działa
    int target_idx = 0;
    while (target_idx < m_subwindows.size()) {
        if (m_subwindows[target_idx].title >= title)
            break;
        target_idx++;
    }

    beginInsertRows(QModelIndex(), target_idx, target_idx);
    m_subwindows.insert(m_subwindows.begin() + target_idx, {window, title});
    endInsertRows();

    setCurrentSubWindow(window);
}


void SubWindowModel::removeSubWindow(SubWindow* window)
{
    auto it = std::ranges::find(m_subwindows, window, [](const auto& el){ return el.window; });
    assert(it != m_subwindows.end());
    auto idx = it - m_subwindows.begin();
    beginRemoveRows(QModelIndex(), idx, idx);
    m_subwindows.erase(it);
    if (m_current_subwindow == window)
        setCurrentSubWindow(m_subwindows.empty() ? nullptr : m_subwindows.back().window);
    endRemoveRows();
}


/////////////////////////////
/// SubWindowChannelModel
/////////////////////////////

SubWindowChannelModel::SubWindowChannelModel(SubWindow* subwindow)
    : m_subwindow(nullptr), m_cvt_to_bgr(cv::COLOR_COLORCVT_MAX)
{
    setSubWindow(subwindow);
}


int SubWindowChannelModel::rowCount(const QModelIndex&) const
{
    return m_channels;
}


QVariant SubWindowChannelModel::data(const QModelIndex& index, int role) const
{
    if (role != Qt::DisplayRole)
        return {};

    if (m_cvt_to_bgr == cv::COLOR_COLORCVT_MAX)
        return {};

    if (m_cvt_to_bgr == cv::COLOR_GRAY2BGR)
        return "Jasność";

    if (m_cvt_to_bgr == -1) {
        switch (index.row()) {
        case 0: return "Kanał B";
        case 1: return "Kanał G";
        case 2: return "Kanał R";
        default: return {};
        }
    }

    return QString("Kanał %1").arg(index.row());
}


void SubWindowChannelModel::updateChannels()
{
    if (m_subwindow == nullptr) {
        beginResetModel();
        m_channels = 0;
        m_cvt_to_bgr = cv::COLOR_COLORCVT_MAX;
        endResetModel();
        return;
    }

    beginResetModel();
    m_channels = m_subwindow->getMat().channels();
    m_cvt_to_bgr = m_subwindow->getCvtToBgr();
    endResetModel();
}


void SubWindowChannelModel::setSubWindow(SubWindow* subwindow)
{
    SubWindow* old_subwindow = m_subwindow;
    m_subwindow = subwindow;

    if (old_subwindow != nullptr && old_subwindow != m_subwindow)
        disconnect(old_subwindow, nullptr, this, nullptr);

    if (m_subwindow == nullptr) {
        updateChannels();
        return;
    }

    if (old_subwindow != m_subwindow) {
        connect(m_subwindow, &SubWindow::matChanged, this, [this] { updateChannels(); });
        connect(m_subwindow, &SubWindow::closing, this, [this] { setSubWindow(nullptr); });
    }

    updateChannels();
}



///////////////////
/// SubWindow
///////////////////

struct SubWindow::Data
{
    SubWindow* self;
    QString title;
    QPixmap pixmap;
    cv::Mat mat;
    int cvt_to_bgr;

    QVBoxLayout layout;
    QGraphicsScene scene;
    MyGraphicsView view;
    QGraphicsPixmapItem item;
};


static void init(SubWindow::Data& m)
{
    m.self->setAttribute(Qt::WA_DeleteOnClose);
    m.self->setFocusPolicy(Qt::StrongFocus);
    m.self->setMinimumSize(200, 200);

    //auto* menu_fit = m.menubar.addMenu("Dopasuj");
    //menu_fit->addAction("okno do obrazu", m.self, [&m]{ fitWindowToImage(m); });
    //menu_fit->addAction("obraz do okna", m.self, [&m]{ fitImageToWindow(m); });

    //auto* menu_edit = m.menubar.addMenu("Edytuj");
    //m.layout.addWidget(&m.menubar);

    m.item.setPixmap(m.pixmap);
    m.item.setPos(0, 0);
    m.scene.setSceneRect(0, 0, m.pixmap.width(), m.pixmap.height());
    m.scene.addItem(&m.item);
    m.view.setSceneRect(0, 0, m.pixmap.width(), m.pixmap.height());
    m.view.setLineWidth(0);
    m.view.setFrameShape(QFrame::Box);
    m.view.setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m.view.setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m.view.setContentsMargins(0, 0, 0, 0);
    m.view.setScene(&m.scene);
    m.view.fitInView(&m.item, Qt::KeepAspectRatio);
    m.view.resize(m.pixmap.width(), m.pixmap.height());
    m.layout.addWidget(&m.view, 1);
    m.self->setLayout(&m.layout);

    SubWindowModel::get()->addSubWindowAndModifyTitle(m.self, m.title);
    m.self->setWindowTitle(m.title);
}


SubWindow::SubWindow(const QString& filename)
    : m(std::make_unique<Data>())
{
    m->self = this;
    QImage bgr_image = QImage(filename).convertToFormat(QImage::Format_BGR888);
    m->pixmap = QPixmap::fromImage(bgr_image).copy();
    m->mat = cv::Mat(bgr_image.height(), bgr_image.width(), CV_8UC3, bgr_image.bits(), bgr_image.bytesPerLine()).clone();
    m->cvt_to_bgr = -1;
    m->title = filename;
    const auto idx = std::max(m->title.lastIndexOf('/'), m->title.lastIndexOf('\\'));
    m->title.remove(0, idx > 0 ? idx + 1 : 0);

    init(*m);
}


SubWindow::SubWindow(const QImage& image, const QString& name)
    : m(std::make_unique<Data>())
{
    m->self = this;
    QImage bgr_image = image.convertToFormat(QImage::Format_BGR888);
    m->pixmap = QPixmap::fromImage(image).copy();
    m->mat = cv::Mat(bgr_image.height(), bgr_image.width(), CV_8UC3, bgr_image.bits(), bgr_image.bytesPerLine()).clone();
    m->cvt_to_bgr = -1;
    m->title = name;

    init(*m);
}


SubWindow::SubWindow(const cv::Mat& mat, int cvt_to_bgr, const QString& name)
    : m(std::make_unique<Data>())
{
    m->self = this;
    setMat(mat, cvt_to_bgr);
    m->title = name;

    init(*m);
}


SubWindow::~SubWindow()
{
    SubWindowModel::get()->removeSubWindow(this);
}


QImage SubWindow::getImage() const
{
    QImage image;
    if (m->cvt_to_bgr != -1) {
        cv::Mat mat_bgr;
        cv::cvtColor(m->mat, mat_bgr, m->cvt_to_bgr);
        image = QImage(mat_bgr.data, mat_bgr.cols, mat_bgr.rows, mat_bgr.step, QImage::Format_BGR888).copy();
    }
    else {
        image = QImage(m->mat.data, m->mat.cols, m->mat.rows, m->mat.step, QImage::Format_BGR888);
    }
    return image;
}


const cv::Mat SubWindow::getMat() const
{
    return m->mat;
}


void SubWindow::setMat(const cv::Mat& mat, int cvt_to_bgr)
{
    m->mat = mat.clone();
    m->cvt_to_bgr = cvt_to_bgr;

    // Skonwertuj mat na znany format (BGR888)
    {
        cv::Mat mat_bgr;
        QImage image;
        if (m->cvt_to_bgr != -1) {
            cv::cvtColor(m->mat, mat_bgr, m->cvt_to_bgr);
            image = QImage(mat_bgr.data, mat_bgr.cols, mat_bgr.rows, mat_bgr.step, QImage::Format_BGR888);
        }
        else {
            image = QImage(m->mat.data, m->mat.cols, m->mat.rows, m->mat.step, QImage::Format_BGR888);
        }
        m->pixmap = QPixmap::fromImage(image).copy();
        m->item.setPixmap(m->pixmap);
    }
    m->scene.setSceneRect(0, 0, m->pixmap.width(), m->pixmap.height());
    m->view.setSceneRect(0, 0, m->pixmap.width(), m->pixmap.height());
    //m->self->adjustSize();
    m->view.fitInView(&m->item, Qt::KeepAspectRatio);
    emit matChanged(this);
}


int SubWindow::getCvtToBgr() const
{
    return m->cvt_to_bgr;
}


void SubWindow::setPreview(const QPixmap& pixmap)
{
    m->item.setPixmap(pixmap);
    m->scene.setSceneRect(0, 0, pixmap.width(), pixmap.height());
    m->view.setSceneRect(0, 0, pixmap.width(), pixmap.height());
    //m->self->adjustSize();
    m->view.fitInView(&m->item, Qt::KeepAspectRatio);
}

void SubWindow::setPreview(cv::Mat mat, int cvt_to_bgr)
{
    if (cvt_to_bgr != -1)
        cv::cvtColor(mat, mat, cvt_to_bgr);
    setPreview(QPixmap::fromImage(QImage(mat.data, mat.cols, mat.rows, mat.step, QImage::Format_BGR888)).copy());
}


void SubWindow::removePreview()
{
    if (m->item.pixmap().data_ptr() == m->pixmap.data_ptr())
        return;
    m->item.setPixmap(m->pixmap);
    m->scene.setSceneRect(0, 0, m->pixmap.width(), m->pixmap.height());
    m->view.setSceneRect(0, 0, m->pixmap.width(), m->pixmap.height());
    //m->self->adjustSize();
    m->view.fitInView(&m->item, Qt::KeepAspectRatio);
}


QString SubWindow::getTitle() const
{
    return m->title;
}


void SubWindow::focusInEvent(QFocusEvent* event)
{
    SubWindowModel::get()->setCurrentSubWindow(this);
    QWidget::focusInEvent(event);
}


void SubWindow::mousePressEvent(QMouseEvent* event)
{
    SubWindowModel::get()->setCurrentSubWindow(this);
    QWidget::mousePressEvent(event);
}


void SubWindow::resizeEvent(QResizeEvent* event)
{
    m->view.fitInView(&m->item, Qt::KeepAspectRatio);
    QWidget::resizeEvent(event);
}


void SubWindow::closeEvent(QCloseEvent* event)
{
    emit closing();
    QWidget::closeEvent(event);
}
