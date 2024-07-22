cmake_minimum_required(VERSION 3.4...3.18)
project(sourceline)

# make the Release version create a PDB
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /Zi")
# minimize the size or the resulting EXE
set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} /DEBUG /OPT:REF /OPT:ICF")

add_subdirectory(pybind11)
pybind11_add_module(sourceline src/main.cpp)

# EXAMPLE_VERSION_INFO is defined by setup.py and passed into the C++ code as a
# define (VERSION_INFO) here.
target_compile_definitions(sourceline
                           PRIVATE VERSION_INFO=${EXAMPLE_VERSION_INFO} UNICODE _UNICODE)
