#include <Geode/Geode.hpp>
#include <Geode/modify/MenuLayer.hpp>
#include <Geode/modify/PauseLayer.hpp>
#include <Geode/modify/PlayLayer.hpp>

#include "GroqAPI.hpp"
#include "AIAssistant.hpp"
#include "ChatUI.hpp"

using namespace geode::prelude;

// ============================================================
// MenuLayer - Add button to main menu
// ============================================================

class $modify(AIMenuLayer, MenuLayer) {
    bool init() {
        if (!MenuLayer::init()) return false;
        
        auto menu = CCMenu::create();
        menu->setPosition({0, 0});
        this->addChild(menu, 100);
        
        // Create button sprite with fallback
        CCNode* btnSprite = nullptr;
        
        auto circleSpr = CircleButtonSprite::createWithSpriteFrameName(
            "gj_chatBtn_001.png", 1.0f, 
            CircleBaseColor::Green, CircleBaseSize::Small
        );
        
        if (circleSpr) {
            btnSprite = circleSpr;
        } else {
            btnSprite = ButtonSprite::create("AI", "goldFont.fnt", "GJ_button_01.png", 0.8f);
        }
        
        auto btn = CCMenuItemSpriteExtra::create(
            btnSprite,
            this,
            menu_selector(AIMenuLayer::onAI)
        );
        
        auto winSize = CCDirector::sharedDirector()->getWinSize();
        btn->setPosition({winSize.width - 30.0f, 30.0f});
        menu->addChild(btn);
        
        return true;
    }
    
    void onAI(CCObject*) {
        if (!GroqAPI::get()->hasApiKey()) {
            FLAlertLayer::create(
                "API Key Needed",
                "Set your <cg>Groq API key</c> in mod settings.\n\nGet one free at <cy>console.groq.com</c>",
                "OK"
            )->show();
            return;
        }
        
        ChatUI::open();
    }
};

// ============================================================
// PauseLayer - Add button to pause menu
// ============================================================

class $modify(AIPauseLayer, PauseLayer) {
    void customSetup() {
        PauseLayer::customSetup();
        
        auto menu = CCMenu::create();
        menu->setPosition({0, 0});
        this->addChild(menu, 100);
        
        auto spr = ButtonSprite::create("AI", "goldFont.fnt", "GJ_button_04.png", 0.8f);
        auto btn = CCMenuItemSpriteExtra::create(
            spr,
            this,
            menu_selector(AIPauseLayer::onAI)
        );
        
        auto winSize = CCDirector::sharedDirector()->getWinSize();
        btn->setPosition({winSize.width - 40.0f, 40.0f});
        btn->setScale(0.7f);
        menu->addChild(btn);
    }
    
    void onAI(CCObject*) {
        if (!GroqAPI::get()->hasApiKey()) {
            FLAlertLayer::create(
                "API Key Needed",
                "Set your Groq API key in mod settings.",
                "OK"
            )->show();
            return;
        }
        
        ChatUI::open();
    }
};

// ============================================================
// PlayLayer - Track deaths
// ============================================================

class $modify(AIPlayLayer, PlayLayer) {
    bool init(GJGameLevel* level, bool useReplay, bool dontCreateObjects) {
        if (!PlayLayer::init(level, useReplay, dontCreateObjects)) {
            return false;
        }
        
        if (level) {
            AIAssistant::get()->onLevelStart(std::string(level->m_levelName));
        }
        
        return true;
    }
    
    void destroyPlayer(PlayerObject* player, GameObject* obj) {
        if (player && m_levelLength > 0) {
            float pct = (player->getPositionX() / m_levelLength) * 100.0f;
            
            std::string mode = "cube";
            if (player->m_isShip) mode = "ship";
            else if (player->m_isBall) mode = "ball";
            else if (player->m_isBird) mode = "ufo";
            else if (player->m_isDart) mode = "wave";
            else if (player->m_isRobot) mode = "robot";
            else if (player->m_isSpider) mode = "spider";
            else if (player->m_isSwing) mode = "swing";
            
            AIAssistant::get()->onDeath(pct, mode);
        }
        
        PlayLayer::destroyPlayer(player, obj);
    }
    
    void onQuit() {
        AIAssistant::get()->onLevelEnd();
        PlayLayer::onQuit();
    }
};

// ============================================================
// Mod Init
// ============================================================

$on_mod(Loaded) {
    log::info("[AI Assistant] Mod loaded!");
    
    GroqAPI::get()->loadSettings();
    AIAssistant::get()->loadSettings();
    
    listenForSettingChanges("api-key", [](std::string value) {
        GroqAPI::get()->setApiKey(value);
    });
    
    listenForSettingChanges("model", [](std::string value) {
        GroqAPI::get()->setModel(value);
    });
    
    listenForSettingChanges("temperature", [](double value) {
        GroqAPI::get()->setTemperature(static_cast<float>(value));
    });
    
    listenForSettingChanges("max-tokens", [](int64_t value) {
        GroqAPI::get()->setMaxTokens(static_cast<int>(value));
    });
}
