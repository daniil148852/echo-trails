cmake_minimum_required(VERSION 3.21)

# Отключаем проверку SSL, если возникают проблемы с загрузкой ресурсов в РФ
set(CMAKE_TLS_VERIFY OFF)

# Название проекта берем из кода (EchoTrails)
project(EchoTrails VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Поиск Geode SDK (автоматически использует переменную окружения GEODE_SDK)
find_package(Geode REQUIRED)

# Собираем все файлы исходного кода из папки src
file(GLOB_RECURSE SOURCES src/*.cpp)

# Создаем мод. Эта функция заменяет add_library и setup_geode_mod
add_geode_mod(${PROJECT_NAME}
    SOURCES ${SOURCES}
)

# Добавляем пути для поиска заголовочных файлов
target_include_directories(${PROJECT_NAME} PUBLIC include src)
