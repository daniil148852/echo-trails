#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/PlayerObject.hpp>
#include <Geode/modify/PauseLayer.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>

#include "TimeRewindManager.hpp"
#include "RewindVisuals.hpp"

using namespace geode::prelude;
using namespace TimeRewind;

class $modify(TimeRewindPlayLayer, PlayLayer) {
    
    struct Fields {
        bool m_rewindInputBlocked = false;
    };
    
    bool init(GJGameLevel* level, bool useReplay, bool dontCreateObjects) {
        if (!PlayLayer::init(level, useReplay, dontCreateObjects)) {
            return false;
        }
        
        TimeRewindManager::get()->init(this);
        RewindVisuals::get()->init(this);
        
        log::info("TimeRewind mod initialized for level: {}", level->m_levelName);
        
        return true;
    }
    
    void postUpdate(float dt) {
        PlayLayer::postUpdate(dt);
        
        auto manager = TimeRewindManager::get();
        
        if (manager->isRewinding()) {
            manager->updateRewind(dt);
            return;
        }
        
        manager->recordFrame(dt);
    }
    
    void destroyPlayer(PlayerObject* player, GameObject* obj) {
        auto manager = TimeRewindManager::get();
        
        if (manager->canRewind()) {
            log::info("Death intercepted - starting rewind");
            
            if (manager->startRewind()) {
                if (m_player1) {
                    m_player1->m_isDead = false;
                }
                if (m_player2) {
                    m_player2->m_isDead = false;
                }
                
                return;
            }
        }
        
        PlayLayer::destroyPlayer(player, obj);
    }
    
    void resetLevel() {
        auto manager = TimeRewindManager::get();
        
        if (manager->isRewinding()) {
            manager->cancelRewind();
        }
        
        manager->reset();
        
        RewindVisuals::get()->updateCharges(
            manager->getCharges(), 
            manager->getMaxCharges()
        );
        
        PlayLayer::resetLevel();
        
        log::debug("Level reset - rewind buffer cleared");
    }
    
    void levelComplete() {
        TimeRewindManager::get()->reset();
        PlayLayer::levelComplete();
    }
    
    void onQuit() {
        TimeRewindManager::destroy();
        RewindVisuals::destroy();
        
        PlayLayer::onQuit();
        
        log::info("TimeRewind mod cleanup complete");
    }
    
    void update(float dt) {
        auto manager = TimeRewindManager::get();
        
        if (manager->isRewinding()) {
            if (m_uiLayer) {
                m_uiLayer->update(dt);
            }
            return;
        }
        
        PlayLayer::update(dt);
    }
    
    void checkCollisions(PlayerObject* player, float dt, bool p2) {
        if (TimeRewindManager::get()->isRewinding()) {
            return;
        }
        
        PlayLayer::checkCollisions(player, dt, p2);
    }
};

class $modify(TimeRewindPlayerObject, PlayerObject) {
    
    void pushButton(PlayerButton btn) {
        if (TimeRewindManager::get()->isRewinding()) {
            return;
        }
        
        PlayerObject::pushButton(btn);
    }
    
    void releaseButton(PlayerButton btn) {
        if (TimeRewindManager::get()->isRewinding()) {
            return;
        }
        
        PlayerObject::releaseButton(btn);
    }
    
    void updateJump(float dt) {
        if (TimeRewindManager::get()->isRewinding()) {
            return;
        }
        
        PlayerObject::updateJump(dt);
    }
};

class $modify(TimeRewindGJBaseGameLayer, GJBaseGameLayer) {
    
    void updateCamera(float dt) {
        if (TimeRewindManager::get()->isRewinding()) {
            return;
        }
        
        GJBaseGameLayer::updateCamera(dt);
    }
};

class $modify(TimeRewindPauseLayer, PauseLayer) {
    
    void customSetup() {
        PauseLayer::customSetup();
        
        auto manager = TimeRewindManager::get();
        auto winSize = CCDirector::sharedDirector()->getWinSize();
        
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
    }
};

$on_mod(Loaded) {
    log::info("Time Rewind Mod loaded!");
    log::info("Version: {}", Mod::get()->getVersion().toVString());
    
    auto mod = Mod::get();
    
    log::info("Settings - Charges: {}, Duration: {}, Speed: {}", 
        mod->getSettingValue<int64_t>("rewind-charges"),
        mod->getSettingValue<double>("rewind-duration"),
        mod->getSettingValue<double>("rewind-speed")
    );
}
