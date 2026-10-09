#include "EqualizeDialog.h"
#include <QMessageBox>
#include <opencv2/imgproc.hpp>
#include <QVBoxLayout>
#include <QPushButton>
#include "../SubWindow.h"
#include "../ChannelPicker.h"



struct EqualizeDialog::Data
{
    EqualizeDialog* self;
    SubWindow* subwindow;
    QVBoxLayout layout;
    ChannelPicker picker;
    QHBoxLayout layout_h;
    QPushButton button_cancel;
    QPushButton button_apply;
};


EqualizeDialog* EqualizeDialog::create(SubWindow* subwindow)
{
    if (!subwindow) {
        QMessageBox::warning(nullptr, "Wyrównanie histogramu", "Brak wybranego obrazu");
        return nullptr;
    }
    return new EqualizeDialog(subwindow);
}


static void updatePreview(EqualizeDialog::Data& m, bool apply = false)
{
    if (!m.subwindow)
        return;

    cv::Mat mat = m.subwindow->getMat().clone();
    const int mat_channels = mat.channels();
    assert(mat_channels > 0);

    uint64_t sel_channels = m.picker.getSelectedChannels();
    for (int channel = 0; sel_channels > 0; sel_channels >>= 1, channel++) {
        if ((sel_channels & 1) == 0)
            continue;
        assert(channel < mat_channels);

        // Oblicz histogram
        std::array<size_t, 256> hist{};
        for (const uint8_t* p = mat.data + channel; p < mat.dataend; p += mat_channels)
            hist[*p] += 1;

        // Oblicz dystrybuantę histogramu
        std::array<size_t, 256> dist{};
        dist[0] = hist[0];
        for (int i = 1; i < 256; i++)
            dist[i] = dist[i - 1] + hist[i];

        // Dystrybuantę będziemy musieli przeskalować z (0, dist_h) na (0, 255)
        const double dist_h = (double)dist.back();

        // Normalizacja obrazu
        for (uint8_t* p = mat.data + channel; p < mat.dataend; p += mat_channels)
            *p = std::round((double)dist[*p] / dist_h * 255.0);
    }



    if (apply) {
        m.subwindow->setMat(mat, m.subwindow->getCvtToBgr());
        m.subwindow->removePreview();
        m.subwindow->raise();
        return;
    }

    m.subwindow->setPreview(mat, m.subwindow->getCvtToBgr());
}


EqualizeDialog::EqualizeDialog(SubWindow* subwindow)
    : m(std::make_unique<Data>())
{
    assert(subwindow != nullptr);
    setAttribute(Qt::WA_DeleteOnClose);
    m->self = this;
    m->subwindow = subwindow;
    setWindowTitle("Normalizacja obrazu");

    m->picker.setSubWindow(m->subwindow);
    m->layout.addWidget(&m->picker);

    {
        m->layout_h.addStretch(1);

        m->button_apply.setText("Zastosuj");
        m->layout_h.addWidget(&m->button_apply, 0);

        m->button_cancel.setText("Anuluj");
        m->layout_h.addWidget(&m->button_cancel, 0);
    }
    m->layout.addLayout(&m->layout_h);

    setLayout(&m->layout);

    connect(m->subwindow, &SubWindow::closing, this, [this] {
        m->subwindow = nullptr;
        close();
    });
    connect(m->subwindow, &SubWindow::matChanged, this, [this] {
        updatePreview(*m);
    });
    connect(&m->picker, &ChannelPicker::selectedChannelsChanged, this, [this] {
        updatePreview(*m);
    });
    connect(&m->button_apply, &QPushButton::clicked, this, [this] {
        updatePreview(*m, true);
        m->subwindow = nullptr;
        close();
    });
    connect(&m->button_cancel, &QPushButton::clicked, this, [this] {
        close();
    });

    updatePreview(*m);
    show();
}


EqualizeDialog::~EqualizeDialog()
{
}


