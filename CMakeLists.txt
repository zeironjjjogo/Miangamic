cmake_minimum_required(VERSION 4.3)
project(miangamic VERSION 0.0.0 LANGUAGES CXX)
include_directories(${CMAKE_SOURCE_DIR}/aviutl2_sdk_mirror/include)
add_library(${PROJECT_NAME} SHARED 
    src/inireader.cpp
    src/i18n.cpp
    src/covenc.cpp
    src/lgr.cpp
    include/inireader.h
    include/i18n.h
    include/covenc.h
    include/lgr.h
    src/main.cpp
    include/errmsg.h
    src/errmsg.cpp)
target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_20)
target_compile_options(${PROJECT_NAME} PRIVATE /utf-8)