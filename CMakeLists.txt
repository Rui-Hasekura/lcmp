# Copyright (C) 2026 Rui-Hasekura <ruihasekura@gmail.com>
# SPDX-License-Identifier: GPL-3.0-only

cmake_minimum_required(VERSION 4.4)
project(
    lcmp
    VERSION 1.0.0
    DESCRIPTION "Material list comparator for Litematica schematics."
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS
    Core
    Gui
    Qml
    Quick
)
find_package(Qt6 REQUIRED COMPONENTS LinguistTools)

include(FetchContent)

add_library(lcmp STATIC
    src/core/lcmp.h
)

add_library(lcmp::Core ALIAS lcmp)

FetchContent_Declare(
  abseil-cpp
  GIT_REPOSITORY https://github.com/abseil/abseil-cpp.git
  GIT_TAG        20260817.0
  SYSTEM
)
FetchContent_MakeAvailable(abseil-cpp)

set(NBT_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(NBT_USE_ZLIB ON CACHE BOOL "" FORCE)

set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(ZLIB_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(SKIP_BUILD_SHARED_LIBS ON CACHE BOOL "" FORCE)
set(ZLIB_BUILD_SHARED OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
    zlib
    GIT_REPOSITORY https://github.com/madler/zlib.git
    GIT_TAG        v1.3.1
    SYSTEM
    PATCH_COMMAND ${CMAKE_COMMAND} -E copy_if_different
        <SOURCE_DIR>/zconf.h.cmakein
        <BINARY_DIR>/zconf.h.cmakein
)

FetchContent_MakeAvailable(zlib)

if(TARGET zlibstatic)
    if(NOT TARGET ZLIB::ZLIB)
        add_library(ZLIB::ZLIB ALIAS zlibstatic)
    endif()
    if(NOT TARGET zlib)
        add_library(zlib ALIAS zlibstatic)
    endif()
    set(ZLIB_LIBRARY zlibstatic CACHE STRING "" FORCE)
    set(ZLIB_LIBRARIES zlibstatic CACHE STRING "" FORCE)
endif()

FetchContent_GetProperties(zlib SOURCE_DIR zlib_SOURCE_DIR BINARY_DIR zlib_BINARY_DIR)
set(ZLIB_FOUND TRUE CACHE BOOL "" FORCE)
set(ZLIB_INCLUDE_DIR "${zlib_SOURCE_DIR};${zlib_BINARY_DIR}" CACHE PATH "" FORCE)
set(ZLIB_INCLUDE_DIRS "${ZLIB_INCLUDE_DIR}" CACHE PATH "" FORCE)

add_subdirectory(3rdParty/libnbtplusplus EXCLUDE_FROM_ALL)

target_include_directories(lcmp PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/3rdParty/pdqsort>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/3rdParty/libnbtplusplus/include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/3rdParty/libnbtplusplus>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
target_compile_features(lcmp PUBLIC cxx_std_23)
if(MSVC)
    target_compile_options(lcmp PRIVATE /arch:AVX2)
else()
    target_compile_options(lcmp PRIVATE -mavx2 -mbmi2)
endif()
target_link_libraries(lcmp PUBLIC
    nbt++
    absl::flat_hash_map
    Qt6::Core
    Qt6::Gui
    Qt6::Qml
    Qt6::Quick
)

include(GNUInstallDirs)
install(TARGETS lcmp
    EXPORT lcmpTargets
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(DIRECTORY src/ 3rdParty/pdqsort/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    FILES_MATCHING PATTERN "*.h"
)

qt_add_executable(lcmp-gui
    app/main.cpp
)

qt_add_qml_module(lcmp-gui
    URI "lcmp.gui"
    VERSION 1.0
    QML_FILES
        qml/Main.qml
    SOURCES
        src/lcmp_bridge.h
        src/lcmp_bridge.cc
)

qt_add_translations(lcmp-gui
    TS_FILES
        translations/lcmp_zh_CN.ts
        translations/lcmp_zh_TW.ts
        translations/lcmp_zh_HK.ts
        translations/lcmp_ja_JP.ts
        translations/lcmp_de_DE.ts
        translations/lcmp_fr_FR.ts
)

set_source_files_properties(qml/Main.qml
    PROPERTIES
        QT_RESOURCE_ALIAS "Main.qml"
)

target_link_libraries(lcmp-gui PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Qml
    Qt6::Quick
    lcmp::Core
)

set_target_properties(lcmp-gui PROPERTIES
    WIN32_EXECUTABLE TRUE
    MACOSX_BUNDLE TRUE
)