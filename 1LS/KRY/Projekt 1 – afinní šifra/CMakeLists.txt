cmake_minimum_required(VERSION 3.16)
project(kry1)

set(CMAKE_CXX_STANDARD 20)

add_executable(kry1 main.cpp Crypto.cpp Crypto.h Analysis.cpp Analysis.h Constants.h Utils.cpp Utils.h Constants.cpp)
