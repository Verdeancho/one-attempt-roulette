cmake_minimum_required(VERSION 3.21)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64")
set(CMAKE_CXX_VISIBILITY_PRESET hidden)

project(OneAttemptRoulette VERSION 1.3.0)

file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS src/*.cpp)
add_library(${PROJECT_NAME} SHARED ${SOURCES})

if (NOT DEFINED ENV{GEODE_SDK})
    message(FATAL_ERROR "Geode SDK not found. Set the GEODE_SDK environment variable.")
else()
    message(STATUS "Found Geode: $ENV{GEODE_SDK}")
endif()

# No incrustar rutas del PC (nombre de usuario) en el DLL que se comparte
if (MSVC AND CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    file(TO_CMAKE_PATH "$ENV{USERPROFILE}" HOME_DIR)
    add_compile_options("/clang:-ffile-prefix-map=${HOME_DIR}=C:/dev")
    target_link_options(${PROJECT_NAME} PRIVATE "/PDBALTPATH:%_PDB%")
endif()

add_subdirectory($ENV{GEODE_SDK} ${CMAKE_CURRENT_BINARY_DIR}/geode)

setup_geode_mod(${PROJECT_NAME})
