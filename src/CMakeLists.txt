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

target_link_libraries(apo
    PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Widgets
        Qt6::Charts
        Qt6::Multimedia
        Qt6::MultimediaWidgets
        opencv_core
        opencv_imgproc
        opencv_objdetect
        opencv_photo
)

qt_add_resources(apo
    "data"
    PREFIX "/"
    BASE "../resources"
    FILES
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
    # Dołącz manifest aby wymusić UTF-8 + ustaw ikonkę programu
    target_sources(apo PRIVATE ../resources/app.manifest ../resources/apo.rc)
endif()

include(GNUInstallDirs)
if(WIN32)
    get_filename_component(apo_compiler_dir "${CMAKE_CXX_COMPILER}" DIRECTORY)

    # Normalizuj ścieżki
    install(CODE [[
        if(POLICY CMP0207)
            cmake_policy(SET CMP0207 NEW)
        endif()
    ]])

    install(TARGETS apo
        RUNTIME_DEPENDENCIES
            DIRECTORIES
                "$<TARGET_FILE_DIR:Qt6::Core>"
                "$<TARGET_FILE_DIR:opencv_core>"
                "${apo_compiler_dir}"
            # Biblioteki systemowe
            PRE_EXCLUDE_REGEXES "api-ms-.*" "ext-ms-.*"
            POST_INCLUDE_REGEXES ".*[/\\\\](msvcp|vcruntime|concrt|ucrtbased)[^/\\\\]*[.]dll"
            POST_EXCLUDE_REGEXES ".*[/\\\\][Ss][Yy][Ss][Tt][Ee][Mm]32[/\\\\].*"
        RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
    )

    get_target_property(apo_qt_type Qt6::Core TYPE)
    if(NOT apo_qt_type STREQUAL "STATIC_LIBRARY")
        qt_generate_deploy_app_script(TARGET apo OUTPUT_SCRIPT apo_deploy_script)
        install(SCRIPT "${apo_deploy_script}")
    endif()
else()
    install(TARGETS apo
        RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
        BUNDLE DESTINATION .
    )
endif()
