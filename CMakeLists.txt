cmake_minimum_required(VERSION 3.16)
project(maxwell_demon_sim VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(ENABLE_OPENMP "Enable OpenMP when available" ON)

find_package(OpenMP QUIET)

add_executable(maxwell_demon_sim
    src/main.cpp
    src/config.cpp
    src/demon.cpp
    src/equilibrium.cpp
    src/io.cpp
    src/simulation.cpp
)

target_include_directories(maxwell_demon_sim PRIVATE include)

target_compile_options(maxwell_demon_sim PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-O3 -Wall -Wextra -Wpedantic>
    $<$<CXX_COMPILER_ID:MSVC>:/O2 /W4>
)

if(ENABLE_OPENMP AND OpenMP_CXX_FOUND)
    target_link_libraries(maxwell_demon_sim PRIVATE OpenMP::OpenMP_CXX)
    target_compile_definitions(maxwell_demon_sim PRIVATE MAXWELL_DEMON_USE_OPENMP=1)
endif()
