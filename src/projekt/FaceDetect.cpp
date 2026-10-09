#include "FaceDetect.h"
#include <QMediaDevices>
#include <QMediaCaptureSession>
#include <QCamera>
#include <QComboBox>
#include <QMessageBox>
#include <QVBoxLayout>
#include <QGraphicsView>
#include <QGraphicsVideoItem>
#include <QVideoSink>
#include <QTemporaryDir>
#include <QGraphicsRectItem>
#include <QLabel>
#include <opencv2/opencv.hpp>

#include <semaphore>


// Dane okna
struct FaceDetect::Data
{
    // GUI
    QWidget* self{};
    QMediaDevices devices{};
    QMediaCaptureSession capture_session{};
    std::unique_ptr<QCamera> camera{};
    QVBoxLayout layout{};
    QComboBox combo{};
    QLabel message{};
    QGraphicsView view{};
    QGraphicsScene scene{};
    QGraphicsVideoItem video{};
    QHBoxLayout layout_sliders{};
    QLabel label_scale{};
    QSlider slider_scale{};
    QLabel label_mn{};
    QSlider slider_mn{};

    // Modele kaskadowe
    cv::CascadeClassifier face_classifier{};
    cv::CascadeClassifier eyes_classifier{};

    // Wykryte twarze (prostokąty) i oczy (elipsy)
    std::vector<QGraphicsRectItem*> rect_items{};
    std::vector<QGraphicsEllipseItem*> ellipse_items{};

    // Dane używane podczas przetwarzania obrazu
    std::vector<QRectF> rects_staged{};
    std::vector<QRectF> ellipses_staged{};
    std::binary_semaphore semaphore{1};
};


// Aktualizacja listy kamer
static void updateCameras(FaceDetect::Data& m)
{
    m.combo.clear();
    // Załaduj listę podłączonych kamer
    const QList<QCameraDevice> camera_list = QMediaDevices::videoInputs();
    // Dodaj kamery do UI, ustaw domyślną kamerę
    for (int i = 0; const QCameraDevice& camera : camera_list) {
        m.combo.addItem(camera.description(), QVariant::fromValue(camera));
        if (camera == QMediaDevices::defaultVideoInput())
            m.combo.setCurrentIndex(i);
        i++;
    }
}


// Detekcja twarzy
static void runFaceDetection(FaceDetect::Data& m, QImage img)
{
    // Zamień obraz na szaro odcieniowy
    img.convertTo(QImage::Format_Grayscale8);
    // Zamień obraz na macierz
    cv::Mat mat(cv::Size(img.width(), img.height()), CV_8UC1, (void*)img.constBits());
    // Wyrównaj histogram
    cv::equalizeHist(mat, mat);

    // Załaduj opcje detekcji
    double scale = 1.0 + (double)m.slider_scale.value() / 100.0;
    int min_neighbours = m.slider_mn.value();

    // Wykryj twarze
    std::vector<cv::Rect> faces;
    m.face_classifier.detectMultiScale(mat, faces, scale, min_neighbours);

    // Dla każdej wykrytej twarzy...
    for (size_t i = 0; i < faces.size(); i++) {
        // Weź i zapisz prostokąt otaczający tę twarz
        cv::Rect rect = faces[i];
        m.rects_staged.emplace_back(rect.x, rect.y, rect.width, rect.height);

        // Wytnij wejściową macierz tak, aby uzyskać tylko ten obszar, w którym jest twarz
        cv::Mat mat_face = mat(rect);
        // Wykryj oczy w tej twarzy
        std::vector<cv::Rect> eyes;
        m.eyes_classifier.detectMultiScale(mat_face, eyes, scale, min_neighbours);

        // Weź i zapisz elipsy reprezentujące wykryte oczy
        for (size_t j = 0; j < eyes.size(); j++) {
            cv::Rect eye_rect = eyes[j];
            m.ellipses_staged.emplace_back(rect.x + eye_rect.x, rect.y + eye_rect.y, eye_rect.width, eye_rect.height);
        }
    }

    // Sygnalizuj ukończenie detekcji
    m.semaphore.release();
}


static void processFrame(FaceDetect::Data& m, const QVideoFrame& frame)
{
    // Modele mogły zostać niezaładowane. Nie idź dalej, jeśli ich nie mamy
    if (m.eyes_classifier.empty() || m.face_classifier.empty())
        return;

    // Nie idź dalej, jeśli detekcja twarzy jest aktywna
    if (not m.semaphore.try_acquire())
        return;

    // Zaktualizuj prostokąty
    for (size_t i = 0; i < m.rect_items.size() && i < m.rects_staged.size(); i++) {
        m.rect_items[i]->show();
        m.rect_items[i]->setRect(m.rects_staged[i]);
    }
    for (size_t i = m.rect_items.size(); i < m.rects_staged.size(); i++)
        m.rect_items.push_back(m.scene.addRect(m.rects_staged[i], QColorConstants::Red, Qt::NoBrush));
    for (size_t i = m.rects_staged.size(); i < m.rect_items.size(); i++)
        m.rect_items[i]->hide();
    m.rects_staged.clear();

    // Zaktualizuj elipsy
    for (size_t i = 0; i < m.ellipse_items.size() && i < m.ellipses_staged.size(); i++) {
        m.ellipse_items[i]->show();
        m.ellipse_items[i]->setRect(m.ellipses_staged[i]);
    }
    for (size_t i = m.ellipse_items.size(); i < m.ellipses_staged.size(); i++)
        m.ellipse_items.push_back(m.scene.addEllipse(m.ellipses_staged[i], QColorConstants::Blue, Qt::NoBrush));
    for (size_t i = m.ellipses_staged.size(); i < m.ellipse_items.size(); i++)
        m.ellipse_items[i]->hide();
    m.ellipses_staged.clear();

    // Uruchom runFaceDetection()
    std::thread(runFaceDetection, std::ref(m), frame.toImage()).detach();
}


// Ustaw aktywną kamerę na 'camera'
static void setCamera(FaceDetect::Data& m, const QCameraDevice& camera)
{
    m.camera.reset(new QCamera(camera));
    m.capture_session.setCamera(m.camera.get());

    QWidget::connect(m.camera.get(), &QCamera::errorOccurred, m.self, [&m] {
        if (m.camera->error() != QCamera::NoError)
            QMessageBox::warning(m.self, "Błąd kamery!", m.camera->errorString());
    });

    m.capture_session.setVideoOutput(&m.video);
    QWidget::connect(m.video.videoSink(), &QVideoSink::videoFrameChanged, m.self,
        [&m] (const QVideoFrame& frame) { processFrame(m, frame); }
    );
    m.camera->start();
}


// Załaduj modele
static QString loadModels(FaceDetect::Data& m)
{
    struct ModelEntry {
        const char* filename;
        const char* temp_filename;
        cv::CascadeClassifier* classifier;
    };
    std::array<ModelEntry, 2> entries = {{
        {":haarcascade_frontalface_alt.xml", "face.xml", &m.face_classifier},
        {":haarcascade_eye_tree_eyeglasses.xml", "eyes.xml", &m.eyes_classifier}
    }};

    // Ja zapisuje modele, wbudowując je w samą aplikację, aby mieć jeden plik .exe
    // Dzięki temu, wystarczy tylko sama aplikacja do jej uruchomienia bez żadnych dodatkowych folderów czy plików.
    // Ale OpenCV wymaga ścieżki do fizycznego pliku na dysku.
    // Więc muszę:
    // 1. Stworzyć tymczasowy folder
    // 2. Stworzyć plik w tym folderze
    // 3. Przekopiować dane modelu do tego pliku
    // 4. Załadować model podając ścieżkę tego pliku
    // 5. Powtórzyć to kopiowanie dla każdego modelu
    // 6. Usunąć tymczasowy folder po załadowaniu wszystkich modeli

    // Stwórz tymczasowy folder
    QTemporaryDir dir("APO");

    // Dla każdego modelu, jaki mamy wgrać...
    for (ModelEntry& entry : entries) {

        // Otwórz wejściowy 'plik' modelu
        // (tak naprawdę, nie jest to faktyczny plik na dysku, ale blok danych zamieszony we .exe)
        QFile file(entry.filename);
        if (not file.open(QIODevice::ReadOnly)) {
            return QString("Nie udało się załadować kaskadowego klasyfikatora: ") + file.errorString();
        }

        // Stwórz fizyczny plik na dysku
        QFile dst(dir.filePath(entry.temp_filename));
        if (not dst.open(QIODevice::ReadWrite)) {
            return QString("Nie udało się załadować kaskadowego klasyfikatora: ") + file.errorString();
        }

        // Przekopuj dane.
        dst.write(file.readAll());
        dst.flush();

        // Wgraj model z tego fizycznego pliku
        if (not entry.classifier->load(dst.fileName().toStdString())) {
            return QString("Nie udało się załadować kaskadowego klasyfikatora: Błąd w OpenCV.");
        }
    }

    // OK
    return {};
}


// Ustaw układ graficzny
static void layoutInit(FaceDetect::Data& m)
{
    m.self->setWindowTitle("Detekcja twarzy");
    m.self->resize(800, 500);
    m.layout.addWidget(&m.combo);
    m.combo.setPlaceholderText("Brak kamery");

    {
        m.label_scale.setText("Skalowanie:");
        m.layout_sliders.addWidget(&m.label_scale);
        m.slider_scale.setOrientation(Qt::Horizontal);
        m.layout_sliders.addWidget(&m.slider_scale);
        m.slider_scale.setRange(2, 20);
        m.slider_scale.setValue(10);

        m.label_mn.setText("Min. sąsiadów:");
        m.layout_sliders.addWidget(&m.label_mn);
        m.slider_mn.setOrientation(Qt::Horizontal);
        m.layout_sliders.addWidget(&m.slider_mn);
        m.slider_mn.setRange(1, 6);
        m.slider_mn.setValue(3);

        m.layout.addLayout(&m.layout_sliders);
    }

    m.message.setAlignment(Qt::AlignCenter);
    m.layout.addWidget(&m.message);

    m.layout.addWidget(&m.view);
    m.view.setScene(&m.scene);
    m.scene.addItem(&m.video);
    m.scene.setSceneRect(0, 0, 100, 100);
    m.video.setPos(0, 0);
    m.view.setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m.view.setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m.self->setLayout(&m.layout);

    QWidget::connect(&m.devices, &QMediaDevices::videoInputsChanged, m.self, [&m] {
        updateCameras(m);
    });
    QWidget::connect(&m.combo, &QComboBox::currentIndexChanged, m.self, [&m] {
        setCamera(m, qvariant_cast<QCameraDevice>(m.combo.currentData()));
    });
    QWidget::connect(&m.video, &QGraphicsVideoItem::nativeSizeChanged, m.self, [&m] (const QSizeF& size) {
        m.video.setSize(size);
        m.scene.setSceneRect({0, 0, size.width(), size.height()});
        m.view.fitInView(&m.video);
    });
}


// Zapewnij inicjalizację okna
static void ensureInit(FaceDetect::Data& m)
{
	// Układ graficzny ustawiony?
    if (m.layout.parent() == nullptr) {
        // Ustaw układ
        layoutInit(m);
    }

    // Modele załadowane?
    if (m.face_classifier.empty() || m.eyes_classifier.empty()) {
        // Załaduj modele detekcji twarzy
        QString error_message = loadModels(m);
        m.message.setText(error_message);
    }

    // Zaktualizuj listę kamer
    updateCameras(m);
}


FaceDetect::FaceDetect()
    : m(std::make_unique<Data>())
{
    m->self = this;
}


FaceDetect::~FaceDetect()
{
    // Poczekaj na zatrzymanie detekcji przed usunięciem danych
    m->semaphore.acquire();
}


void FaceDetect::showEvent(QShowEvent* event)
{
    // Załaduj GUI i modele
    ensureInit(*m);

    if (m->camera)
        m->camera->start();

    QWidget::showEvent(event);
}


void FaceDetect::closeEvent(QCloseEvent* event)
{
    if (m->camera)
        m->camera->stop();

    QWidget::closeEvent(event);
}


void FaceDetect::resizeEvent(QResizeEvent* event)
{
    if (m->view.scene())
        m->view.fitInView(&m->video);

    QWidget::resizeEvent(event);
}
