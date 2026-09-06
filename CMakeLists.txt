cmake_minimum_required(VERSION 4.4)
project(
    lcmp
    VERSION 1.0.0
    DESCRIPTION "Material list comparator for Litematica schematics."
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

add_library(lcmp INTERFACE)
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
add_subdirectory(3rdParty/zlib EXCLUDE_FROM_ALL)
set(ZLIB_FOUND TRUE CACHE BOOL "" FORCE)
set(ZLIB_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/3rdParty/zlib;${CMAKE_CURRENT_BINARY_DIR}/3rdParty/zlib" CACHE PATH "" FORCE)
set(ZLIB_LIBRARY zlibstatic CACHE STRING "" FORCE)
set(ZLIB_LIBRARIES zlibstatic CACHE STRING "" FORCE)

if(TARGET zlibstatic AND NOT TARGET ZLIB::ZLIB)
    add_library(ZLIB::ZLIB ALIAS zlibstatic)
elseif(TARGET zlib AND NOT TARGET ZLIB::ZLIB)
    add_library(ZLIB::ZLIB ALIAS zlib)
endif()

add_subdirectory(3rdParty/libnbtplusplus EXCLUDE_FROM_ALL)

target_include_directories(lcmp INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/3rdParty/pdqsort>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/3rdParty/libnbtplusplus/include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/3rdParty/libnbtplusplus>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/3rdParty/zlib>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/3rdParty/zlib>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

target_link_libraries(lcmp INTERFACE
    nbt++
    absl::flat_hash_map
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