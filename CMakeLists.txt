cmake_minimum_required(VERSION 3.7)
project(zfsfun)

set(CMAKE_CXX_STANDARD 14   )


set(SOURCE_FILES main.cpp bswap.h bswap.cpp)
add_executable(zfsfun ${SOURCE_FILES})