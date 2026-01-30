cmake_minimum_required(VERSION 3.21)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Для iOS/Mac архитектуры
if ("${CMAKE_SYSTEM_NAME}" STREQUAL "iOS" OR IOS)
    set(CMAKE_OSX_ARCHITECTURES "arm64")
else()
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64")
endif()

project(EchoTrails VERSION 1.0.0)

# Собираем исходники
add_library(${PROJECT_NAME} SHARED
    src/*.cpp
)

# ========== GEODE SDK ==========
# Проверяем что GEODE_SDK установлен
if (NOT DEFINED ENV{GEODE_SDK})
    message(FATAL_ERROR "Unable to find Geode SDK! Please define GEODE_SDK environment variable to point to Geode")
else()
    message(STATUS "Found Geode: $ENV{GEODE_SDK}")
endif()

# Подключаем SDK через add_subdirectory (НЕ find_package!)
add_subdirectory($ENV{GEODE_SDK} ${CMAKE_CURRENT_BINARY_DIR}/geode)

# Настраиваем мод
setup_geode_mod(${PROJECT_NAME})
