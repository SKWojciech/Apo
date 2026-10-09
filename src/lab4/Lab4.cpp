#include "Lab4.h"
#include "../SubWindow.h"
#include "ThresholdDialog.h"
#include "GrabCutWindow.h"
#include "ObjectCharacteristicsWindow.h"
#include "InpaintWindow.h"

#include <QMessageBox>

#include <opencv2/imgproc.hpp>



static void actionWatershed(SubWindow* subwindow)
{
    if (subwindow == nullptr) {
        QMessageBox::warning(nullptr, "Watershed", "Brak wybranego obrazu");
        return;
    }

    cv::Mat orig_mat = subwindow->getMat().clone();
    if (subwindow->getCvtToBgr() != -1)
        cv::cvtColor(orig_mat, orig_mat, subwindow->getCvtToBgr());

    cv::Mat mat = orig_mat;
    cv::cvtColor(mat, mat, cv::COLOR_BGR2GRAY);
    cv::threshold(mat, mat, 0, 255, cv::THRESH_BINARY_INV | cv::THRESH_OTSU);

    cv::Mat kernel = cv::Mat::ones(3, 3, CV_8UC1);
    cv::morphologyEx(mat, mat, cv::MORPH_OPEN, kernel, cv::Point(-1, -1), 2);

    cv::Mat background;
    cv::dilate(mat, background, kernel, cv::Point(-1, -1), 3);

    cv::Mat foreground;
    cv::distanceTransform(mat, foreground, cv::DIST_L2, 5);
    double max_val;
    cv::minMaxLoc(foreground, nullptr, &max_val);
    cv::threshold(foreground, foreground, 0.7*max_val, 255, 0);

    foreground.convertTo(foreground, CV_8U);
    background.convertTo(background, CV_8U);
    cv::Mat border;
    cv::subtract(background, foreground, border);

    cv::Mat markers;
    cv::connectedComponents(foreground, markers);
    markers += 1;
    markers.setTo(0, border == 255);

    cv::watershed(orig_mat, markers);

    orig_mat.setTo(cv::Scalar(255, 255, 255), markers == -1);
    subwindow->setMat(orig_mat, -1);
}


Lab4::Lab4()
    : QMenu("Lab4")
{
    addAction("Progowanie", this, [] {
        ThresholdDialog::create(SubWindowModel::get()->getCurrentSubWindow());
    });

    addAction("GrabCut", this, [] {
        GrabCutWindow::create(SubWindowModel::get()->getCurrentSubWindow());
    });

    addAction("Watershed", this, [] {
        actionWatershed(SubWindowModel::get()->getCurrentSubWindow());
    });

    addAction("Inpaint", this, [] {
        InpaintWindow::create(SubWindowModel::get()->getCurrentSubWindow());
    });

    addAction("Cechy obiektów", this, [] {
        ObjectCharacteristicsWindow::create(SubWindowModel::get()->getCurrentSubWindow());
    });
}
