#include <Geode/Geode.hpp>
#include <Geode/loader/SettingEvent.hpp>
#include "TimeRewindManager.hpp"

using namespace geode::prelude;

$on_mod(Loaded) {
    log::info("Time Rewind mod loaded!");
    
    // Подписываемся на изменение настроек
    listenForSettingChanges("enabled", [](bool value) {
        log::info("Time Rewind enabled: {}", value);
    });
    
    listenForSettingChanges("rewind-duration", [](double value) {
        TimeRewindManager::get()->m_rewindDuration = static_cast<float>(value);
        TimeRewindManager::get()->loadSettings(); // Пересчитываем буфер
    });
}
