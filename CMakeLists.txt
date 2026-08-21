cmake_minimum_required(VERSION 3.16)

project(cgofast LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(PkgConfig REQUIRED)
find_package(Threads REQUIRED)

pkg_check_modules(JACK REQUIRED IMPORTED_TARGET jack)

set(LIBPD_DIR "${CMAKE_SOURCE_DIR}/third_party/libpd")
set(LIBPD_LIBRARY "${LIBPD_DIR}/libs/libpd.a")

if(NOT EXISTS "${LIBPD_LIBRARY}")
    message(FATAL_ERROR
        "No se encontro ${LIBPD_LIBRARY}. "
        "Primero debes compilar el submodulo libpd."
    )
endif()

add_executable(cgofast
    src/main.cpp
    src/audio_engine.cpp
)

target_include_directories(cgofast PRIVATE
    "${CMAKE_SOURCE_DIR}/src"
    "${LIBPD_DIR}/libpd_wrapper"
    "${LIBPD_DIR}/libpd_wrapper/util"
    "${LIBPD_DIR}/pure-data/src"
)

target_compile_definitions(cgofast PRIVATE
    CGOFAST_PATCH_DIR="${CMAKE_SOURCE_DIR}/patches"
)

target_compile_options(cgofast PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    $<$<CONFIG:Release>:-O2>
)

target_link_libraries(cgofast PRIVATE
    "${LIBPD_LIBRARY}"
    PkgConfig::JACK
    Threads::Threads
    ${CMAKE_DL_LIBS}
    m
)
