#pragma once
#include <QWidget>

struct FaceDetect : QWidget
{
    FaceDetect();
    ~FaceDetect() override;

    struct Data;
private:
    std::unique_ptr<Data> m;

protected:
    void showEvent(QShowEvent* event) override;
    void closeEvent(QCloseEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
};
