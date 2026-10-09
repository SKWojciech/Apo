#include <QApplication>
#include <QStyleHints>

#include "MainWindow.h"


int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("APO");
    {
        QImage image(":/icon.png");
        if (app.styleHints()->colorScheme() == Qt::ColorScheme::Dark)
            image.invertPixels();
        app.setWindowIcon(QIcon(QPixmap::fromImage(image)));
    }
    MainWindow window{};
    window.resize(800, 100);
    window.show();
    return QApplication::exec();
}
