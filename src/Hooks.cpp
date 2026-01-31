#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/PlayerObject.hpp>
#include <Geode/modify/PauseLayer.hpp>
#include "TimeRewindManager.hpp"

using namespace geode::prelude;

// ============================================================================
// PLAYLAYER HOOKS
// ============================================================================

class $modify(RewindPlayLayer, PlayLayer) {
    
    bool init(GJGameLevel* level, bool useReplay, bool dontCreateObjects) {
        if (!PlayLayer::init(level, useReplay, dontCreateObjects)) {
            return false;
        }
        
        TimeRewindManager::get()->initialize(this);
        
        return true;
    }
    
    void update(float dt) {
        auto* manager = TimeRewindManager::get();
        
        // Если идёт отмотка — не вызываем базовый update
        if (manager->isRewinding()) {
            manager->update(dt);
            
            // Обновляем только необходимые визуалы
            this->updateVisibility();
            return;
        }
        
        PlayLayer::update(dt);
        
        // Записываем кадр
        manager->update(dt);
    }
    
    void resetLevel() {
        TimeRewindManager::get()->reset();
        PlayLayer::resetLevel();
    }
    
    void onQuit() {
        TimeRewindManager::get()->cleanup();
        PlayLayer::onQuit();
    }
    
    void levelComplete() {
        // При завершении уровня отключаем отмотку
        TimeRewindManager::get()->cleanup();
        PlayLayer::levelComplete();
    }
    
    void destroyPlayer(PlayerObject* player, GameObject* obj) {
        auto* manager = TimeRewindManager::get();
        
        // Если можем отмотать — делаем это вместо смерти
        if (manager->canRewind()) {
            manager->startRewind();
            return; // Не вызываем базовый destroyPlayer!
        }
        
        // Иначе — обычная смерть
        PlayLayer::destroyPlayer(player, obj);
    }
};

// ============================================================================
// PLAYEROBJECT HOOKS (для дополнительного контроля)
// ============================================================================

class $modify(RewindPlayerObject, PlayerObject) {
    
    void playerDestroyed(bool p0) {
        auto* manager = TimeRewindManager::get();
        
        // Если идёт отмотка — игнорируем смерть
        if (manager->isRewinding()) {
            return;
        }
        
        // Проверяем можем ли отмотать
        if (manager->canRewind()) {
            // Не вызываем базовый метод — отмотка запустится в destroyPlayer
            return;
        }
        
        PlayerObject::playerDestroyed(p0);
    }
};

// ============================================================================
// PAUSE LAYER — кнопка отмотки и настройки
// ============================================================================

class $modify(RewindPauseLayer, PauseLayer) {
    
    void customSetup() {
        PauseLayer::customSetup();
        
        auto* manager = TimeRewindManager::get();
        if (!manager->m_enabled) return;
        
        auto winSize = CCDirector::sharedDirector()->getWinSize();
        
        // Информация об отмотках
        std::string info = manager->m_infiniteRewinds ?
            "Rewinds: ∞" :
            fmt::format("Rewinds left: {}", manager->m_rewindsRemaining);
        
        auto* infoLabel = CCLabelBMFont::create(info.c_str(), "bigFont.fnt");
        infoLabel->setPosition(ccp(winSize.width / 2, 50.0f));
        infoLabel->setScale(0.4f);
        infoLabel->setColor(ccc3(100, 200, 255));
        this->addChild(infoLabel, 100);
        
        // Кнопка настроек мода
        auto* menu = CCMenu::create();
        menu->setPosition(CCPointZero);
        this->addChild(menu, 100);
        
        auto* settingsBtn = CCMenuItemSpriteExtra::create(
            CCSprite::createWithSpriteFrameName("GJ_optionsBtn_001.png"),
            this,
            menu_selector(RewindPauseLayer::onRewindSettings)
        );
        settingsBtn->setPosition(ccp(winSize.width - 40.0f, 40.0f));
        settingsBtn->setScale(0.6f);
        menu->addChild(settingsBtn);
        
        // Подпись
        auto* btnLabel = CCLabelBMFont::create("REWIND", "bigFont.fnt");
        btnLabel->setPosition(settingsBtn->getContentSize() / 2 + CCSize(0, -20.0f));
        btnLabel->setScale(0.35f);
        settingsBtn->addChild(btnLabel);
    }
    
    void onRewindSettings(CCObject* sender) {
        geode::openSettingsPopup(Mod::get());
    }
};
