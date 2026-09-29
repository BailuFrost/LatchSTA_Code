cmake_minimum_required(VERSION 3.10)

# All third-party dependencies are installed in the conda environment.
if(DEFINED ENV{CONDA_PREFIX})
    set(CMAKE_PREFIX_PATH "$ENV{CONDA_PREFIX}")
endif()

# LEDA library location (LEDA 7.2.3). Can be overridden with the LEDAROOT
# environment variable.
if(DEFINED ENV{LEDAROOT})
    set(LEDA_ROOT "$ENV{LEDAROOT}")
else()
    set(LEDA_ROOT "/home/bailufrost/下载/LEDA-7.2.3")
endif()

project(EllCpp LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# LEDA 7.x headers (must come before the project include dir so they shadow
# the vendored LEDA 6.3 headers in include/LEDA).
include_directories("${LEDA_ROOT}/incl")

# Project headers plus conda-provided third-party headers.
include_directories("${CMAKE_CURRENT_LIST_DIR}/include")
if(DEFINED ENV{CONDA_PREFIX})
    include_directories("$ENV{CONDA_PREFIX}/include")
endif()

find_package(Threads REQUIRED)

# LEDA core library (static)
find_library(LEDA_LIB NAMES leda HINTS "${LEDA_ROOT}" REQUIRED)

# GLPK (used by src/LP.cpp for linear-programming based MCR/ELL)
find_library(GLPK_LIB NAMES glpk REQUIRED)

# OpenBLAS provides BLAS/CBLAS/LAPACK symbols used through xtensor-blas
find_library(OPENBLAS_LIB NAMES openblas REQUIRED)

# Fortran runtime (required by OpenBLAS / LAPACK)
find_library(GFORTRAN_LIB NAMES gfortran REQUIRED)

# libm
find_library(M_LIB NAMES m REQUIRED)

aux_source_directory(src SRC_LIST)
add_executable(main ${SRC_LIST})

target_link_libraries(main
    Threads::Threads
    ${LEDA_LIB}
    ${GLPK_LIB}
    ${OPENBLAS_LIB}
    ${GFORTRAN_LIB}
    ${M_LIB})
