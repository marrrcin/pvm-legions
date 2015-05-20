cmake_minimum_required(VERSION 3.2)
project(src)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

set(SOURCE_FILES
    data.h
    def.h
    input.dat
    livereload.py
    Makefile
    master.c
    slave.c
    slaveName.h)

add_executable(src ${SOURCE_FILES} utils.h)

include_directories("/usr/include/glib-2.0")