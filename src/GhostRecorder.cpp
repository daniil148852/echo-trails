#include "GhostRecorder.hpp"
#include <Geode/Geode.hpp>

using namespace geode::prelude;

GhostRecorder* GhostRecorder::s_instance = nullptr;

GhostRecorder* GhostRecorder::get() {
    if (!s_instance) {
        s_instance = new GhostRecorder();
    }
    return s_instance;
}

void GhostRecorder::startRecording(int levelID) {
    m_isRecording = true;
    m_currentRecording = GhostData();
    m_currentRecording.levelID = levelID;
    m_currentRecording.frames.clear();
    m_lastXPos = 0.0f;
    
    log::debug("Начата запись для уровня {}", levelID);
}

void GhostRecorder::stopRecording() {
    m_isRecording = false;
    log::debug("Запись остановлена. Записано {} кадров", m_currentRecording.frames.size());
}

void GhostRecorder::recordFrame(PlayerObject* player, float xPos) {
    if (!m_isRecording || !player) return;
    
    GhostFrame frame;
    frame.xPos = xPos;
    frame.yPos = player->getPositionY();
    frame.rotation = player->getRotation();
    frame.yScale = player->getScaleY();
    
    // Вместо m_isHolding используем другие способы:
    // Вариант 1: Отслеживаем изменение Y позиции (прыжок = движение вверх)
    // Вариант 2: Просто не записываем это поле (оно не критично для визуала)
    frame.isHolding = false; // Убираем, не критично для отображения призрака
    
    // Записываем состояния игрока из доступных полей
    frame.isDashing = player->m_isDashing;
    frame.isUpsideDown = player->m_isUpsideDown;
    frame.isShip = player->m_isShip;
    frame.isBall = player->m_isBall;
    frame.isUFO = player->m_isBird;  // UFO в коде называется Bird
    frame.isWave = player->m_isDart; // Wave в коде называется Dart
    frame.isRobot = player->m_isRobot;
    frame.isSpider = player->m_isSpider;
    frame.isSwing = player->m_isSwing;
    frame.isMini = player->m_vehicleSize != 1.0f; // Мини режим
    
    m_currentRecording.frames.push_back(frame);
}

void GhostRecorder::saveGhost(const std::string& filename) {
    if (m_currentRecording.frames.empty()) {
        log::warn("Нет данных для сохранения");
        return;
    }
    
    auto path = Mod::get()->getSaveDir() / (filename + ".ghost");
    
    std::ofstream file(path, std::ios::binary);
    if (!file.is_open()) {
        log::error("Не удалось открыть файл для записи: {}", path.string());
        return;
    }
    
    // Записываем заголовок
    file.write(reinterpret_cast<const char*>(&m_currentRecording.levelID), sizeof(int));
    
    size_t frameCount = m_currentRecording.frames.size();
    file.write(reinterpret_cast<const char*>(&frameCount), sizeof(size_t));
    
    // Записываем кадры
    for (const auto& frame : m_currentRecording.frames) {
        file.write(reinterpret_cast<const char*>(&frame), sizeof(GhostFrame));
    }
    
    file.close();
    log::info("Призрак сохранён: {}", path.string());
}

bool GhostRecorder::loadGhost(const std::string& filename, GhostData& outData) {
    auto path = Mod::get()->getSaveDir() / (filename + ".ghost");
    
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        log::debug("Файл не найден: {}", path.string());
        return false;
    }
    
    // Читаем заголовок
    file.read(reinterpret_cast<char*>(&outData.levelID), sizeof(int));
    
    size_t frameCount;
    file.read(reinterpret_cast<char*>(&frameCount), sizeof(size_t));
    
    // Читаем кадры
    outData.frames.resize(frameCount);
    for (size_t i = 0; i < frameCount; i++) {
        file.read(reinterpret_cast<char*>(&outData.frames[i]), sizeof(GhostFrame));
    }
    
    file.close();
    log::info("Призрак загружен: {} кадров", frameCount);
    return true;
}

bool GhostRecorder::loadBestGhost(int levelID, GhostData& outData) {
    std::string filename = fmt::format("ghost_{}_best", levelID);
    return loadGhost(filename, outData);
}

void GhostRecorder::saveBestGhost(int levelID) {
    std::string filename = fmt::format("ghost_{}_best", levelID);
    saveGhost(filename);
}
