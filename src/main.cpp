#include <Geode/Geode.hpp>
#include <Geode/loader/SettingEvent.hpp>

// Подключаем наши файлы
#include "GhostRecorder.hpp"
#include "GhostData.hpp"

using namespace geode::prelude;

// Инициализация синглтона
GhostRecorder* GhostRecorder::s_instance = nullptr;

$on_mod(Loaded) {
    log::info("Ghost Replay загружен!");
    
    // Создаём папку для сохранений если её нет
    auto savePath = Mod::get()->getSaveDir();
    if (!std::filesystem::exists(savePath)) {
        std::filesystem::create_directories(savePath);
        log::debug("Создана папка для сохранений: {}", savePath.string());
    }
    
    // Инициализируем рекордер
    GhostRecorder::get();
    
    log::info("Ghost Replay готов к работе!");
}

$on_mod(Unloaded) {
    log::info("Ghost Replay выгружен");
    
    // Очищаем синглтон
    if (GhostRecorder::s_instance) {
        delete GhostRecorder::s_instance;
        GhostRecorder::s_instance = nullptr;
    }
}

// Опционально: слушаем изменения настроек в реальном времени
$execute {
    listenForSettingChanges("enabled", [](bool value) {
        log::debug("Ghost Replay {}", value ? "включён" : "выключен");
    });
    
    listenForSettingChanges("opacity", [](int64_t value) {
        log::debug("Прозрачность изменена на {}%", value);
    });
}
