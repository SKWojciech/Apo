#include "LUTEditWindow.h"
#include <QVBoxLayout>
#include <QComboBox>
#include <QPushButton>
#include <QAbstractTableModel>
#include <QMessageBox>
#include <QTableView>
#include <opencv2/core/mat.hpp>
#include <opencv2/imgproc.hpp>
#include "../SubWindow.h"



///////////////
/// LUTModel
///////////////

struct LUTModel final : QAbstractTableModel
{
    Q_OBJECT
public:
    std::array<uint8_t, 256> m_lut;
    std::array<size_t, 256> m_hist;

    LUTModel() {
        m_hist.fill(0);
        setToIdentity();
    }
    void setToIdentity() {
        for (int i = 0; i < 256; i++)
            m_lut[i] = i;
        dataChanged(index(0, 0), index(0, 255));
        emit lutChanged();
    }
    void updateHist(const cv::Mat& mat, const int channel) {
        const int channels = mat.channels();
        assert(channels > 0 && channel < channels);
        m_hist.fill(0);
        for (const uint8_t* p = mat.data + channel; p < mat.datalimit; p += channels)
            m_hist[*p] += 1;
        dataChanged(index(1, 0), index(1, 255));
    }
    int rowCount(const QModelIndex&) const override {
        return 2;
    }
    int columnCount(const QModelIndex&) const override {
        return 256;
    }
    QVariant data(const QModelIndex& index, int role) const override {
        if (role == Qt::TextAlignmentRole)
            return Qt::AlignCenter;
        if (role != Qt::DisplayRole)
            return {};
        if (index.row() == 0)
            return m_lut[index.column()];
        if (index.row() == 1)
            return m_hist[index.column()];
        return {};
    }
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override {
        if (role != Qt::DisplayRole)
            return {};
        if (orientation == Qt::Horizontal)
            return section;
        if (section == 0)
            return "Wartość";
        if (section == 1)
            return "Ilość";
        return {};
    }
    bool setData(const QModelIndex& index, const QVariant& value, int role) override {
        if (role != Qt::EditRole)
            return false;
        bool ok = false;
        int x = value.toInt(&ok);
        if (!ok || x < 0 || x > 255)
            return false;
        m_lut[index.column()] = x;
        emit lutChanged();
        return true;
    }
    Qt::ItemFlags flags(const QModelIndex& index) const override {
        if (index.row() == 0)
            return Qt::ItemIsEnabled | Qt::ItemIsEditable;
        return {};
    }

Q_SIGNALS:
    void lutChanged();
};


////////////////////
/// LUTEditWindow
////////////////////

struct LUTEditWindow::Data
{
    LUTEditWindow* self;
    SubWindow* subwindow;

    QVBoxLayout layout;
    QComboBox combo_subwindow;
    SubWindowChannelModel model_channel;
    QComboBox combo_channel;
    LUTModel model_lut;
    QTableView view;
    QHBoxLayout layout_buttons;
    QPushButton button_reset;
    QPushButton button_apply;
};


static void updatePreview(LUTEditWindow::Data& m, bool apply = false)
{
    const int channel = m.combo_channel.currentIndex();
    if (m.subwindow == nullptr || channel == -1)
        return;

    cv::Mat mat = m.subwindow->getMat().clone();
    const int channels = mat.channels();
    assert(channel < channels);

    for (auto* p = mat.data + channel; p < mat.datalimit; p += channels)
        *p = m.model_lut.m_lut[*p];

    if (apply) {
        m.subwindow->setMat(mat, m.subwindow->getCvtToBgr());
        return;
    }

    m.subwindow->setPreview(mat, m.subwindow->getCvtToBgr());
}


static void setSubWindow(LUTEditWindow::Data& m, SubWindow* subwindow)
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
            m.model_lut.updateHist(m.subwindow->getMat(), m.combo_channel.currentIndex());
            m.view.resizeColumnsToContents();
            updatePreview(m);
        });
    }

    m.model_channel.setSubWindow(m.subwindow);
}


LUTEditWindow* LUTEditWindow::create(SubWindow* subwindow)
{
    if (subwindow == nullptr) {
        QMessageBox::warning(nullptr, "Edycja LUT", "Brak wybranego obrazu");
        return nullptr;
    }
    return new LUTEditWindow(subwindow);
}


LUTEditWindow::LUTEditWindow(SubWindow* subwindow)
    : m (std::make_unique<Data>())
{
    setAttribute(Qt::WA_DeleteOnClose, true);
    setWindowTitle("Edycja LUT");
    m->self = this;
    assert(subwindow != nullptr);
    assert(subwindow->getMat().channels() > 0);

    m->combo_subwindow.setModel(SubWindowModel::get());
    m->combo_subwindow.setCurrentIndex(SubWindowModel::get()->getIndexOfSubWindow(subwindow));
    m->layout.addWidget(&m->combo_subwindow, 0, Qt::AlignTop);

    m->combo_channel.setModel(&m->model_channel);
    m->layout.addWidget(&m->combo_channel, 0, Qt::AlignTop);

    m->view.setModel(&m->model_lut);
    m->layout.addWidget(&m->view, 1);

    m->layout_buttons.addStretch(1);
    m->button_reset.setText("Reset");
    m->layout_buttons.addWidget(&m->button_reset, 0, Qt::AlignBottom);
    m->button_apply.setText("Zastosuj");
    m->layout_buttons.addWidget(&m->button_apply, 0, Qt::AlignBottom);
    m->layout.addLayout(&m->layout_buttons, 0);

    setLayout(&m->layout);

    connect(&m->combo_subwindow, &QComboBox::currentIndexChanged, this, [this] (int idx) {
        if (idx == -1)
            close();
        else
            setSubWindow(*m, SubWindowModel::get()->getSubWindowAt(idx));
    });
    connect(&m->combo_channel, &QComboBox::currentIndexChanged, this, [this] (int idx) {
        if (idx == -1)
            return;
        m->model_lut.setToIdentity();
        m->model_lut.updateHist(m->subwindow->getMat(), idx);
        m->view.resizeColumnsToContents();
    });
    connect(&m->model_lut, &LUTModel::lutChanged, this, [this] {
        updatePreview(*m);
    });
    connect(&m->button_apply, &QPushButton::clicked, this, [this] {
        updatePreview(*m, true);
        m->subwindow = nullptr;
        close();
    });
    connect(&m->button_reset, &QPushButton::clicked, this, [this] {
        m->model_lut.setToIdentity();
    });

    setSubWindow(*m, subwindow);
    resize(800, 300);
    m->view.resizeColumnsToContents();
    show();
}


void LUTEditWindow::closeEvent(QCloseEvent* event)
{
    if (m->subwindow)
        m->subwindow->removePreview();
    QWidget::closeEvent(event);
}


LUTEditWindow::~LUTEditWindow()
{
}



#include "LUTEditWindow.moc"
