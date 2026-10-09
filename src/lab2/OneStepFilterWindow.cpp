#include "OneStepFilterWindow.h"
#include <QMessageBox>
#include <QVBoxLayout>
#include <QGridLayout>
#include <QComboBox>
#include <QSpinBox>
#include <QPushButton>
#include <QLabel>
#include <QInputDialog>
#include <QFileDialog>
#include <opencv2/imgproc.hpp>
#include "../SubWindow.h"



static constexpr int MAX_MASK_LENGTH = 7;
static constexpr std::array g_mask_lengths = {3, 5, 7};
static constexpr std::array g_mask_size_labels = {"3x3", "5x5", "7x7"};
enum MaskType : int
{
    MASK_NOTHING, MASK_BOX_BLUR, MASK_GAUSSIAN_BLUR, MASK_SOBEL_OPERATOR, MASK_SOBEL_OPERATOR_DIAG, MASK_LAPLACIAN_OPERATOR_141,
    MASK_LAPLACIAN_OPERATOR_181, MASK_LAPLACIAN_OPERATOR_242, MASK_PREWITT_OPERATOR, MASK_PREWITT_OPERATOR_DIAG, MASK_LOAD, MASK_SAVE
};
static constexpr std::array g_mask_labels = {
    "Wybierz maskę...", "Wygładzanie liniowe", "Wygładzanie Gaussa", "Maska Sobela", "Maska Sobela kąt", "Maska Laplasjana 141",
    "Maska Laplasjana 181", "Maska Laplasjana 242", "Maska Prewitta", "Maska Prewitta kąt", "Wczytaj z pliku...", "Zapisz do pliku..."
};
static constexpr std::array g_borders = {
    cv::BORDER_ISOLATED, cv::BORDER_REFLECT, cv::BORDER_REPLICATE
};
static constexpr std::array g_border_labels = {
    "Bez zmian (Isolated)", "Odbicie lustrzane (Reflect)", "Powtórz wartość (Replicate)"
};


struct OneStepFilterWindow::Data
{
    OneStepFilterWindow* self;
    SubWindow* subwindow;

    QVBoxLayout layout;
    QComboBox combo_subwindow;
    QGridLayout layout_settings;
    QLabel label_combo_mask_size;
    QComboBox combo_mask_size;
    QLabel label_combo_border;
    QComboBox combo_border;
    QComboBox combo_mask_templates;

    QGridLayout layout_grid;
    std::array<QDoubleSpinBox, MAX_MASK_LENGTH*MAX_MASK_LENGTH> spinboxes;

    QHBoxLayout layout_h_buttons;
    QPushButton button_transpose;
    QPushButton button_rotate;
    QPushButton button_apply;
    QPushButton button_cancel;
};


static void setDefaultMask(OneStepFilterWindow::Data& m)
{
    int mask_length = g_mask_lengths[m.combo_mask_size.currentIndex()];
    for (int row = 0; row < mask_length; row++)
        for (int col = 0; col < mask_length; col++)
            m.spinboxes[row*MAX_MASK_LENGTH + col].setValue(row == col && row == mask_length / 2 ? 1.0 : 0.0);
}


static void updatePreview(OneStepFilterWindow::Data& m, bool apply = false)
{
    if (m.subwindow == nullptr)
        return;

    int mask_length = g_mask_lengths[m.combo_mask_size.currentIndex()];

    cv::Mat kernel(mask_length, mask_length, CV_64F);
    double sum = 0;
    for (int row = 0; row < mask_length; row++)
        for (int col = 0; col < mask_length; col++)
            sum += kernel.at<double>(row, col) = m.spinboxes[row*MAX_MASK_LENGTH + col].value();

    if (sum != 0)
        kernel /= sum;

    cv::BorderTypes border = g_borders[m.combo_border.currentIndex()];

    cv::Mat mat = m.subwindow->getMat().clone();

    cv::filter2D(mat, mat, -1, kernel, cv::Point(-1, -1), 0, border);

    if (apply) {
        m.subwindow->setMat(mat, m.subwindow->getCvtToBgr());
        m.subwindow->removePreview();
        return;
    }

    m.subwindow->setPreview(mat, m.subwindow->getCvtToBgr());
}


static void setSubwindow(OneStepFilterWindow::Data& m, SubWindow* subwindow)
{
    SubWindow* old_subwindow = m.subwindow;
    m.subwindow = subwindow;

    if (old_subwindow != nullptr && m.subwindow != old_subwindow)
        QWidget::disconnect(old_subwindow, nullptr, m.self, nullptr);

    if (m.subwindow == nullptr)
        return;

    if (m.subwindow != old_subwindow) {
        QWidget::connect(m.subwindow, &SubWindow::closing, m.self, [&m] {
            m.subwindow = nullptr;
            m.self->close();
        });
        QWidget::connect(m.subwindow, &SubWindow::matChanged, m.self, [&m] {
            updatePreview(m);
        });
    }

    updatePreview(m);
}


static void saveToFile(OneStepFilterWindow::Data& m)
{
    int mask_length = g_mask_lengths[m.combo_mask_size.currentIndex()];

    QFile file;
    while (true) {
        QString filename = QFileDialog::getSaveFileName(nullptr, "Zapisz maskę do...", {}, "Plik tekstowy (*.txt)");
        if (filename.isEmpty())
            return;
        file.setFileName(filename);
        if (file.open(QIODeviceBase::WriteOnly | QIODeviceBase::Truncate))
            break;
        QMessageBox::warning(nullptr, "Błąd zapisywania pliku", "Podana ścieżka nie istnieje lub nie jest dostępna");
    }
    std::string data{};
    data.append(g_mask_size_labels[m.combo_mask_size.currentIndex()]);
    data.push_back('\n');
    for (int row = 0; row < mask_length; row++)
        for (int col = 0; col < mask_length; col++)
            data.append(std::to_string(m.spinboxes[row*MAX_MASK_LENGTH + col].value())).push_back(col + 1 == mask_length ? '\n' : ' ');
    file.write(data.data(), data.size());
}


static void loadFromFile(OneStepFilterWindow::Data& m)
{
    int mask_length = g_mask_lengths[m.combo_mask_size.currentIndex()];

    QFile file;
    while (true) {
        QString filename = QFileDialog::getOpenFileName(nullptr, "Otwórz plik", {}, "Plik tekstowy (*.txt)");
        if (filename.isEmpty())
            break;
        file.setFileName(filename);
        if (file.open(QIODeviceBase::ReadOnly))
            break;
        QMessageBox::warning(nullptr, "Błąd wybierania pliku", "Podana ścieżka nie istnieje lub nie jest dostępna");
    }

    QByteArray data = file.readAll();
    file.close();
    if (data.size() == 0) {
        QMessageBox::warning(nullptr, "Błąd wczytywania pliku", "Plik jest pusty / błąd pliku");
        return;
    }

    // Sprawdź, czy nagłówek się zgadza
    const char* header_end_ptr;
    {
        unsigned int len1 = 0;
        auto [ptr1, err1] = std::from_chars(data.begin(), data.end(), len1);
        if (err1 != std::errc{} || *ptr1 != 'x') {
            QMessageBox::warning(nullptr, "Błąd wczytywania pliku", "Wadliwy pierwszy rozmiar macierzy");
            return;
        }
        unsigned int len2 = 0;
        auto [ptr2, err2] = std::from_chars(ptr1 + 1, data.end(), len2);
        if (err2 != std::errc{} || *ptr2 != '\n' || len1 != len2) {
            QMessageBox::warning(nullptr, "Błąd wczytywania pliku", "Wadliwy drugi rozmiar macierzy");
            return;
        }
        if ((unsigned)mask_length != len1) {
            QMessageBox::warning(nullptr, "Błąd wczytywania pliku", QString("Maska jest za mała.\nW pliku: %1x%1, wybrano: %2x%2").arg(len1).arg(mask_length));
            return;
        }
        header_end_ptr = ptr2;
    }

    std::array<double, MAX_MASK_LENGTH*MAX_MASK_LENGTH> mask;

    auto ptr = header_end_ptr + 1;
    int extracted = 0;
    while (ptr < data.end()) {
        double d = 0;
        auto [p, e] = std::from_chars(ptr, data.end(), d);
        if (p == ptr || e != std::errc{} || !(*p == ' ' || *p == '\n' || *p == '\0')) {
            QMessageBox::warning(nullptr, "Błąd wczytywania pliku", QString("Błędna liczba idx: %1").arg(extracted));
            m.combo_mask_templates.setCurrentIndex(0);
            return;
        }
        int row = extracted / mask_length;
        int col = extracted % mask_length;
        mask[row*mask_length + col] = d;
        extracted++;
        if (extracted == mask_length * mask_length)
            break;
        ptr = p + 1;
    }
    if (extracted != mask_length * mask_length) {
        QMessageBox::warning(nullptr, "Błąd wczytywania pliku", QString("Za mało liczb.\nOczekiwano: %1, wyjęto: %2").arg(mask_length * mask_length).arg(extracted));
        return;
    }

    int i = 0;
    for (int row = 0; row < mask_length; row++)
        for (int col = 0; col < mask_length; col++)
            m.spinboxes[row*MAX_MASK_LENGTH + col].setValue(mask[i++]);
}


static void updateMask(OneStepFilterWindow::Data& m)
{
    auto selection = (MaskType)m.combo_mask_templates.currentIndex();
    if (selection < 0)
        return;

    int mask_length = g_mask_lengths[m.combo_mask_size.currentIndex()];

    switch (selection)
    {
    case MASK_NOTHING: return;
    case MASK_BOX_BLUR: {
        qreal x = 1.0 / (mask_length*mask_length);
        for (int row = 0; row < mask_length; row++)
            for (int col = 0; col < mask_length; col++)
                m.spinboxes[row*MAX_MASK_LENGTH + col].setValue(x);
        break;
    }
    case MASK_GAUSSIAN_BLUR: {
        assert(m.subwindow->getMat().depth() == CV_8U);
        int ksize = mask_length;
        cv::Mat kernel = cv::getGaussianKernel(ksize, 0, CV_64F);
        cv::mulTransposed(kernel, kernel, false);
        int i = 0;
        for (int row = 0; row < mask_length; row++)
            for (int col = 0; col < mask_length; col++)
                m.spinboxes[row*MAX_MASK_LENGTH + col].setValue(kernel.at<double>(i++));
        break;
    }
    case MASK_SOBEL_OPERATOR_DIAG:
    case MASK_SOBEL_OPERATOR: {
        if (mask_length != 3) {
            QMessageBox::warning(nullptr, "Filtracja jednoetapowa", "Operator Sobela jest dostępny tylko z maską 3x3.");
            break;
        }
        std::array kernel = {
            -1, +0, +1,
            -2, +0, +2,
            -1, +0, +1
        };
        if (selection == MASK_SOBEL_OPERATOR_DIAG)
            kernel = {+0, +1, +2,
                         -1, +0, +1,
                         -2, -1, +0};
        int i = 0;
        for (int row = 0; row < mask_length; row++)
            for (int col = 0; col < mask_length; col++)
                m.spinboxes[row*MAX_MASK_LENGTH + col].setValue(kernel[i++]);
        break;
    }
    case MASK_LAPLACIAN_OPERATOR_242:
    case MASK_LAPLACIAN_OPERATOR_141:
    case MASK_LAPLACIAN_OPERATOR_181: {
        if (mask_length != 3) {
            QMessageBox::warning(nullptr, "Filtracja jednoetapowa", "Operator Laplaciana jest dostępny tylko z maską 3x3.");
            break;
        }

        std::array<double, 3*3> kernel;
        if (selection == MASK_LAPLACIAN_OPERATOR_141)
            kernel = {+0, -1, +0,
                         -1, +4, -1,
                         +0, -1, +0};
        else if (selection == MASK_LAPLACIAN_OPERATOR_242)
            kernel = {+1, -2, +1,
                         -2, +4, -2,
                         +1, -2, +1};
        else // selection == MASK_LAPLACIAN_OPERATOR_181
            kernel = {-1, -1, -1,
                         -1, +8, -1,
                         -1, -1, -1};

        int i = 0;
        for (int row = 0; row < mask_length; row++)
            for (int col = 0; col < mask_length; col++)
                m.spinboxes[row*MAX_MASK_LENGTH + col].setValue(kernel[i++]);
        break;
    }
    case MASK_PREWITT_OPERATOR_DIAG:
    case MASK_PREWITT_OPERATOR: {
        if (mask_length != 3) {
            QMessageBox::warning(nullptr, "Filtracja jednoetapowa", "Operator Prewitta'ego jest dostępny tylko z maską 3x3.");
            break;
        }

        std::array<double, 3*3> kernel = {
            +1, +0, -1,
            +1, +0, -1,
            +1, +0, -1
        };
        if (selection == MASK_PREWITT_OPERATOR_DIAG)
            kernel = {+0, +1, +1,
                         -1, +0, +1,
                         -1, -1, +0};

        int i = 0;
        for (int row = 0; row < mask_length; row++)
            for (int col = 0; col < mask_length; col++)
                m.spinboxes[row*MAX_MASK_LENGTH + col].setValue(kernel[i++]);
        break;
    }
    case MASK_LOAD: {
        loadFromFile(m);
        break;
    }
    case MASK_SAVE: {
        saveToFile(m);
        break;
    }
    }

    m.combo_mask_templates.setCurrentIndex(0);
    updatePreview(m);
}


static void updateMaskSize(OneStepFilterWindow::Data& m)
{
    int length = g_mask_lengths[m.combo_mask_size.currentIndex()];
    for (int row = 0; row < MAX_MASK_LENGTH; row++)
        for (int col = 0; col < MAX_MASK_LENGTH; col++)
            m.spinboxes[row*MAX_MASK_LENGTH + col].setVisible(row < length && col < length);
    m.self->adjustSize();
}


static void transposeMask(OneStepFilterWindow::Data& m)
{
    int length = g_mask_lengths[m.combo_mask_size.currentIndex()];
    cv::Mat mat(length, length, CV_64F);
    for (int row = 0; row < length; row++)
        for (int col = 0; col < length; col++)
            mat.at<double>(row, col) = m.spinboxes[row*MAX_MASK_LENGTH + col].value();
    cv::transpose(mat, mat);
    for (int row = 0; row < length; row++)
        for (int col = 0; col < length; col++)
            m.spinboxes[row*MAX_MASK_LENGTH + col].setValue(mat.at<double>(row, col));
}


static void rotateMask(OneStepFilterWindow::Data& m)
{
    int length = g_mask_lengths[m.combo_mask_size.currentIndex()];
    cv::Mat mat(length, length, CV_64F);
    for (int row = 0; row < length; row++)
        for (int col = 0; col < length; col++)
            mat.at<double>(row, col) = m.spinboxes[row*MAX_MASK_LENGTH + col].value();
    cv::rotate(mat, mat, cv::ROTATE_90_CLOCKWISE);
    for (int row = 0; row < length; row++)
        for (int col = 0; col < length; col++)
            m.spinboxes[row*MAX_MASK_LENGTH + col].setValue(mat.at<double>(row, col));
}


OneStepFilterWindow* OneStepFilterWindow::create(SubWindow* subwindow)
{
    if (subwindow == nullptr) {
        QMessageBox::warning(nullptr, "Filtracja jednoetapowa", "Brak wybranego obrazu");
        return nullptr;
    }
    return new OneStepFilterWindow(subwindow);
}


OneStepFilterWindow::OneStepFilterWindow(SubWindow* subwindow)
    : m(std::make_unique<Data>())
{
    assert(subwindow != nullptr);
    setAttribute(Qt::WA_DeleteOnClose);
    setWindowTitle("Filtracja jednoetapowa");
    m->self = this;

    m->combo_subwindow.setModel(SubWindowModel::get());
    m->combo_subwindow.setCurrentIndex(SubWindowModel::get()->getIndexOfSubWindow(subwindow));
    m->layout.addWidget(&m->combo_subwindow);

    {
        m->label_combo_mask_size.setText("Rozmiar maski: ");
        m->layout_settings.addWidget(&m->label_combo_mask_size, 0, 0, Qt::AlignRight);

        m->combo_mask_size.addItems(QStringList(g_mask_size_labels.begin(), g_mask_size_labels.end()));
        m->combo_mask_size.setCurrentIndex(0);
        m->layout_settings.addWidget(&m->combo_mask_size, 0, 1);

        m->label_combo_border.setText("Wartości brzegowe: ");
        m->layout_settings.addWidget(&m->label_combo_border, 1, 0, Qt::AlignRight);

        m->combo_border.addItems(QStringList(g_border_labels.begin(), g_border_labels.end()));
        m->combo_border.setCurrentIndex(1);
        m->layout_settings.addWidget(&m->combo_border, 1, 1);
    }
    m->layout.addLayout(&m->layout_settings);

    m->combo_mask_templates.addItems(QStringList(g_mask_labels.begin(), g_mask_labels.end()));
    m->combo_mask_templates.setCurrentIndex(0);
    m->layout.addWidget(&m->combo_mask_templates);

    {
        for (int row = 0; row < MAX_MASK_LENGTH; row++) {
            for (int col = 0; col < MAX_MASK_LENGTH; col++) {
                auto& spinbox = m->spinboxes[row*MAX_MASK_LENGTH + col];
                spinbox.setRange(-99, 99);
                spinbox.setVisible(false);
                spinbox.setDecimals(3);
                m->layout_grid.addWidget(&spinbox, row, col, Qt::AlignVCenter);
            }
        }
        updateMaskSize(*m);
        setDefaultMask(*m);
    }
    m->layout.addStretch(1);
    m->layout.addLayout(&m->layout_grid, 0);
    m->layout.addStretch(1);

    {
        m->button_transpose.setText("Transpozycja");
        m->layout_h_buttons.addWidget(&m->button_transpose);

        m->button_rotate.setText("Obrót");
        m->layout_h_buttons.addWidget(&m->button_rotate);

        m->layout_h_buttons.addStretch(1);

        m->button_apply.setText("Zastosuj");
        m->layout_h_buttons.addWidget(&m->button_apply);

        m->button_cancel.setText("Anuluj");
        m->layout_h_buttons.addWidget(&m->button_cancel);
    }
    m->layout.addLayout(&m->layout_h_buttons);

    setLayout(&m->layout);

    connect(&m->combo_subwindow, &QComboBox::currentIndexChanged, this, [this] (int idx) {
        if (idx != -1)
            setSubwindow(*m, SubWindowModel::get()->getSubWindowAt(idx));
        else
            setSubwindow(*m, nullptr);
    });
    connect(&m->combo_mask_size, &QComboBox::currentIndexChanged, this, [this] {
        updateMaskSize(*m);
        setDefaultMask(*m);
        updatePreview(*m);
    });
    connect(&m->combo_border, &QComboBox::currentIndexChanged, this, [this] {
        updatePreview(*m);
    });
    connect(&m->combo_mask_templates, &QComboBox::currentIndexChanged, this, [this] {
        updateMask(*m);
    });
    connect(&m->button_transpose, &QPushButton::clicked, this, [this] {
        transposeMask(*m);
    });
    connect(&m->button_rotate, &QPushButton::clicked, this, [this] {
        rotateMask(*m);
    });
    connect(&m->button_apply, &QPushButton::clicked, this, [this] {
        updatePreview(*m, true);
        close();
    });
    connect(&m->button_cancel, &QPushButton::clicked, this, [this] {
        close();
    });
    for (auto& spinbox : m->spinboxes)
        connect(&spinbox, &QDoubleSpinBox::valueChanged, this, [this] { updatePreview(*m); });

    setSubwindow(*m, subwindow);
    show();
}


OneStepFilterWindow::~OneStepFilterWindow()
{
    if (m->subwindow)
        m->subwindow->removePreview();
}
