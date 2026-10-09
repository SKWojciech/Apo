#include "Lab1.h"

#include "../SubWindow.h"
#include "HistogramWindow.h"
#include "HistogramTableWindow.h"
#include "HistogramStretchWindow.h"
#include "SelectiveStretchWindow.h"
#include "LUTEditWindow.h"
#include "BinarizeDialog.h"
#include "EqualizeDialog.h"

#include <QMessageBox>

#include <opencv2/imgproc.hpp>



static void actionScale(double scale)
{
    auto* subwindow = SubWindowModel::get()->getCurrentSubWindow();
    if (!subwindow) {
        QMessageBox::warning(nullptr, "Skalowanie obrazu", "Brak wybranego obrazu");
        return;
    }

    cv::Mat mat = subwindow->getMat();

    if (scale <= 0) {
        auto* dialog = new QInputDialog();
        dialog->setWindowTitle("Podaj wartość skalowania");
        dialog->setLabelText("Skaluj o:");
        dialog->setCancelButtonText("Anuluj");
        dialog->setOkButtonText("Zastosuj");
        dialog->setDoubleDecimals(2);
        dialog->setDoubleRange(0.1, 100);
        dialog->setDoubleValue(1);
        int result = dialog->exec();
        if (result == QDialog::Rejected)
            return;
        scale = dialog->doubleValue();
    }

    if (int(mat.size().height * scale) == 0 || int(mat.size().width * scale) == 0) {
        QMessageBox::warning(nullptr, "Skalowanie obrazu", "Nie można przeskalować obraz:\nObraz były za mały");
        return;
    }

    cv::resize(mat,mat, {}, scale, scale);

    subwindow->setMat(mat, subwindow->getCvtToBgr());
}


static void actionSplit()
{
    auto* subwindow = SubWindowModel::get()->getCurrentSubWindow();
    if (!subwindow) {
        QMessageBox::warning(nullptr, "Rozdzielanie obrazu", "Brak wybranego obrazu");
        return;
    }
    const cv::Mat& mat = subwindow->getMat();

    std::vector<cv::Mat> mats;
    mats.resize(mat.channels());
    split(mat, mats.data());
    int offset = 0;
    for (int i = 0; i < mats.size(); i++) {
        auto* sub = new SubWindow(mats[i], cv::COLOR_GRAY2BGR,  QString("%1 (Kanał: %2)").arg(subwindow->getTitle()).arg(i));
        sub->show();
        QRect r = sub->geometry();
        r.translate(offset, offset);
        sub->setGeometry(r);
        offset += 30;
    }
}


static void actionConvert(int from_bgr_to_target, int from_target_to_bgr)
{
    auto* subwindow = SubWindowModel::get()->getCurrentSubWindow();
    if (!subwindow) {
        QMessageBox::warning(nullptr, "Konwersja obrazu", "Brak wybranego obrazu");
        return;
    }

    cv::Mat mat = subwindow->getMat();
    if (subwindow->getCvtToBgr() != -1)
        cv::cvtColor(mat, mat, subwindow->getCvtToBgr());

    // 'target' colorspace jest BGR
    if (from_bgr_to_target == -1 || from_target_to_bgr == -1) {
        subwindow->setMat(mat, -1);
        return;
    }

    cv::cvtColor(mat, mat, from_bgr_to_target);
    subwindow->setMat(mat, from_target_to_bgr);
    subwindow->raise();
}


Lab1::Lab1()
    : QMenu("Lab1")
{

    addAction("Histogram", this, [] {
        HistogramWindow::create(SubWindowModel::get()->getCurrentSubWindow());
    });
    addAction("Tablica wartości", this, [] {
        HistogramTableWindow::create(SubWindowModel::get()->getCurrentSubWindow());
    });
    addAction("Edytuj LUT", this, [] {
        LUTEditWindow::create(SubWindowModel::get()->getCurrentSubWindow());
    });

    addAction("Rozdziel kanały", this, actionSplit);

    auto* menu_scale = addMenu("Skaluj o...");
    menu_scale->addAction("0.5", this, []{ actionScale(0.5); });
    menu_scale->addAction("0.75", this, []{ actionScale(0.75); });
    menu_scale->addAction("1.5", this, []{ actionScale(1.5); });
    menu_scale->addAction("2.0", this, []{ actionScale(2.0); });
    menu_scale->addAction("Inne", this, []{ actionScale(-1); });

    auto* menu_cvt = addMenu("Zamień na format...");
    menu_cvt->addAction("BGR", this, [] {
        actionConvert(-1, -1);
    });
    menu_cvt->addAction("Szaroodcieniowy", this, [] {
        actionConvert(cv::COLOR_BGR2GRAY, cv::COLOR_GRAY2BGR);
    });
    menu_cvt->addAction("HSV", this, [] {
        actionConvert(cv::COLOR_BGR2HSV, cv::COLOR_HSV2BGR);
    });
    menu_cvt->addAction("Lab", this, [] {
        actionConvert(cv::COLOR_BGR2Lab, cv::COLOR_Lab2BGR);
    });
    menu_cvt->addAction("Binary", this, [] {
        BinarizeDialog::create(SubWindowModel::get()->getCurrentSubWindow());
    });

    addAction("Wyrównaj histogram", this, [] {
        EqualizeDialog::create(SubWindowModel::get()->getCurrentSubWindow());
    });
    addAction("Rozciągnij histogram", this, [] {
        HistogramStretchWindow::create(SubWindowModel::get()->getCurrentSubWindow());
    });
    addAction("Rozciąganie selektywne", this, [] {
        SelectiveStretchWindow::create(SubWindowModel::get()->getCurrentSubWindow());
    });
}
