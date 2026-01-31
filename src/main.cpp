#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/PauseLayer.hpp>
#include <Geode/modify/LevelInfoLayer.hpp>
#include <Geode/modify/MenuLayer.hpp>

#include "GroqAPI.hpp"
#include "AIAssistant.hpp"
#include "ChatPopup.hpp"

using namespace geode::prelude;

// ============================================================
// PlayLayer Hooks
// ============================================================

class $modify(AIPlayLayer, PlayLayer) {
    struct Fields {
        int m_lastDeathPercent = -1;
    };
    
    bool init(GJGameLevel* level, bool useReplay, bool dontCreateObjects) {
        if (!PlayLayer::init(level, useReplay, dontCreateObjects)) {
            return false;
        }
        
        AIAssistant::get()->onLevelStart(level);
        
        return true;
    }
    
    void destroyPlayer(PlayerObject* player, GameObject* object) {
        // Track death before calling original
        float xPos = player->getPositionX();
        float percentage = (xPos / m_levelLength) * 100.0f;
        
        AIAssistant::get()->onPlayerDeath(player, xPos, percentage);
        
        // Check for auto-tips
        auto assistant = AIAssistant::get();
        if (assistant->getAutoTips()) {
            int roundedPercent = static_cast<int>(percentage);
            
            // Show tip popup if died at same spot multiple times
            // This is handled in AIAssistant based on threshold
        }
        
        PlayLayer::destroyPlayer(player, object);
    }
    
    void resetLevel() {
        AIAssistant::get()->onLevelReset();
        PlayLayer::resetLevel();
    }
    
    void levelComplete() {
        AIAssistant::get()->onLevelComplete();
        PlayLayer::levelComplete();
    }
    
    void onQuit() {
        AIAssistant::get()->onLevelEnd();
        PlayLayer::onQuit();
    }
};

// ============================================================
// PauseLayer Hooks - Add AI button to pause menu
// ============================================================

class $modify(AIPauseLayer, PauseLayer) {
    void customSetup() {
        PauseLayer::customSetup();
        
        // Add AI Assistant button
        auto menu = this->getChildByID("left-button-menu");
        if (!menu) {
            menu = CCMenu::create();
            menu->setPosition({0, 0});
            this->addChild(menu);
        }
        
        auto sprite = CircleButtonSprite::createWithSpriteFrameName(
            "gj_chatBtn_001.png", 1.0f,
            CircleBaseColor::Green, CircleBaseSize::Medium
        );
        
        auto btn = CCMenuItemSpriteExtra::create(
            sprite,
            this,
            menu_selector(AIPauseLayer::onAIAssistant)
        );
        
        auto winSize = CCDirector::sharedDirector()->getWinSize();
        btn->setPosition({winSize.width - 50.0f, 50.0f});
        btn->setID("ai-assistant-btn"_spr);
        
        if (auto leftMenu = typeinfo_cast<CCMenu*>(menu)) {
            leftMenu->addChild(btn);
        }
    }
    
    void onAIAssistant(CCObject* sender) {
        auto popup = ChatPopup::create();
        if (popup) {
            popup->show();
        }
    }
};

// ============================================================
// LevelInfoLayer Hooks - Add AI button to level info
// ============================================================

class $modify(AILevelInfoLayer, LevelInfoLayer) {
    bool init(GJGameLevel* level, bool challenge) {
        if (!LevelInfoLayer::init(level, challenge)) {
            return false;
        }
        
        // Add AI button
        auto menu = CCMenu::create();
        menu->setPosition({0, 0});
        this->addChild(menu);
        
        auto sprite = CircleButtonSprite::createWithSpriteFrameName(
            "gj_chatBtn_001.png", 1.0f,
            CircleBaseColor::Green, CircleBaseSize::Small
        );
        
        auto btn = CCMenuItemSpriteExtra::create(
            sprite,
            this,
            menu_selector(AILevelInfoLayer::onAIAssistant)
        );
        
        auto winSize = CCDirector::sharedDirector()->getWinSize();
        btn->setPosition({winSize.width - 30.0f, winSize.height - 30.0f});
        btn->setID("ai-assistant-btn"_spr);
        
        menu->addChild(btn);
        
        return true;
    }
    
    void onAIAssistant(CCObject* sender) {
        auto popup = ChatPopup::create();
        if (popup) {
            popup->show();
        }
    }
};

// ============================================================
// MenuLayer Hooks - Add AI button to main menu
// ============================================================

class $modify(AIMenuLayer, MenuLayer) {
    bool init() {
        if (!MenuLayer::init()) {
            return false;
        }
        
        // Add AI button to bottom right
        auto menu = CCMenu::create();
        menu->setPosition({0, 0});
        this->addChild(menu);
        
        auto sprite = CircleButtonSprite::createWithSpriteFrameName(
            "gj_chatBtn_001.png", 1.0f,
            CircleBaseColor::Cyan, CircleBaseSize::Medium
        );
        
        auto btn = CCMenuItemSpriteExtra::create(
            sprite,
            this,
            menu_selector(AIMenuLayer::onAIAssistant)
        );
        
        auto winSize = CCDirector::sharedDirector()->getWinSize();
        btn->setPosition({winSize.width - 40.0f, 40.0f});
        btn->setID("ai-assistant-btn"_spr);
        
        menu->addChild(btn);
        
        return true;
    }
    
    void onAIAssistant(CCObject* sender) {
        if (!GroqAPI::get()->hasApiKey()) {
            FLAlertLayer::create(
                "API Key Required",
                "Please set your <cg>Groq API key</c> in the mod settings to use the AI Assistant.\n\n"
                "Get a free key at <cy>console.groq.com</c>",
                "OK"
            )->show();
            return;
        }
        
        auto popup = ChatPopup::create();
        if (popup) {
            popup->show();
        }
    }
};

// ============================================================
// Mod Lifecycle
// ============================================================

$execute {
    log::info("========================================");
    log::info("   AI Assistant Mod v1.0.0 Loaded!");
    log::info("========================================");
}

$on_mod(Loaded) {
    log::info("[AIAssistant] Mod loaded, initializing...");
    
    // Initialize managers
    GroqAPI::get()->loadSettings();
    AIAssistant::get()->loadSettings();
    
    // Listen for setting changes
    listenForSettingChanges("api-key", [](std::string value) {
        GroqAPI::get()->setApiKey(value);
        log::info("[AIAssistant] API key updated");
    });
    
    listenForSettingChanges("model", [](std::string value) {
        GroqAPI::get()->setDefaultModel(value);
        log::info("[AIAssistant] Model changed to: {}", value);
    });
    
    listenForSettingChanges("temperature", [](double value) {
        GroqAPI::get()->setDefaultTemperature(static_cast<float>(value));
    });
    
    listenForSettingChanges("max-tokens", [](int64_t value) {
        GroqAPI::get()->setDefaultMaxTokens(static_cast<int>(value));
    });
    
    listenForSettingChanges("auto-tips", [](bool value) {
        AIAssistant::get()->setAutoTips(value);
    });
    
    listenForSettingChanges("death-threshold", [](int64_t value) {
        AIAssistant::get()->setDeathThreshold(static_cast<int>(value));
    });
    
    log::info("[AIAssistant] Initialization complete!");
}
