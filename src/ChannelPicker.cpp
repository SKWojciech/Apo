#include "ChannelPicker.h"
#include "SubWindow.h"
#include <QHBoxLayout>
#include <QLabel>
#include <QCheckBox>
#include <QPushButton>
#include <QButtonGroup>
#include <opencv2/imgproc.hpp>



struct ChannelPicker::Data
{
    ChannelPicker* self;
    SubWindow* subwindow;

    QHBoxLayout layout;
    QButtonGroup button_group;
    QCheckBox main_checkbox;
    std::vector<QCheckBox*> checkboxes;
    int channels;
    int cvt_to_gbr;
    uint64_t checked_map;
};


static void updateCheckboxes(ChannelPicker::Data& m)
{
    int old_cvt_to_gbr = m.cvt_to_gbr;
    uint64_t old_map = m.checked_map;

    if (m.subwindow == nullptr) {
        m.main_checkbox.setEnabled(false);
        m.channels = 0;
        m.cvt_to_gbr = INT_MIN;
    }
    else {
        m.main_checkbox.setEnabled(true);
        m.channels = m.subwindow->getMat().channels();
        m.cvt_to_gbr = m.subwindow->getCvtToBgr();
    }

    for (int i = m.channels; i < m.checkboxes.size(); i++) {
        QCheckBox* checkbox = m.checkboxes[i];
        checkbox->setVisible(false);
        m.checked_map &= ~(1 << i);
    }

    for (int i = 0; i < m.channels && i < m.checkboxes.size(); i++) {
        auto* checkbox = m.checkboxes[i];
        checkbox->setVisible(true);
        bool checked = m.main_checkbox.isChecked();
        checkbox->setChecked(checked);
        m.checked_map |= (uint64_t(checked) << i);
    }

    for (int i = (int)m.checkboxes.size(); i < m.channels; i++) {
        auto* checkbox = new QCheckBox();
        m.checkboxes.push_back(checkbox);
        bool checked = m.main_checkbox.isChecked();
        checkbox->setChecked(checked);
        m.checked_map |= (uint64_t(checked) << i);
        m.layout.addWidget(checkbox);
        m.button_group.addButton(checkbox);
    }

    if (old_cvt_to_gbr != m.cvt_to_gbr) {
        QString prefix("Kanał %1");

        if (m.cvt_to_gbr == -1) {
            std::array<char, 3> suffixes = {'B', 'G', 'R'};
            for (int i = 0; i < 3; i++)
                m.checkboxes[i]->setText(prefix.arg(suffixes[i]));
        }
        else {
            for (int i = 0; auto* checkbox : m.checkboxes)
                checkbox->setText(prefix.arg(i++));
        }
    }

    if (old_map != m.checked_map)
        emit m.self->selectedChannelsChanged(m.checked_map);
}


void ChannelPicker::setSubWindow(SubWindow* subwindow)
{
    if (m->subwindow != nullptr)
        disconnect(m->subwindow, nullptr, this, nullptr);

    m->subwindow = subwindow;
    m->checked_map = 0;
    updateCheckboxes(*m);

    if (m->subwindow == nullptr)
        return;

    connect(m->subwindow, &SubWindow::closing, this, [this] {
        m->subwindow = nullptr;
        updateCheckboxes(*m);
    });

    connect(m->subwindow, &SubWindow::matChanged, this, [this] {
        updateCheckboxes(*m);
    });
}


uint64_t ChannelPicker::getSelectedChannels()
{
    return m->checked_map;
}


ChannelPicker::ChannelPicker(SubWindow* subwindow)
    : m(std::make_unique<Data>())
{
    m->self = this;
    m->cvt_to_gbr = INT_MIN;

    setLayout(&m->layout);
    m->layout.setAlignment(Qt::AlignLeft);
    m->main_checkbox.setText("Wszystkie kanały");
    m->layout.addWidget(&m->main_checkbox);
    m->main_checkbox.setChecked(true);

    m->button_group.setExclusive(false);
    m->button_group.addButton(&m->main_checkbox);

    setSubWindow(subwindow);

    connect(&m->main_checkbox, &QCheckBox::clicked, this, [this](bool checked) {
        for (auto& checkbox : m->checkboxes)
            checkbox->setChecked(checked);

        if (checked)
            m->checked_map = (1ull << m->channels) - 1;
        else
            m->checked_map = 0;
        emit selectedChannelsChanged(m->checked_map);
    });

    connect(&m->button_group, &QButtonGroup::buttonClicked, this, [this] (QAbstractButton* checkbox) {
        if (!checkbox->isChecked())
            m->main_checkbox.setChecked(false);

        int id = m->button_group.id(checkbox);
        auto it = std::ranges::find(m->checkboxes, checkbox);
        int idx = it - m->checkboxes.begin();
        if (id != -2)
            assert(-(id + 3) == idx);

        uint64_t old_map = m->checked_map;

        if (checkbox->isChecked())
            m->checked_map |= (1ull << idx);
        else
            m->checked_map &= ~(1ull << idx);

        if (old_map != m->checked_map)
            emit m->self->selectedChannelsChanged(m->checked_map);

        if (m->checked_map == (1ull << m->channels) - 1)
            m->main_checkbox.setChecked(true);
    });
}


ChannelPicker::~ChannelPicker()
{
}
