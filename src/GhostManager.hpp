#pragma once

#include "GhostRecorder.hpp"
#include "GhostPlayer.hpp"

namespace EchoTrails {

// Единый менеджер для удобного доступа
class GhostManager {
public:
    static GhostManager& get() {
        static GhostManager instance;
        return instance;
    }
    
    GhostRecorder& recorder() { return GhostRecorder::get(); }
    GhostPlayer& player() { return GhostPlayer::get(); }
    
    bool isEnabled() const {
        return Mod::get()->getSettingValue<bool>("enabled");
    }
    
private:
    GhostManager() = default;
};

// Удобные макросы
#define GhostMgr GhostManager::get()
#define GhostRec GhostRecorder::get()
#define GhostPlay GhostPlayer::get()

} // namespace EchoTrails
