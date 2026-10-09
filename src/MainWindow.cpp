#include "MainWindow.h"

#include "SubWindow.h"
#include "lab1/Lab1.h"
#include "lab2/Lab2.h"
#include "lab3/Lab3.h"
#include "lab4/Lab4.h"
#include "projekt/FaceDetect.h"

#include <numbers>
#include <QApplication>
#include <QMessageBox>
#include <QVBoxLayout>
#include <QFileDialog>
#include <QLineEdit>
#include <QMenuBar>
#include <QWindow>
#include <QImageWriter>

#include <opencv2/imgproc.hpp>



struct MainWindow::Data
{
    Lab1 lab1;
    Lab2 lab2;
    Lab3 lab3;
    Lab4 lab4;
    FaceDetect face_detect;
};
alignas(MainWindow::Data) static char buf[sizeof(MainWindow::Data)];

static MainWindow::Data* g_main_window_data = nullptr;
static MainWindow* g_main_window = nullptr;


static void actionSaveFile()
{
    SubWindow* w = SubWindowModel::get()->getCurrentSubWindow();
    if (w == nullptr)
        return;

    QFile file;
    QString filename;
    while (true) {
        filename = QFileDialog::getSaveFileName(g_main_window,
            "Zapisz plik",
            {},
            "Obraz PNG (*.png);;Obraz JPEG (*.jpg);;Obraz BMP *.bmp;;Obraz TIFF *.tiff;; Obraz GIF *.gif"
        );
        if (filename.isEmpty())
            return;
        file.setFileName(filename);
        if (file.open(QIODeviceBase::WriteOnly | QIODeviceBase::Truncate))
            break;
        QMessageBox::warning(g_main_window, "Błąd wybierania pliku", "Podana ścieżka nie istnieje lub nie jest dostępna");
    }
    file.close();

    {
        QImageWriter writer(filename);
        bool success = writer.write(w->getImage());
        if (!success)
            QMessageBox::warning(g_main_window, "Błąd zapisywania pliku", QString("Zapis pliku się nie udał:\n%1").arg(writer.errorString()));
    }
}


static void actionLoadFile()
{
    QString filename;
    while (true) {
        filename = QFileDialog::getOpenFileName(
            g_main_window,
            "Otwórz plik",
            {},
            "Obrazy (*.jpg *.png *.bmp *.tiff *.gif)"
        );
        if (filename.isEmpty())
            return;
        if (QFile(filename).exists())
            break;
        QMessageBox::warning(g_main_window, "Błąd wczytywania pliku", "Podany plik nie istnieje");
    }
    auto* w = new SubWindow(filename);
    w->show();
}


static void actionLoadBuiltinImage(const char* filename, const char* name)
{
    auto* w = new SubWindow(QImage(filename), QString(name));
    w->show();
}


enum class Shape
{
    RECTANGLE,
    CIRCLE,
    ELLIPSE,
    TRIANGLE,
    VARIOUS
};
static void actionLoadShape(Shape shape)
{
    QImage image;
    QString name;
    switch (shape) {
        using enum Shape;
    case RECTANGLE: image.load(":/rect.png"); name = "Prostokąt"; break;
    case CIRCLE: image.load(":/circle.png"); name = "Koło"; break;
    case ELLIPSE: image.load(":/ellipse.png"); name = "Elipsa"; break;
    case TRIANGLE: image.load(":/triangle.png"); name = "Trójkąt"; break;
    case VARIOUS: image.load(":/shapes.png"); name = "Różne"; break;
    }
    image.convertTo(QImage::Format_Grayscale8);
    cv::Mat mat = cv::Mat(image.height(), image.width(), CV_8UC1, image.scanLine(0)).clone();
    auto* w = new SubWindow(mat, cv::COLOR_GRAY2BGR, name);
    w->show();
}


MainWindow::MainWindow()
{
    assert(g_main_window == nullptr);
    g_main_window_data = new (buf) Data();
    auto& m = *g_main_window_data;

    g_main_window = this;
    setFocusPolicy(Qt::ClickFocus);

    auto* menu_bar = menuBar();
    auto* menu_file = menu_bar->addMenu("Plik");
    auto* action_save = menu_file->addAction("Zapisz plik", this, actionSaveFile);
    action_save->setEnabled(false);
    menu_file->addAction("Wczytaj plik", this, actionLoadFile);
    auto* menu_shapes = menu_file->addMenu("Wczytaj kształt...");
    menu_shapes->addAction("Prostokąt", this, [] { actionLoadShape(Shape::RECTANGLE); });
    menu_shapes->addAction("Koło", this, [] { actionLoadShape(Shape::CIRCLE); });
    menu_shapes->addAction("Elipsa", this, [] { actionLoadShape(Shape::ELLIPSE); });
    menu_shapes->addAction("Trójkąt", this, [] { actionLoadShape(Shape::TRIANGLE); });
    menu_shapes->addAction("Różne", this, [] { actionLoadShape(Shape::VARIOUS); });
    auto* menu_images = menu_file->addMenu("Wczytaj obraz testowy...");
    menu_images->addAction("Wczytaj lenę", this, [] { actionLoadBuiltinImage(":/lena.bmp", "Lena"); });
    menu_images->addAction("Wczytaj sudoku", this, [] { actionLoadBuiltinImage(":/sudoku.png", "Sudoku"); });
    menu_images->addAction("Wczytaj messi", this, [] { actionLoadBuiltinImage(":/messi5.jpg", "Messi"); });
    menu_images->addAction("Wczytaj monety", this, [] { actionLoadBuiltinImage(":/water_coins.jpg", "Monety"); });
    menu_bar->addMenu(&m.lab1);
    menu_bar->addMenu(&m.lab2);
    menu_bar->addMenu(&m.lab3);
    menu_bar->addMenu(&m.lab4);
    menu_bar->addAction("Detekcja twarzy", this, []{ g_main_window_data->face_detect.show(); });
    menu_bar->addAction("O Autorze", this, [this] {
        QMessageBox::about(this, "O Autorze",
            "Aplikacja zbiorcza na laboratorium i projekt\n"
            "Autor: Wojciech Kwiliński 22080\n"
            "Prowadzący: dr inż. Łukasz Roszkowiak\n"
            "Algorytmy Przetwarzania Obrazów 2026\n"
            "WIT grupa ID06IO1"
        );
    });

    connect(SubWindowModel::get(), &SubWindowModel::currentSubWindowChanged, this, [action_save] (const SubWindow* subwindow) {
        action_save->setEnabled(subwindow != nullptr);
    });

    /*
    auto* main_layout = new QVBoxLayout();

    auto* central_widget = new QWidget();
    central_widget->setLayout(main_layout);
    setCentralWidget(central_widget);*/
}


MainWindow::~MainWindow()
{
}


void MainWindow::closeEvent(QCloseEvent* event)
{
    QMainWindow::closeEvent(event);
    QApplication::closeAllWindows();
    QApplication::exit();
}


void MainWindow::focusInEvent(QFocusEvent* event)
{
    QMainWindow::focusInEvent(event);
//    for (auto* w : QApplication::allWindows())
//        w->raise();
}
