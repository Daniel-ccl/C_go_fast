cmake_minimum_required(VERSION 3.16)
project(cgofast CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(PkgConfig REQUIRED)
pkg_check_modules(JACK REQUIRED jack)

set(LIBPD_DIR ${CMAKE_SOURCE_DIR}/third_party/libpd)

add_executable(cgofast src/main.cpp)

target_include_directories(cgofast PRIVATE
    ${LIBPD_DIR}/libpd_wrapper
    ${LIBPD_DIR}/libpd_wrapper/util
    ${LIBPD_DIR}/pure-data/src
    ${JACK_INCLUDE_DIRS}
)

target_link_libraries(cgofast PRIVATE
    ${LIBPD_DIR}/libs/libpd.a
    ${JACK_LIBRARIES}
    pthread
    dl
    m
)

target_compile_options(cgofast PRIVATE -O2)
