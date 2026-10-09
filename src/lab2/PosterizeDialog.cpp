#include "PosterizeDialog.h"
#include <QMessageBox>
#include <QVBoxLayout>
#include <QLabel>
#include <QSpinBox>
#include <QPushButton>
#include <opencv2/imgproc.hpp>
#include "../ChannelPicker.h"
#include "../SubWindow.h"

struct PosterizeDialog::Data
{
    PosterizeDialog* self;
    SubWindow* subwindow;
    QVBoxLayout layout;
    ChannelPicker picker;
    QLabel label;
    QSpinBox spinbox;
    QHBoxLayout layout_h;
    QPushButton button_cancel;
    QPushButton button_apply;
};


static void updatePreview(PosterizeDialog::Data& m, bool apply = false)
{
    const int steps = m.spinbox.value() - 1;
    assert(steps > 0 && steps < 256);

    std::array<uint8_t, 256> lut{};
    for (int i = 1; i <= steps; i++) {
        uint8_t value = 255 * i / steps;
        int begin_idx = 255 * i / (steps + 1);
        int end_idx = 255 * (i+1) / (steps + 1);
        for (int j = begin_idx; j < end_idx; j++)
            lut[j] = value;
    }

    cv::Mat mat = m.subwindow->getMat().clone();
    const int mat_channels = mat.channels();

    uint64_t channels = m.picker.getSelectedChannels();
    for (int channel = 0; channels > 0; channels >>= 1, channel++) {
        if ((channels & 1) == 0)
            continue;
        for (uint8_t* p = mat.data + channel; p < mat.datalimit; p += mat_channels)
            *p = lut[*p];
    }

    if (apply) {
        m.subwindow->setMat(mat, m.subwindow->getCvtToBgr());
        m.subwindow->removePreview();
        return;
    }

    m.subwindow->setPreview(mat, m.subwindow->getCvtToBgr());
}


PosterizeDialog* PosterizeDialog::create(SubWindow* subwindow)
{
    if (!subwindow) {
        QMessageBox::warning(nullptr, "Posteryzacja obrazu", "Brak wybranego obrazu");
        return nullptr;
    }
    return new PosterizeDialog(subwindow);
}


PosterizeDialog::PosterizeDialog(SubWindow* subwindow)
    : m(std::make_unique<Data>())
{
    assert(subwindow != nullptr);
    setAttribute(Qt::WA_DeleteOnClose);
    setWindowTitle("Posteryzacja");
    m->self = this;
    m->subwindow = subwindow;

    m->picker.setSubWindow(m->subwindow);
    m->layout.addWidget(&m->picker);

    m->label.setText("Kolory: ");
    m->layout.addWidget(&m->label, 0, Qt::AlignLeft);

    m->spinbox.setRange(2, 256);
    m->spinbox.setSingleStep(1);
    m->spinbox.setValue(3);
    m->layout.addWidget(&m->spinbox, 0);

    m->layout_h.addStretch(1);

    m->button_apply.setText("Zastosuj");
    m->layout_h.addWidget(&m->button_apply, 0);

    m->button_cancel.setText("Anuluj");
    m->layout_h.addWidget(&m->button_cancel, 0);

    m->layout.addLayout(&m->layout_h);

    setLayout(&m->layout);

    connect(subwindow, &SubWindow::closing, this, [this] {
        m->subwindow = nullptr;
        close();
    });
    connect(&m->picker, &ChannelPicker::selectedChannelsChanged, this, [this] {
        updatePreview(*m);
    });
    connect(&m->spinbox, &QSpinBox::valueChanged, this, [this] {
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


PosterizeDialog::~PosterizeDialog()
{
    if (m->subwindow)
        m->subwindow->removePreview();
}

