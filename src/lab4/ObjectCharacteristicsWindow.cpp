#include "ObjectCharacteristicsWindow.h"

#include "../SubWindow.h"

#include <QVBoxLayout>
#include <QComboBox>
#include <QAbstractTableModel>
#include <QTableView>

#include <opencv2/imgproc.hpp>


/////////////////////////////////
/// ObjectCharacteristicsModel
/////////////////////////////////

struct ObjectCharacteristicsModel : QAbstractTableModel
{
    SubWindow* m_subwindow;
    struct Characteristics {
        cv::Moments moments;
        double area, perimeter, aspectRatio, extent, solidity, equivalent_diameter;
    };
    std::vector<Characteristics> m_characteristics;

    void updateCharacteristics() {
        cv::Mat mat = m_subwindow->getMat();
        const int cvt_to_bgr = m_subwindow->getCvtToBgr();
        if (cvt_to_bgr != -1)
            cv::cvtColor(mat, mat, cvt_to_bgr);
        cv::cvtColor(mat, mat, cv::COLOR_BGR2GRAY);
        cv::threshold(mat, mat, 0, 255, cv::THRESH_BINARY | cv::THRESH_OTSU);

        std::vector<std::vector<cv::Point>> contours;
        cv::findContours(mat, contours, cv::RETR_TREE, cv::CHAIN_APPROX_SIMPLE);

        beginResetModel();
        m_characteristics.clear();
        for (auto& contour : contours) {
            Characteristics c{};
            c.moments = cv::moments(contour);
            c.area = cv::contourArea(contour);
            c.perimeter = cv::arcLength(contour, true);
            cv::Rect2d rect = cv::boundingRect(contour);
            c.aspectRatio = rect.width / rect.height;
            c.extent = c.area / (rect.width * rect.height);
            std::vector<cv::Point> hull;
            cv::convexHull(contour, hull);
            c.solidity = c.area / cv::contourArea(hull);
            c.equivalent_diameter = std::sqrt(4*c.area / CV_PI);
            m_characteristics.push_back(c);
        }
        endResetModel();
    }

    void setSubwindow(SubWindow* subwindow) {
        SubWindow* old_subwindow = m_subwindow;
        m_subwindow = subwindow;

        if (old_subwindow != nullptr && old_subwindow != m_subwindow)
            disconnect(old_subwindow, nullptr, this, nullptr);

        if (m_subwindow == nullptr) {
            beginResetModel();
            m_characteristics.clear();
            endResetModel();
            return;
        }

        if (old_subwindow != m_subwindow)
            connect(m_subwindow, &SubWindow::matChanged, this, &ObjectCharacteristicsModel::updateCharacteristics);

        updateCharacteristics();
    }

    int rowCount(const QModelIndex&) const override {
        return (int)m_characteristics.size();
    }
    int columnCount(const QModelIndex&) const override {
        return 16;
    }
    QVariant data(const QModelIndex& index, int role) const override {
        if (role == Qt::TextAlignmentRole)
            return Qt::AlignCenter;
        if (role != Qt::DisplayRole)
            return {};
        const Characteristics& c = m_characteristics[index.row()];
        switch (index.column()) {
        case 0: return c.area;
        case 1: return c.perimeter;
        case 2: return c.aspectRatio;
        case 3: return c.extent;
        case 4: return c.solidity;
        case 5: return c.equivalent_diameter;
        case 6: return c.moments.m00;
        case 7: return c.moments.m10;
        case 8: return c.moments.m01;
        case 9: return c.moments.m20;
        case 10: return c.moments.m11;
        case 11: return c.moments.m02;
        case 12: return c.moments.m30;
        case 13: return c.moments.m21;
        case 14: return c.moments.m12;
        case 15: return c.moments.m03;
        default: return {};
        }
    }
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override {
        if (role != Qt::DisplayRole)
            return {};
        if (orientation == Qt::Vertical)
            return section + 1;
        switch (section) {
        case 0: return "Pole";
        case 1: return "Obwód";
        case 2: return "Proporcje";
        case 3: return "Extent";
        case 4: return "Solidity";
        case 5: return "equivalentDiameter";
        case 6: return "m00";
        case 7: return "m10";
        case 8: return "m01";
        case 9: return "m20";
        case 10: return "m11";
        case 11: return "m02";
        case 12: return "m30";
        case 13: return "m21";
        case 14: return "m12";
        case 15: return "m03";
        default: return {};
        }
    }
    Qt::ItemFlags flags(const QModelIndex&) const override {
        if (m_subwindow == nullptr)
            return Qt::NoItemFlags;
        return Qt::ItemIsEnabled;
    }
};



//////////////////////////////////
/// ObjectCharacteristicsWindow
//////////////////////////////////

struct ObjectCharacteristicsWindow::Data
{
    QVBoxLayout layout;
    QComboBox combo_subwindow;
    ObjectCharacteristicsModel model;
    QTableView view;
};


ObjectCharacteristicsWindow* ObjectCharacteristicsWindow::create(struct SubWindow* subwindow)
{
    return new ObjectCharacteristicsWindow(subwindow);
}


ObjectCharacteristicsWindow::ObjectCharacteristicsWindow(SubWindow* init_subwindow)
    : m(std::make_unique<Data>())
{
    setAttribute(Qt::WA_DeleteOnClose);
    setWindowTitle("Cechy obiektów");

    m->layout.addWidget(&m->combo_subwindow, 0, Qt::AlignTop);

    m->view.setModel(&m->model);
    m->layout.addWidget(&m->view);

    setLayout(&m->layout);

    connect(&m->combo_subwindow, &QComboBox::currentIndexChanged, this, [this] (int idx) {
        if (idx == -1)
            m->model.setSubwindow(nullptr);
        else
            m->model.setSubwindow(SubWindowModel::get()->getSubWindowAt(idx));
    });
    m->combo_subwindow.setModel(SubWindowModel::get());

    connect(&m->model, &QAbstractItemModel::modelReset, this, [this] {
        m->view.resizeColumnsToContents();
    });

    if (init_subwindow != nullptr)
        m->combo_subwindow.setCurrentIndex(SubWindowModel::get()->getIndexOfSubWindow(init_subwindow));
    m->view.resizeColumnsToContents();
    show();
}


ObjectCharacteristicsWindow::~ObjectCharacteristicsWindow()
{
}
