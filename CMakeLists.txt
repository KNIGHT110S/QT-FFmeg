cmake_minimum_required(VERSION 3.16)

project(QT-FFmeg-player VERSION 0.1 LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(FFMPEG_ROOT_CANDIDATES
    "${CMAKE_SOURCE_DIR}/third_party/FFmeg"
    "${CMAKE_SOURCE_DIR}/third_party/ffmpeg"
)

foreach(FFMPEG_ROOT_CANDIDATE IN LISTS FFMPEG_ROOT_CANDIDATES)
    if(EXISTS "${FFMPEG_ROOT_CANDIDATE}/include"
       AND EXISTS "${FFMPEG_ROOT_CANDIDATE}/lib"
       AND EXISTS "${FFMPEG_ROOT_CANDIDATE}/bin")
        set(FFMPEG_ROOT "${FFMPEG_ROOT_CANDIDATE}")
        break()
    endif()
endforeach()

if(NOT FFMPEG_ROOT)
    message(FATAL_ERROR "Local FFmpeg was not found under third_party/FFmeg or third_party/ffmpeg.")
endif()

set(FFMPEG_INCLUDE_DIR "${FFMPEG_ROOT}/include")
set(FFMPEG_LIB_DIR "${FFMPEG_ROOT}/lib")
set(FFMPEG_BIN_DIR "${FFMPEG_ROOT}/bin")

if(MINGW)
    set(FFMPEG_LIB_PREFIX "lib")
    set(FFMPEG_IMPORT_SUFFIX ".dll.a")
elseif(MSVC)
    set(FFMPEG_LIB_PREFIX "")
    set(FFMPEG_IMPORT_SUFFIX ".lib")
else()
    message(FATAL_ERROR "This project currently supports FFmpeg integration for MinGW or MSVC on Windows.")
endif()

function(resolve_ffmpeg_library out_var base_name)
    set(library_path "${FFMPEG_LIB_DIR}/${FFMPEG_LIB_PREFIX}${base_name}${FFMPEG_IMPORT_SUFFIX}")
    if(NOT EXISTS "${library_path}")
        message(FATAL_ERROR "Required FFmpeg import library was not found: ${library_path}")
    endif()
    set(${out_var} "${library_path}" PARENT_SCOPE)
endfunction()

resolve_ffmpeg_library(FFMPEG_AVFORMAT_LIBRARY avformat)
resolve_ffmpeg_library(FFMPEG_AVCODEC_LIBRARY avcodec)
resolve_ffmpeg_library(FFMPEG_AVUTIL_LIBRARY avutil)
resolve_ffmpeg_library(FFMPEG_SWSCALE_LIBRARY swscale)
resolve_ffmpeg_library(FFMPEG_SWRESAMPLE_LIBRARY swresample)

file(GLOB FFMPEG_RUNTIME_DLLS "${FFMPEG_BIN_DIR}/*.dll")

message(STATUS "Using FFmpeg from: ${FFMPEG_ROOT}")

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets)

set(PROJECT_SOURCES
        main.cpp
        mainwindow.cpp
        mainwindow.h
        mainwindow.ui
)

if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    qt_add_executable(QT-FFmeg-player
        MANUAL_FINALIZATION
        ${PROJECT_SOURCES}
    )
# Define target properties for Android with Qt 6 as:
#    set_property(TARGET QT-FFmeg-player APPEND PROPERTY QT_ANDROID_PACKAGE_SOURCE_DIR
#                 ${CMAKE_CURRENT_SOURCE_DIR}/android)
# For more information, see https://doc.qt.io/qt-6/qt-add-executable.html#target-creation
else()
    if(ANDROID)
        add_library(QT-FFmeg-player SHARED
            ${PROJECT_SOURCES}
        )
# Define properties for Android with Qt 5 after find_package() calls as:
#    set(ANDROID_PACKAGE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/android")
    else()
        add_executable(QT-FFmeg-player
            ${PROJECT_SOURCES}
        )
    endif()
endif()

target_include_directories(QT-FFmeg-player PRIVATE "${FFMPEG_INCLUDE_DIR}")

target_link_libraries(QT-FFmeg-player PRIVATE
    Qt${QT_VERSION_MAJOR}::Widgets
    "${FFMPEG_AVFORMAT_LIBRARY}"
    "${FFMPEG_AVCODEC_LIBRARY}"
    "${FFMPEG_AVUTIL_LIBRARY}"
    "${FFMPEG_SWSCALE_LIBRARY}"
    "${FFMPEG_SWRESAMPLE_LIBRARY}"
)

if(FFMPEG_RUNTIME_DLLS)
    add_custom_command(TARGET QT-FFmeg-player POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${FFMPEG_RUNTIME_DLLS}
            $<TARGET_FILE_DIR:QT-FFmeg-player>
        COMMAND_EXPAND_LISTS
    )
endif()

# Qt for iOS sets MACOSX_BUNDLE_GUI_IDENTIFIER automatically since Qt 6.1.
# If you are developing for iOS or macOS you should consider setting an
# explicit, fixed bundle identifier manually though.
if(${QT_VERSION} VERSION_LESS 6.1.0)
  set(BUNDLE_ID_OPTION MACOSX_BUNDLE_GUI_IDENTIFIER com.example.QT-FFmeg-player)
endif()
set_target_properties(QT-FFmeg-player PROPERTIES
    ${BUNDLE_ID_OPTION}
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

include(GNUInstallDirs)
install(TARGETS QT-FFmeg-player
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(QT-FFmeg-player)
endif()
