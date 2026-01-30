#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/PauseLayer.hpp>
#include <Geode/modify/EndLevelLayer.hpp>

#include "GhostManager.hpp"

using namespace geode::prelude;
using namespace EchoTrails;

class $modify(EchoPlayLayer, PlayLayer) {
    struct Fields {
        float m_gameTime = 0.0f;
        bool m_hasStarted = false;
        int m_lastPercent = 0;
    };
    
    // Вспомогательная функция для проверки dual mode
    bool isDualMode() {
        // В GD 2.2074 проверяем наличие активного второго игрока
        return m_player2 != nullptr && m_player2->isVisible();
    }
    
    bool init(GJGameLevel* level, bool useReplay, bool dontCreateObjects) {
        if (!PlayLayer::init(level, useReplay, dontCreateObjects)) {
            return false;
        }
        
        if (!GhostMgr.isEnabled()) {
            return true;
        }
        
        // Загружаем существующего призрака
        std::string levelID = std::to_string(level->m_levelID.value());
        GhostPlay.loadGhostForLevel(levelID);
        
        // Создаем спрайт призрака
        if (GhostPlay.hasGhost()) {
            // Добавляем в слой объектов
            if (auto* objectLayer = m_objectLayer) {
                GhostPlay.createSprite(objectLayer);
            }
        }
        
        // Начинаем запись
        GhostRec.startRecording(level);
        
        m_fields->m_gameTime = 0.0f;
        m_fields->m_hasStarted = false;
        m_fields->m_lastPercent = 0;
        
        log::info("EchoTrails: Initialized for level '{}'", level->m_levelName);
        
        return true;
    }
    
    void update(float dt) {
        PlayLayer::update(dt);
        
        if (!GhostMgr.isEnabled()) return;
        
        // Обновляем время игры
        if (!m_isPaused && !m_hasCompletedLevel) {
            m_fields->m_gameTime += dt;
            
            // Начало игры (после старта)
            if (!m_fields->m_hasStarted && m_player1 && 
                m_player1->getPositionX() > 0) {
                m_fields->m_hasStarted = true;
                GhostPlay.start();
            }
        }
        
        // Записываем кадр
        if (GhostRec.isRecording() && m_player1 && m_fields->m_hasStarted) {
            GhostRec.recordFrame(m_player1, false);
            
            // Записываем второго игрока если dual mode
            if (isDualMode()) {
                GhostRec.recordFrame(m_player2, true);
            }
            
            // Обновляем процент
            int currentPercent = 0;
            if (m_levelLength > 0) {
                currentPercent = static_cast<int>(m_player1->getPositionX() / m_levelLength * 100.0f);
                currentPercent = std::clamp(currentPercent, 0, 100);
            }
            
            if (currentPercent > m_fields->m_lastPercent) {
                m_fields->m_lastPercent = currentPercent;
                GhostRec.setCurrentPercent(currentPercent);
            }
        }
        
        // Воспроизводим призрака
        if (GhostPlay.isPlaying() && m_fields->m_hasStarted) {
            GhostPlay.update(m_fields->m_gameTime);
        }
    }
    
    void resetLevel() {
        // Сохраняем текущую запись если нужно
        if (GhostRec.isRecording() && GhostRec.shouldSaveRecording()) {
            GhostRec.stopRecording();
            GhostRec.saveCurrentRecording();
        }
        
        PlayLayer::resetLevel();
        
        if (!GhostMgr.isEnabled()) return;
        
        // Сбрасываем состояние
        m_fields->m_gameTime = 0.0f;
        m_fields->m_hasStarted = false;
        m_fields->m_lastPercent = 0;
        
        // Перезапускаем запись
        if (m_level) {
            GhostRec.startRecording(m_level);
        }
        
        // Сбрасываем призрака
        GhostPlay.reset();
        
        log::debug("EchoTrails: Level reset");
    }
    
    void levelComplete() {
        PlayLayer::levelComplete();
        
        if (!GhostMgr.isEnabled()) return;
        
        // Устанавливаем 100% и сохраняем
        GhostRec.setCurrentPercent(100);
        GhostRec.stopRecording();
        
        if (GhostRec.shouldSaveRecording()) {
            GhostRec.saveCurrentRecording();
            
            // Перезагружаем призрака с новой записью
            std::string levelID = std::to_string(m_level->m_levelID.value());
            GhostPlay.loadGhostForLevel(levelID);
        }
        
        GhostPlay.stop();
        
        log::info("EchoTrails: Level completed!");
    }
    
    void onQuit() {
        // Сохраняем перед выходом если нужно
        if (GhostRec.isRecording() && GhostRec.shouldSaveRecording()) {
            GhostRec.stopRecording();
            GhostRec.saveCurrentRecording();
        }
        
        GhostPlay.unloadGhost();
        GhostRec.reset();
        
        PlayLayer::onQuit();
    }
    
    void pauseGame(bool pause) {
        PlayLayer::pauseGame(pause);
        
        if (pause) {
            GhostPlay.pause();
        } else {
            GhostPlay.resume();
        }
    }
};

// Хук на паузу для дополнительных настроек
class $modify(EchoPauseLayer, PauseLayer) {
    void customSetup() {
        PauseLayer::customSetup();
        
        // Можно добавить кнопку настроек мода в меню паузы
    }
};

// Хук на экран завершения уровня
class $modify(EchoEndLevelLayer, EndLevelLayer) {
    void customSetup() {
        EndLevelLayer::customSetup();
        
        // Можно показать статистику призрака
        if (GhostPlay.hasGhost()) {
            auto* recording = GhostPlay.getRecording();
            if (recording) {
                log::info("EchoTrails: Ghost comparison - Previous best: {}%", 
                          recording->bestPercent);
            }
        }
    }
};

// Инициализация мода
$on_mod(Loaded) {
    log::info("EchoTrails mod loaded!");
    
    // Создаем директорию для сохранений
    auto saveDir = Mod::get()->getSaveDir() / "ghosts";
    if (!std::filesystem::exists(saveDir)) {
        std::filesystem::create_directories(saveDir);
    }
}
