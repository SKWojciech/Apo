#pragma once
#include <QWidget>
#include <QAbstractListModel>

namespace cv { class Mat; }



struct SubWindow : QWidget
{
    Q_OBJECT
public:
    explicit SubWindow(const QString& filename);
    SubWindow(const QImage& image, const QString& name);
    SubWindow(const cv::Mat& mat, int cvt_to_bgr, const QString& name);
    ~SubWindow() override;

    QString getTitle() const;

    QImage getImage() const;
    const cv::Mat getMat() const;
    void setMat(const cv::Mat& mat, int cvt_to_bgr);
    int getCvtToBgr() const; // Zwraca kod cv::COLOR_xxx2BGR lub -1 jak Mat jest już we formacie BGR

    void setPreview(cv::Mat mat, int cvt_to_bgr);
    void setPreview(const QPixmap& pixmap);
    void removePreview();

    struct Data;
    std::unique_ptr<Data> m;

protected:
    void focusInEvent(QFocusEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void closeEvent(QCloseEvent* event) override;

Q_SIGNALS:
    void matChanged(SubWindow* self);
    void closing();
};



struct SubWindowModel : QAbstractListModel
{
    Q_OBJECT
public:
    static SubWindowModel* get();

    int rowCount(const QModelIndex& parent) const override;
    QVariant data(const QModelIndex& index, int role) const override;

    void setCurrentSubWindow(SubWindow* window);
    int getIndexOfSubWindow(const SubWindow* window);
    SubWindow* getSubWindowAt(int index);
    SubWindow* getSubWindowWithTitle(const QString& title);
    SubWindow* getCurrentSubWindow();

    struct SubWindowEntry {
        SubWindow* window;
        QString title;
    };
    std::vector<SubWindowEntry> m_subwindows;
    SubWindow* m_current_subwindow{};

    void addSubWindowAndModifyTitle(SubWindow* window, QString& title);
    void removeSubWindow(SubWindow* window);

Q_SIGNALS:
    void currentSubWindowChanged(SubWindow* window);

private:
    SubWindowModel();
};



struct SubWindowChannelModel : QAbstractListModel
{
public:
    explicit SubWindowChannelModel(SubWindow* subwindow = nullptr);
    void setSubWindow(SubWindow* subwindow);

    int rowCount(const QModelIndex& parent) const override;
    QVariant data(const QModelIndex& index, int role) const override;


    SubWindow* m_subwindow;
    int m_channels;
    int m_cvt_to_bgr;
    void updateChannels();
};