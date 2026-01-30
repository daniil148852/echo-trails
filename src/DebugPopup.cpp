#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/MenuLayer.hpp>

using namespace geode::prelude;

// Показываем popup при заходе в уровень
class $modify(DebugPlayLayer, PlayLayer) {
    
    bool init(GJGameLevel* level, bool useReplay, bool dontCreateObjects) {
        if (!PlayLayer::init(level, useReplay, dontCreateObjects)) {
            return false;
        }
        
        // Простая табличка
        auto popup = FLAlertLayer::create(
            "Echo Trails", 
            "Мод работает!\n\nУровень: " + std::string(level->m_levelName) + 
            "\nID: " + std::to_string(level->m_levelID.value()),
            "OK"
        );
        popup->show();
        
        // Также добавим метку на экран
        auto label = CCLabelBMFont::create("Echo Trails Active!", "bigFont.fnt");
        label->setPosition(ccp(
            CCDirector::sharedDirector()->getWinSize().width / 2,
            CCDirector::sharedDirector()->getWinSize().height - 30
        ));
        label->setScale(0.5f);
        label->setZOrder(9999);
        label->setColor(ccc3(100, 255, 100)); // Зелёный
        label->setOpacity(200);
        this->addChild(label);
        
        log::info("=== ECHO TRAILS DEBUG ===");
        log::info("Level: {}", std::string(level->m_levelName));
        log::info("Level ID: {}", level->m_levelID.value());
        log::info("Player1: {}", m_player1 != nullptr);
        log::info("ObjectLayer: {}", m_objectLayer != nullptr);
        log::info("=========================");
        
        return true;
    }
};

// Показываем popup при заходе в главное меню (для проверки загрузки мода)
class $modify(DebugMenuLayer, MenuLayer) {
    
    bool init() {
        if (!MenuLayer::init()) {
            return false;
        }
        
        // Метка в углу экрана
        auto label = CCLabelBMFont::create("Echo Trails v1.0", "goldFont.fnt");
        label->setAnchorPoint(ccp(0, 0));
        label->setPosition(ccp(10, 10));
        label->setScale(0.5f);
        label->setZOrder(9999);
        this->addChild(label);
        
        log::info("Echo Trails: MenuLayer loaded!");
        
        return true;
    }
};
