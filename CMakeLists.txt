cmake_minimum_required(VERSION 3.21)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64")
set(CMAKE_CXX_VISIBILITY_PRESET hidden)

project(InspoGenerator VERSION 3.0.0)

option(INSPO_EDITOR_COLORS "Write palette into level start colors (uses ColorAction fields)" ON)

add_library(${PROJECT_NAME} SHARED
    main.cpp
    Engine.cpp
    Generator.cpp
    Structures.cpp
    Background.cpp
    Layout.cpp
    Decorate.cpp
    Greebles.cpp
    DecoratePopup.cpp
)

if (INSPO_EDITOR_COLORS)
    target_compile_definitions(${PROJECT_NAME} PRIVATE INSPO_EDITOR_COLORS=1)
else()
    target_compile_definitions(${PROJECT_NAME} PRIVATE INSPO_EDITOR_COLORS=0)
endif()

if (NOT DEFINED ENV{GEODE_SDK})
    message(FATAL_ERROR "Unable to find Geode SDK! Please define GEODE_SDK environment variable to point to Geode")
else()
    message(STATUS "Found Geode: $ENV{GEODE_SDK}")
endif()

add_subdirectory($ENV{GEODE_SDK} ${CMAKE_CURRENT_BINARY_DIR}/geode)

setup_geode_mod(${PROJECT_NAME})
