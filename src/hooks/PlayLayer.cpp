#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include "../GhostRecorder.hpp"
#include "../GhostNode.hpp"

using namespace geode::prelude;

class $modify(GhostPlayLayer, PlayLayer) {
    struct Fields {
        GhostNode* ghostNode = nullptr;
        bool ghostInitialized = false;
    };
    
    // Вызывается при инициализации уровня
    bool init(GJGameLevel* level, bool useReplay, bool dontCreateObjects) {
        if (!PlayLayer::init(level, useReplay, dontCreateObjects)) {
            return false;
        }
        
        auto settings = getGhostSettings(); // Получаем из настроек мода
        if (!settings.enabled) return true;
        
        // Загружаем запись для этого уровня
        std::string levelID = std::to_string(level->m_levelID);
        GhostRecorder::get()->loadFromFile(levelID);
        
        // Создаём призрака если есть запись
        if (GhostRecorder::get()->hasRecording()) {
            m_fields->ghostNode = GhostNode::create(
                &GhostRecorder::get()->getBestRecording(), 
                settings
            );
            
            if (m_fields->ghostNode) {
                // Добавляем в тот же слой что и игрок
                m_objectLayer->addChild(m_fields->ghostNode, 10); // z-order
                m_fields->ghostInitialized = true;
                
                log::info("Ghost: Призрак создан для уровня {}", levelID);
            }
        }
        
        // Начинаем запись текущей попытки
        GhostRecorder::get()->startRecording(levelID);
        
        return true;
    }
    
    // Вызывается каждый кадр
    void update(float dt) {
        PlayLayer::update(dt);
        
        auto settings = getGhostSettings();
        if (!settings.enabled) return;
        
        // Записываем текущий кадр
        if (m_player1) {
            GhostRecorder::get()->recordFrame(m_player1, dt);
        }
        
        // Обновляем призрака
        if (m_fields->ghostNode && m_fields->ghostInitialized) {
            m_fields->ghostNode->updateGhost(dt);
            
            // Опционально: скрываем призрака если игрок его обогнал
            if (settings.showOnlyIfBetter && m_player1) {
                bool playerAhead = m_player1->getPositionX() > m_fields->ghostNode->getPositionX();
                m_fields->ghostNode->setVisible(!playerAhead && !m_fields->ghostNode->isFinished());
            }
        }
    }
    
    // Вызывается при смерти
    void destroyPlayer(PlayerObject* player, GameObject* obj) {
        // Сначала вычисляем процент
        float percent = this->getCurrentPercent();
        
        // Останавливаем запись
        GhostRecorder::get()->stopRecording(static_cast<int>(percent));
        
        PlayLayer::destroyPlayer(player, obj);
    }
    
    // Вызывается при респауне
    void resetLevel() {
        PlayLayer::resetLevel();
        
        auto settings = getGhostSettings();
        if (!settings.enabled) return;
        
        // Сбрасываем призрака на начало
        if (m_fields->ghostNode) {
            m_fields->ghostNode->reset();
        }
        
        // Начинаем новую запись
        if (m_level) {
            std::string levelID = std::to_string(m_level->m_levelID);
            GhostRecorder::get()->startRecording(levelID);
        }
    }
    
    // Вызывается при завершении уровня
    void levelComplete() {
        GhostRecorder::get()->stopRecording(100);
        PlayLayer::levelComplete();
    }
    
    // Получаем настройки из Geode
    GhostSettings getGhostSettings() {
        GhostSettings settings;
        settings.enabled = Mod::get()->getSettingValue<bool>("enabled");
        settings.ghostOpacity = static_cast<float>(Mod::get()->getSettingValue<int64_t>("opacity")) / 100.f;
        settings.showOnlyIfBetter = Mod::get()->getSettingValue<bool>("show-only-if-better");
        settings.recordPractice = Mod::get()->getSettingValue<bool>("record-practice");
        return settings;
    }
};
