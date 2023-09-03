cmake_minimum_required(VERSION 3.0)

set(CMAKE_BUILD_TYPE "Debug")

project(webserver)

include_directories(include)

#file(GLOB allSrc src/*.cpp)
aux_source_directory(src allSrc)

add_executable(webserver main.cpp ${allSrc})

add_compile_options(-std=c++11)
