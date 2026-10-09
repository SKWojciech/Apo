#include "HistogramTableWindow.h"
#include <QComboBox>
#include <QVBoxLayout>
#include <QAbstractTableModel>
#include <QTableWidget>
#include <opencv2/core/mat.hpp>
#include <opencv2/imgproc.hpp>
#include "../SubWindow.h"



/////////////////////////
/// HistTableModel
/////////////////////////

struct HistTableModel : QAbstractTableModel
{
    SubWindow* m_subwindow;
    std::vector<std::array<size_t, 256>> m_hists{};

    void updateHists() {
        const cv::Mat mat = m_subwindow->getMat().clone();
        int channels = mat.channels();

        std::vector<std::array<size_t, 256>> hists{};
        hists.resize(channels);

        int channel = 0;
        for (const uchar* p = mat.datastart; p != mat.dataend; p++) {
            hists[channel][*p] += 1;
            channel = (channel + 1) % channels;
        }

        beginResetModel();
        m_hists = std::move(hists);
        endResetModel();
    }

    void setSubWindow(SubWindow* subwindow) {
        SubWindow* old_subwindow = m_subwindow;
        m_subwindow = subwindow;

        if (old_subwindow != nullptr && old_subwindow != m_subwindow)
            disconnect(old_subwindow, nullptr, this, nullptr);

        if (m_subwindow == nullptr) {
            beginResetModel();
            m_hists.clear();
            endResetModel();
            return;
        }

        if (old_subwindow != m_subwindow)
            connect(m_subwindow, &SubWindow::matChanged, this, &HistTableModel::updateHists);

        updateHists();
    }

    HistTableModel(SubWindow* subwindow = nullptr) {
        setSubWindow(subwindow);
    }

    int rowCount(const QModelIndex&) const override {
        return (int)m_hists.size();
    }
    int columnCount(const QModelIndex&) const override {
        return 256;
    }
    QVariant data(const QModelIndex& index, int role) const override {
        if (role == Qt::TextAlignmentRole)
            return Qt::AlignCenter;
        if (role != Qt::DisplayRole)
            return {};
        return m_hists[index.row()][index.column()];
    }
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override {
        if (role != Qt::DisplayRole)
            return {};
        if (orientation == Qt::Horizontal)
            return section;
        if (m_subwindow->getCvtToBgr() == cv::COLOR_GRAY2BGR)
            return "Jasność";
        if (m_subwindow->getCvtToBgr() == -1) {
            switch (section) {
            case 0: return "B";
            case 1: return "G";
            case 2: return "R";
            default: return {};
            }
        }
        return section;
    }
    Qt::ItemFlags flags(const QModelIndex&) const override {
        if (m_subwindow == nullptr)
            return Qt::NoItemFlags;
        return Qt::ItemIsEnabled;
    }
};


/////////////////////////
/// HistogramTableWindow
/////////////////////////

struct HistogramTableWindow::Data
{
    HistTableModel model;
    QVBoxLayout layout;
    QComboBox combo;
    QTableView view;
};


HistogramTableWindow* HistogramTableWindow::create(SubWindow* init_subwindow)
{
    return new HistogramTableWindow(init_subwindow);
}


HistogramTableWindow::HistogramTableWindow(SubWindow* init_subwindow)
    : m(std::make_unique<Data>())
{
    setAttribute(Qt::WA_DeleteOnClose);
    setWindowTitle("Tablica wartości kanałów");
    resize(800, 300);

    m->combo.setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);
    m->layout.addWidget(&m->combo, 0, Qt::AlignTop);
    m->view.setModel(&m->model);
    m->layout.addWidget(&m->view);
    setLayout(&m->layout);

    connect(&m->combo, &QComboBox::currentIndexChanged, this, [this] (int idx) {
        if (idx >= 0)
            m->model.setSubWindow(SubWindowModel::get()->getSubWindowAt(idx));
        else
            m->model.setSubWindow(nullptr);
    });
    m->combo.setModel(SubWindowModel::get());

    connect(&m->model, &QAbstractItemModel::modelReset, this, [this] {
        m->view.resizeColumnsToContents();
    });

    if (init_subwindow != nullptr)
        m->combo.setCurrentIndex(SubWindowModel::get()->getIndexOfSubWindow(init_subwindow));

    m->view.resizeColumnsToContents();
    show();
}


HistogramTableWindow::~HistogramTableWindow()
{
}
