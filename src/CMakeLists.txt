qt_add_executable(apo
    main.cpp
    MainWindow.cpp
    SubWindow.cpp
    ChannelPicker.cpp
    projekt/FaceDetect.cpp
    lab1/Lab1.cpp
    lab1/HistogramWindow.cpp
    lab1/HistogramTableWindow.cpp
    lab1/HistogramStretchWindow.cpp
    lab1/SelectiveStretchWindow.cpp
    lab1/LUTEditWindow.cpp
    lab1/BinarizeDialog.cpp
    lab1/EqualizeDialog.cpp
    lab2/Lab2.cpp
    lab2/PosterizeDialog.cpp
    lab2/OneStepFilterWindow.cpp
    lab2/FilterDialog.cpp
    lab2/TwoArgOperatorWindow.cpp
    lab2/TwoStepFilterWindow.cpp
    lab3/Lab3.cpp
    lab3/MorphologicalOperationsWindow.cpp
    lab3/HoughLineTransformDialog.cpp
    lab3/PyramidDialog.cpp
    lab3/LineProfileWindow.cpp
    lab4/Lab4.cpp
    lab4/ThresholdDialog.cpp
    lab4/GrabCutWindow.cpp
    lab4/ObjectCharacteristicsWindow.cpp
    lab4/InpaintWindow.cpp
)

target_link_libraries(apo PRIVATE Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Charts Qt6::Multimedia Qt6::MultimediaWidgets opencv_core opencv_imgproc opencv_objdetect opencv_photo)
qt_add_resources(apo "data" PREFIX "/" BASE "../resources" FILES
    ../resources/icon.png
    ../resources/lena.bmp
    ../resources/haarcascade_frontalface_alt.xml
    ../resources/haarcascade_eye_tree_eyeglasses.xml
    ../resources/selected.png
    ../resources/rect.png
    ../resources/circle.png
    ../resources/ellipse.png
    ../resources/triangle.png
    ../resources/sudoku.png
    ../resources/messi5.jpg
    ../resources/water_coins.jpg
    ../resources/shapes.png
)
if(WIN32)
    # WIN32_EXECUTABLE wyłącza okienko konsoli oraz stdout - wyłącz je tylko dla Release
    set_target_properties(apo PROPERTIES WIN32_EXECUTABLE $<$<CONFIG:Release,MinSizeRel>:ON>)
    # Dołącz manifest aby wymusić UTF-8
    # Qt w pełni korzysta z Unicode, ale OpenCV wywołuje API Windowsa, które korzystają z lokalnego narrow-encoding.
    # Na przykład: wybranie pliku za pomocą QFileDialog daje UTF-16 QString, który jest zamieniany na UTF-8 std::string,
    # który jest wysyłany do fopen i wszystko wybucha jeśli ścieżka ma znaki nie-ASCII, bo fopen oczekuje bajtów
    # zakodowanych według code page użytkownika a nie UTF-8. Na szczęście możemy wymusić UTF-8 poprzez manifest.
    target_sources(apo PRIVATE ../resources/app.manifest ../resources/apo.rc)
endif()

