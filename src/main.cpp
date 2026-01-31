#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/PlayerObject.hpp>
#include <Geode/modify/PauseLayer.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <Geode/modify/EndLevelLayer.hpp>

#include "TimeRewindManager.hpp"
#include "RewindVisuals.hpp"

using namespace geode::prelude;

// ============================================================
// PlayLayer Hooks
// ============================================================

class $modify(TimeRewindPlayLayer, PlayLayer) {
    struct Fields {
        bool m_initialized = false;
        bool m_deathIntercepted = false;
    };
    
    /**
     * @brief Initialize time rewind when level starts
     */
    bool init(GJGameLevel* level, bool useReplay, bool dontCreateObjects) {
        if (!PlayLayer::init(level, useReplay, dontCreateObjects)) {
            return false;
        }
        
        // Initialize the rewind manager
        auto manager = TimeRewindManager::get();
        manager->loadSettings();
        manager->initialize(this);
        
        // Create the charges display
        RewindVisuals::get()->createChargesDisplay(this);
        
        m_fields->m_initialized = true;
        
        log::info("[TimeRewind] Level initialized: {}", 
                  level->m_levelName.c_str());
        
        return true;
    }
    
    /**
     * @brief Record states in postUpdate (crucial for GD 2.2's variable TPS)
     */
    void postUpdate(float dt) {
        PlayLayer::postUpdate(dt);
        
        auto manager = TimeRewindManager::get();
        
        // If currently rewinding, handle the rewind animation
        if (manager->isRewinding()) {
            manager->updateRewind(this, dt);
            return;
        }
        
        // Otherwise, record frames if we should
        if (manager->isEnabled() && manager->isRecording()) {
            if (manager->shouldRecordFrame(dt)) {
                manager->recordFrame(this);
            }
        }
        
        // Update charges display
        RewindVisuals::get()->updateChargesDisplay(
            manager->getCharges(),
            manager->getMaxCharges()
        );
    }
    
    /**
     * @brief Intercept player death and trigger rewind instead
     */
    void destroyPlayer(PlayerObject* player, GameObject* object) {
        auto manager = TimeRewindManager::get();
        
        // Check if we can rewind instead of dying
        if (manager->isEnabled() && manager->canRewind()) {
            log::info("[TimeRewind] Death intercepted! Starting rewind...");
            
            m_fields->m_deathIntercepted = true;
            
            // Start the rewind sequence (don't call original)
            if (manager->startRewind(this)) {
                // Prevent death state
                if (m_player1) {
                    m_player1->m_isDead = false;
                    m_player1->setVisible(true);
                    m_player1->stopAllActions();
                }
                if (m_player2) {
                    m_player2->m_isDead = false;
                    m_player2->setVisible(true);
                    m_player2->stopAllActions();
                }
                
                // Don't proceed with death
                return;
            }
        }
        
        // No rewinds available or rewind failed, proceed with normal death
        m_fields->m_deathIntercepted = false;
        PlayLayer::destroyPlayer(player, object);
    }
    
    /**
     * @brief Clear buffer on level reset
     */
    void resetLevel() {
        auto manager = TimeRewindManager::get();
        
        // Cancel any active rewind
        if (manager->isRewinding()) {
            manager->cancelRewind(this);
            RewindVisuals::get()->hideRewindOverlay(this);
        }
        
        // Reset rewind state and reinitialize
        manager->reset();
        manager->initialize(this);
        
        // Call original reset
        PlayLayer::resetLevel();
        
        // Recreate UI elements
        RewindVisuals::get()->createChargesDisplay(this);
        
        m_fields->m_deathIntercepted = false;
        
        log::debug("[TimeRewind] Level reset, buffer cleared");
    }
    
    /**
     * @brief Clean up on level exit
     */
    void onQuit() {
        log::info("[TimeRewind] Exiting level, cleaning up...");
        
        // Clean up visuals
        RewindVisuals::get()->cleanupVisuals();
        
        // Reset manager state
        TimeRewindManager::get()->reset();
        
        PlayLayer::onQuit();
    }
    
    /**
     * @brief Handle level completion
     */
    void levelComplete() {
        // Disable rewind on level complete
        auto manager = TimeRewindManager::get();
        
        if (manager->isRewinding()) {
            manager->cancelRewind(this);
        }
        
        manager->reset();
        RewindVisuals::get()->cleanupVisuals();
        
        PlayLayer::levelComplete();
    }
    
    /**
     * @brief Override update to skip during rewind
     */
    void update(float dt) {
        auto manager = TimeRewindManager::get();
        
        // If rewinding, don't run normal game update
        if (manager->isRewinding()) {
            // Only update cocos2d base layer (for animations)
            CCNode::update(dt);
            return;
        }
        
        PlayLayer::update(dt);
    }
    
    /**
     * @brief Handle pausing during rewind
     */
    void pauseGame(bool pause) {
        auto manager = TimeRewindManager::get();
        
        // Cancel rewind if pausing
        if (pause && manager->isRewinding()) {
            manager->cancelRewind(this);
            RewindVisuals::get()->hideRewindOverlay(this);
        }
        
        PlayLayer::pauseGame(pause);
    }
};

// ============================================================
// GJBaseGameLayer Hooks (Input Blocking)
// ============================================================

class $modify(TimeRewindGameLayer, GJBaseGameLayer) {
    /**
     * @brief Block button inputs during rewind
     */
    void handleButton(bool down, int button, bool isPlayer1) {
        auto manager = TimeRewindManager::get();
        
        // Block all input during rewind
        if (manager->isInputBlocked() || manager->isRewinding()) {
            return;
        }
        
        GJBaseGameLayer::handleButton(down, button, isPlayer1);
    }
};

// ============================================================
// PlayerObject Hooks (Input Blocking)
// ============================================================

class $modify(TimeRewindPlayer, PlayerObject) {
    /**
     * @brief Block push button during rewind
     */
    void pushButton(PlayerButton button) {
        auto manager = TimeRewindManager::get();
        
        if (manager->isInputBlocked() || manager->isRewinding()) {
            return;
        }
        
        PlayerObject::pushButton(button);
    }
    
    /**
     * @brief Block release button during rewind
     */
    void releaseButton(PlayerButton button) {
        auto manager = TimeRewindManager::get();
        
        if (manager->isInputBlocked() || manager->isRewinding()) {
            return;
        }
        
        PlayerObject::releaseButton(button);
    }
    
    /**
     * @brief Prevent player updates during rewind
     */
    void update(float dt) {
        auto manager = TimeRewindManager::get();
        
        if (manager->isRewinding()) {
            // Skip physics update during rewind
            return;
        }
        
        PlayerObject::update(dt);
    }
};

// ============================================================
// PauseLayer Hooks
// ============================================================

class $modify(TimeRewindPause, PauseLayer) {
    /**
     * @brief Handle pause menu opening during rewind
     */
    void customSetup() {
        PauseLayer::customSetup();
        
        auto manager = TimeRewindManager::get();
        
        // If we paused during rewind, cancel it
        if (manager->isRewinding()) {
            if (auto playLayer = PlayLayer::get()) {
                manager->cancelRewind(playLayer);
                RewindVisuals::get()->hideRewindOverlay(playLayer);
            }
        }
    }
};

// ============================================================
// EndLevelLayer Hooks
// ============================================================

class $modify(TimeRewindEndLevel, EndLevelLayer) {
    /**
     * @brief Clean up on level end
     */
    void customSetup() {
        EndLevelLayer::customSetup();
        
        // Clean up rewind system
        RewindVisuals::get()->cleanupVisuals();
        TimeRewindManager::get()->reset();
    }
};

// ============================================================
// Mod Lifecycle
// ============================================================

$execute {
    log::info("========================================");
    log::info("   Time Rewind Mod v1.0.0 Loaded!");
    log::info("========================================");
}

$on_mod(Loaded) {
    log::info("[TimeRewind] Mod loaded, initializing...");
    
    // Pre-initialize managers
    TimeRewindManager::get()->loadSettings();
    
    // Listen for setting changes
    listenForSettingChanges("enabled", [](bool value) {
        TimeRewindManager::get()->setEnabled(value);
        log::info("[TimeRewind] Enabled: {}", value);
    });
    
    listenForSettingChanges("max-charges", [](int64_t value) {
        TimeRewindManager::get()->setMaxCharges(static_cast<int>(value));
        log::info("[TimeRewind] Max charges: {}", value);
    });
    
    listenForSettingChanges("rewind-duration", [](double value) {
        TimeRewindManager::get()->setRewindDuration(static_cast<float>(value));
        log::info("[TimeRewind] Rewind duration: {:.1f}s", value);
    });
    
    listenForSettingChanges("rewind-speed", [](double value) {
        TimeRewindManager::get()->setRewindSpeed(static_cast<float>(value));
        log::info("[TimeRewind] Rewind speed: {:.1f}x", value);
    });
    
    listenForSettingChanges("visual-effects", [](bool value) {
        TimeRewindManager::get()->setVisualEffects(value);
        RewindVisuals::get()->setEffectsEnabled(value);
        log::info("[TimeRewind] Visual effects: {}", value);
    });
    
    listenForSettingChanges("audio-sync", [](bool value) {
        TimeRewindManager::get()->setAudioSync(value);
        log::info("[TimeRewind] Audio sync: {}", value);
    });
    
    log::info("[TimeRewind] Initialization complete!");
}
