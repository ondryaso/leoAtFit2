cmake_minimum_required(VERSION 3.1)
project(gal-proj CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_FLAGS -O3)

set(FILES main.cpp 
          BMASolver.cpp 
          BMASolver.h
          GreedySolver.cpp
          GreedySolver.h
          Greedy2Solver.cpp
          Greedy2Solver.h
          RamseySolver.cpp
          RamseySolver.h
          WheelFreeRamseySolver.cpp
          WheelFreeRamseySolver.h
          RandomSolver.cpp
          RandomSolver.h
          MISSolver.h)

add_executable(out-local ${FILES})
add_executable(out-installed ${FILES})

add_library(OGDF-static STATIC IMPORTED)
set_target_properties(OGDF-static PROPERTIES
        IMPORTED_LOCATION "${CMAKE_SOURCE_DIR}/OGDF/build/libOGDF.a"
        INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_SOURCE_DIR}/OGDF/include"
        )

add_library(COIN-static STATIC IMPORTED)
set_target_properties(COIN-static PROPERTIES
        IMPORTED_LOCATION "${CMAKE_SOURCE_DIR}/OGDF/build/libCOIN.a"
        INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_SOURCE_DIR}/OGDF/include"
        )

target_link_libraries(out-local OGDF-static)
target_link_libraries(out-local COIN-static)

target_link_libraries(out-installed OGDF)
target_link_libraries(out-installed COIN)
