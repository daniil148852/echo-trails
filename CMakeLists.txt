cmake_minimum_required(VERSION 3.21)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64")
set(CMAKE_CXX_VISIBILITY_PRESET hidden)

# Название проекта
project(echo-trails VERSION 1.0.0)

# Добавляем исходные файлы
add_library(${PROJECT_NAME} SHARED
    src/main.cpp
    # Добавьте дополнительные .cpp файлы здесь при необходимости
)

# Проверка наличия Geode SDK
if (NOT DEFINED ENV{GEODE_SDK})
    message(FATAL_ERROR "Unable to find Geode SDK! Please define GEODE_SDK environment variable to point to Geode")
else()
    message(STATUS "Found Geode: $ENV{GEODE_SDK}")
endif()

# Подключаем Geode SDK
add_subdirectory($ENV{GEODE_SDK} ${CMAKE_CURRENT_BINARY_DIR}/geode)

# Настройка мода (без лишних зависимостей — проект легковесный)
setup_geode_mod(${PROJECT_NAME})

# Опционально: флаги оптимизации для релизной сборки
if (CMAKE_BUILD_TYPE STREQUAL "Release")
    target_compile_options(${PROJECT_NAME} PRIVATE -O2)
endif()
