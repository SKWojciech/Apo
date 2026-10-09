cmake_minimum_required(VERSION 3.30)
project(Apo LANGUAGES CXX)


# Ustawienia użytkownika
# =====================================

set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "Typ kompilacji: Debug, Release, RelWithDebInfo or MinSizeRel")
set(APO_STATIC OFF "" CACHE BOOL "Statycznie linkuj CRT/Qt/OpenCV. Wymaga to zbudowania własnej wersji Qt/OpenCV")
set(APO_QT_PATH "" CACHE STRING "Ścieżka do Qt")
set(APO_OPENCV_PATH "" CACHE STRING "Ścieżka do OpenCV")


# Ustawienia projektu
# =====================================

set(CMAKE_DISABLE_IN_SOURCE_BUILD ON)
set(CMAKE_CXX_SCAN_FOR_MODULES OFF)
set(CMAKE_CXX_MODULE_STD OFF)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC OFF)
set(CMAKE_AUTOUIC OFF)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)
if(${APO_STATIC})
	set(BUILD_SHARED_LIBS OFF)
else()
	set(BUILD_SHARED_LIBS ON)
endif()
if(MSVC)
	if(${APO_STATIC})
		set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
	else()
		set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
	endif()
	add_compile_options($<$<CONFIG:Debug>:/fsanitize=address>) # Włącz AddressSanitizer
	add_compile_options(/W4) # Włącz ostrzeżenia
	# Wyłącz RTTI
	string(REPLACE "/GR" "" CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS})
	add_compile_options(/GR-)
else()
	add_compile_options($<$<CONFIG:Debug>:-fsanitize=address) # Włącz AddressSanitizer
	add_compile_options(-Wall -Wextra) # Włącz ostrzeżenia
	add_compile_options(-fno-rtti) # Wyłącz RTTI
endif()

# 3rdparty
set(Qt6_ROOT ${APO_QT_PATH})
find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Charts Multimedia MultimediaWidgets)
set(OpenCV_ROOT ${APO_OPENCV_PATH})
find_package(OpenCV REQUIRED)

# Główny projekt
add_subdirectory(src)
