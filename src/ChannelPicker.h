#pragma once
#include <QWidget>


struct ChannelPicker : QWidget
{
    Q_OBJECT
public:
    explicit ChannelPicker(struct SubWindow* subwindow = nullptr);
    ~ChannelPicker() override;

    void setSubWindow(SubWindow* subwindow);
    uint64_t getSelectedChannels();

    struct Data;
    std::unique_ptr<Data> m;

Q_SIGNALS:
    void selectedChannelsChanged(uint64_t map);
};
