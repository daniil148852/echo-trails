#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/PlayerObject.hpp>
#include <Geode/modify/PauseLayer.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>

#include "TimeRewindManager.hpp"
#include "RewindVisuals.hpp"

using namespace geode::prelude;
using namespace TimeRewind;

// ==================== PlayLayer Hooks ====================

class $modify(TimeRewindPlayLayer, PlayLayer) {
    
    struct Fields {
        bool m_rewindInputBlocked = false;
    };
    
    // Hook init to set up the rewind manager
    bool init(GJGameLevel* level, bool useReplay, bool dontCreateObjects) {
        if (!PlayLayer::init(level, useReplay, dontCreateObjects)) {
            return false;
        }
        
        // Initialize the rewind manager
        TimeRewindManager::get()->init(this);
        
        // Initialize visuals
        RewindVisuals::get()->init(this);
        
        log::info("TimeRewind mod initialized for level: {}", level->m_levelName);
        
        return true;
    }
    
    // Hook postUpdate for recording (important for 2.2 variable TPS)
    void postUpdate(float dt) {
        PlayLayer::postUpdate(dt);
        
        auto manager = TimeRewindManager::get();
        
        // Update rewind if active
        if (manager->isRewinding()) {
            manager->updateRewind(dt);
            return;
        }
        
        // Record frame state
        manager->recordFrame(dt);
    }
    
    // Hook destroyPlayer to intercept death
    void destroyPlayer(PlayerObject* player, GameObject* obj) {
        auto manager = TimeRewindManager::get();
        
        // Check if we can rewind instead of dying
        if (manager->canRewind()) {
            log::info("Death intercepted - starting rewind");
            
            // Start rewind instead of dying
            if (manager->startRewind()) {
                // Block the death sequence
                // Reset player death flags
                if (m_player1) {
                    m_player1->m_isDead = false;
                }
                if (m_player2) {
                    m_player2->m_isDead = false;
                }
                
                // Play rewind sound effect
                FMODAudioEngine::sharedEngine()->playEffect("rewind.ogg", 1.0f, 1.0f, 0.5f);
                
                // Don't call original - skip death
                return;
            }
        }
        
        // No rewind available or rewind failed, proceed with normal death
        PlayLayer::destroyPlayer(player, obj);
    }
    
    // Hook resetLevel to clear the buffer
    void resetLevel() {
        // Clear rewind buffer before reset
        auto manager = TimeRewindManager::get();
        
        // Cancel any active rewind
        if (manager->isRewinding()) {
            manager->cancelRewind();
        }
        
        // Reset the manager state
        manager->reset();
        
        // Update UI
        RewindVisuals::get()->updateCharges(
            manager->getCharges(), 
            manager->getMaxCharges()
        );
        
        PlayLayer::resetLevel();
        
        log::debug("Level reset - rewind buffer cleared");
    }
    
    // Hook levelComplete to cleanup
    void levelComplete() {
        // Cleanup rewind state on completion
        TimeRewindManager::get()->reset();
        
        PlayLayer::levelComplete();
    }
    
    // Hook onQuit for cleanup
    void onQuit() {
        // Clean up managers
        TimeRewindManager::destroy();
        RewindVisuals::destroy();
        
        PlayLayer::onQuit();
        
        log::info("TimeRewind mod cleanup complete");
    }
    
    // Override update to handle rewind state
    void update(float dt) {
        auto manager = TimeRewindManager::get();
        
        // If rewinding, we need to skip normal physics updates
        if (manager->isRewinding()) {
            // Only update essential systems during rewind
            // Skip physics/gameplay update
            
            // Still update visuals and UI
            if (m_uiLayer) {
                m_uiLayer->update(dt);
            }
            
            return;
        }
        
        // Normal update
        PlayLayer::update(dt);
    }
    
    // Hook checkCollisions to prevent collision checks during rewind
    void checkCollisions(PlayerObject* player, float dt, bool p2) {
        if (TimeRewindManager::get()->isRewinding()) {
            return; // Skip collision checks during rewind
        }
        
        PlayLayer::checkCollisions(player, dt, p2);
    }
};

// ==================== PlayerObject Hooks ====================

class $modify(TimeRewindPlayerObject, PlayerObject) {
    
    // Block input during rewind
    void pushButton(PlayerButton btn) {
        if (TimeRewindManager::get()->isRewinding()) {
            return; // Block input during rewind
        }
        
        PlayerObject::pushButton(btn);
    }
    
    void releaseButton(PlayerButton btn) {
        if (TimeRewindManager::get()->isRewinding()) {
            return; // Block input during rewind
        }
        
        PlayerObject::releaseButton(btn);
    }
    
    // Block gravity changes during rewind
    void updateJump(float dt) {
        if (TimeRewindManager::get()->isRewinding()) {
            return; // Skip jump physics during rewind
        }
        
        PlayerObject::updateJump(dt);
    }
};

// ==================== GJBaseGameLayer Hooks ====================

class $modify(TimeRewindGJBaseGameLayer, GJBaseGameLayer) {
    
    // Hook player movement to prevent during rewind
    void updateCamera(float dt) {
        if (TimeRewindManager::get()->isRewinding()) {
            // During rewind, camera is controlled by the manager
            return;
        }
        
        GJBaseGameLayer::updateCamera(dt);
    }
};

// ==================== PauseLayer Hooks ====================

class $modify(TimeRewindPauseLayer, PauseLayer) {
    
    void customSetup() {
        PauseLayer::customSetup();
        
        auto manager = TimeRewindManager::get();
        auto winSize = CCDirector::sharedDirector()->getWinSize();
        
        // Add rewind info to pause menu
        std::string infoText = fmt::format(
            "Rewind Charges: {}/{}",
            manager->getCharges(),
            manager->getMaxCharges()
        );
        
        auto rewindLabel = CCLabelBMFont::create(infoText.c_str(), "bigFont.fnt");
        rewindLabel->setScale(0.4f);
        rewindLabel->setPosition(ccp(winSize.width / 2, 30));
        rewindLabel->setColor(ccc3(100, 200, 255));
        this->addChild(rewindLabel, 100);
        
        // Add manual rewind button (optional feature)
        auto rewindBtn = CCMenuItemSpriteExtra::create(
            CCSprite::createWithSpriteFrameName("GJ_undoBtn_001.png"),
            this,
            menu_selector(TimeRewindPauseLayer::onManualRewind)
        );
        
        // Find the menu and add the button
        if (auto menu = this->getChildByType<CCMenu>(0)) {
            rewindBtn->setPosition(ccp(-winSize.width / 2 + 40, -winSize.height / 2 + 40));
            menu->addChild(rewindBtn);
        }
    }
    
    void onManualRewind(CCObject* sender) {
        // Close pause menu and trigger rewind
        auto manager = TimeRewindManager::get();
        
        if (manager->canRewind()) {
            // Resume game first
            this->onResume(sender);
            
            // Then start rewind after a small delay
            auto playLayer = PlayLayer::get();
            if (playLayer) {
                playLayer->scheduleOnce(schedule_selector(TimeRewindPauseLayer::triggerRewindDelayed), 0.1f);
            }
        } else {
            // Show "no rewinds" message
            FLAlertLayer::create(
                "Time Rewind",
                "No rewind charges remaining!",
                "OK"
            )->show();
        }
    }
    
    void triggerRewindDelayed(float dt) {
        TimeRewindManager::get()->startRewind();
    }
};

// ==================== Mod Entry Point ====================

$on_mod(Loaded) {
    log::info("Time Rewind Mod loaded!");
    log::info("Version: {}", Mod::get()->getVersion().toVString());
    
    // Pre-load settings
    auto mod = Mod::get();
    
    log::info("Settings - Charges: {}, Duration: {}, Speed: {}", 
        mod->getSettingValue<int64_t>("rewind-charges"),
        mod->getSettingValue<double>("rewind-duration"),
        mod->getSettingValue<double>("rewind-speed")
    );
}

$on_mod(Unloaded) {
    // Cleanup
    TimeRewindManager::destroy();
    RewindVisuals::destroy();
    
    log::info("Time Rewind Mod unloaded!");
}
