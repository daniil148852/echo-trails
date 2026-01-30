#include "GhostRecorder.hpp"

GhostRecorder* GhostRecorder::s_instance = nullptr;

void GhostRecorder::startRecording(const std::string& levelID) {
    m_currentRecording.clear();
    m_currentRecording.levelID = levelID;
    m_isRecording = true;
    m_recordTimer = 0.f;
    m_timeSinceLastFrame = 0.f;
    
    log::debug("Ghost: Начинаю запись для уровня {}", levelID);
}

void GhostRecorder::recordFrame(PlayerObject* player, float dt) {
    if (!m_isRecording || !player) return;
    
    m_recordTimer += dt;
    m_timeSinceLastFrame += dt;
    
    // Записываем не каждый кадр, а с интервалом (оптимизация)
    if (m_timeSinceLastFrame < RECORD_INTERVAL) return;
    m_timeSinceLastFrame = 0.f;
    
    GhostFrame frame;
    frame.timestamp = m_recordTimer;
    frame.xPos = player->getPositionX();
    frame.yPos = player->getPositionY();
    frame.rotation = player->getRotation();
    
    // Определяем игровой режим
    // В GD это хранится в PlayLayer или в самом PlayerObject
    frame.gameMode = static_cast<int>(player->m_vehicleSize); // упрощённо
    frame.isUpsideDown = player->m_isUpsideDown;
    frame.isMini = player->m_vehicleSize == 0.6f; // мини = 0.6, обычный = 1.0
    frame.isDashing = player->m_isDashing;
    frame.isHolding = player->m_isHolding;
    
    m_currentRecording.frames.push_back(frame);
}

void GhostRecorder::stopRecording(int percent) {
    if (!m_isRecording) return;
    
    m_isRecording = false;
    m_currentRecording.bestPercent = percent;
    m_currentRecording.totalTime = m_recordTimer;
    
    log::debug("Ghost: Запись остановлена. Процент: {}%, Кадров: {}", 
               percent, m_currentRecording.frames.size());
    
    // Сохраняем только если это лучшая попытка
    if (percent > m_bestRecording.bestPercent) {
        m_bestRecording = m_currentRecording;
        log::info("Ghost: Новый лучший результат! {}%", percent);
        
        // Сохраняем на диск
        saveToFile(m_currentRecording.levelID);
    }
}

void GhostRecorder::saveToFile(const std::string& levelID) {
    // Путь к файлу сохранения
    auto savePath = Mod::get()->getSaveDir() / (levelID + ".ghost");
    
    std::ofstream file(savePath, std::ios::binary);
    if (!file) {
        log::error("Ghost: Не удалось сохранить файл");
        return;
    }
    
    // Простой бинарный формат
    size_t frameCount = m_bestRecording.frames.size();
    file.write(reinterpret_cast<char*>(&m_bestRecording.bestPercent), sizeof(int));
    file.write(reinterpret_cast<char*>(&frameCount), sizeof(size_t));
    
    for (const auto& frame : m_bestRecording.frames) {
        file.write(reinterpret_cast<const char*>(&frame), sizeof(GhostFrame));
    }
    
    log::info("Ghost: Сохранено {} кадров", frameCount);
}

void GhostRecorder::loadFromFile(const std::string& levelID) {
    auto savePath = Mod::get()->getSaveDir() / (levelID + ".ghost");
    
    std::ifstream file(savePath, std::ios::binary);
    if (!file) {
        log::debug("Ghost: Нет сохранённой записи для уровня {}", levelID);
        return;
    }
    
    m_bestRecording.clear();
    m_bestRecording.levelID = levelID;
    
    size_t frameCount;
    file.read(reinterpret_cast<char*>(&m_bestRecording.bestPercent), sizeof(int));
    file.read(reinterpret_cast<char*>(&frameCount), sizeof(size_t));
    
    m_bestRecording.frames.resize(frameCount);
    for (size_t i = 0; i < frameCount; i++) {
        file.read(reinterpret_cast<char*>(&m_bestRecording.frames[i]), sizeof(GhostFrame));
    }
    
    log::info("Ghost: Загружено {} кадров, лучший результат: {}%", 
              frameCount, m_bestRecording.bestPercent);
}

void GhostRecorder::clearRecordings() {
    m_currentRecording.clear();
    m_bestRecording.clear();
}
