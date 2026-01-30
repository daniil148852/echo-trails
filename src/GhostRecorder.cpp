#include "GhostRecorder.hpp"
#include <Geode/loader/Mod.hpp>
#include <chrono>

namespace EchoTrails {

GhostRecorder& GhostRecorder::get() {
    static GhostRecorder instance;
    return instance;
}

void GhostRecorder::startRecording(GJGameLevel* level) {
    reset();
    
    m_isRecording = true;
    m_recordingStartTime = 0.0f;
    
    m_currentRecording.levelID = std::to_string(level->m_levelID.value());
    m_currentRecording.levelName = level->m_levelName;
    m_currentRecording.bestPercent = 0;
    m_currentRecording.frameCount = 0;
    m_currentRecording.fps = 60.0f; // Будет обновлено
    
    // Получаем визуал игрока
    auto* gm = GameManager::sharedState();
    m_currentRecording.visuals = {
        .iconID = gm->getPlayerFrame(),
        .shipID = gm->getPlayerShip(),
        .ballID = gm->getPlayerBall(),
        .ufoID = gm->getPlayerBird(),
        .waveID = gm->getPlayerDart(),
        .robotID = gm->getPlayerRobot(),
        .spiderID = gm->getPlayerSpider(),
        .swingID = gm->getPlayerSwing(),
        .color1 = gm->getPlayerColor(),
        .color2 = gm->getPlayerColor2(),
        .glowColor = gm->getPlayerGlowColor(),
        .hasGlow = gm->getPlayerGlow()
    };
    
    m_currentRecording.timestamp = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
    
    log::info("EchoTrails: Started recording for level {}", m_currentRecording.levelName);
}

void GhostRecorder::recordFrame(PlayerObject* player, bool isPlayer2) {
    if (!m_isRecording || !player) return;
    
    GhostFrame frame = capturePlayerState(player, isPlayer2);
    
    if (isPlayer2) {
        m_currentRecording.player2Frames.push_back(frame);
    } else {
        m_currentRecording.frames.push_back(frame);
        m_currentRecording.frameCount = m_currentRecording.frames.size();
    }
    
    m_hasLastFrame = true;
    m_lastFrame = frame;
}

GhostFrame GhostRecorder::capturePlayerState(PlayerObject* player, bool isPlayer2) {
    GhostFrame frame;
    
    frame.posX = player->getPositionX();
    frame.posY = player->getPositionY();
    frame.rotation = player->getRotation();
    frame.scaleX = player->getScaleX();
    frame.scaleY = player->getScaleY();
    
    frame.gameMode = getPlayerGameMode(player);
    frame.isUpsideDown = player->m_isUpsideDown;
    frame.isVisible = player->isVisible();
    frame.isDashing = player->m_isDashing;
    frame.isMini = player->m_vehicleSize != 1.0f;
    frame.isDual = false; // Будет установлено в PlayLayer
    frame.isPlayer2 = isPlayer2;
    
    return frame;
}

GameMode GhostRecorder::getPlayerGameMode(PlayerObject* player) {
    if (player->m_isShip) return GameMode::Ship;
    if (player->m_isBall) return GameMode::Ball;
    if (player->m_isBird) return GameMode::UFO;
    if (player->m_isDart) return GameMode::Wave;
    if (player->m_isRobot) return GameMode::Robot;
    if (player->m_isSpider) return GameMode::Spider;
    if (player->m_isSwing) return GameMode::Swing;
    return GameMode::Cube;
}

void GhostRecorder::stopRecording() {
    if (!m_isRecording) return;
    
    m_isRecording = false;
    m_currentRecording.bestPercent = m_currentPercent;
    
    log::info("EchoTrails: Stopped recording. Frames: {}, Progress: {}%", 
              m_currentRecording.frameCount, m_currentPercent);
}

void GhostRecorder::reset() {
    m_currentRecording.frames.clear();
    m_currentRecording.player2Frames.clear();
    m_currentRecording.frameCount = 0;
    m_currentPercent = 0;
    m_hasLastFrame = false;
    m_recordingStartTime = 0.0f;
}

bool GhostRecorder::shouldSaveRecording() const {
    auto recordMode = Mod::get()->getSettingValue<std::string>("record-mode");
    
    if (recordMode == "always") {
        return true;
    } else if (recordMode == "completion-only") {
        return m_currentPercent >= 100;
    } else { // best-progress
        // Загружаем существующую запись и сравниваем
        auto existingPath = getRecordingPath(m_currentRecording.levelID);
        if (std::filesystem::exists(existingPath)) {
            if (auto existing = GhostRecording::loadFromFile(existingPath)) {
                return m_currentPercent > existing->bestPercent;
            }
        }
        return m_currentPercent > 0;
    }
}

bool GhostRecorder::saveCurrentRecording() {
    if (m_currentRecording.frames.empty()) return false;
    
    auto path = getRecordingPath(m_currentRecording.levelID);
    
    // Создаем директорию если не существует
    auto dir = path.parent_path();
    if (!std::filesystem::exists(dir)) {
        std::filesystem::create_directories(dir);
    }
    
    bool success = m_currentRecording.saveToFile(path);
    if (success) {
        log::info("EchoTrails: Saved ghost recording to {}", path.string());
    } else {
        log::error("EchoTrails: Failed to save ghost recording");
    }
    
    return success;
}

// Теперь const метод
std::filesystem::path GhostRecorder::getRecordingPath(const std::string& levelID) const {
    return Mod::get()->getSaveDir() / "ghosts" / (levelID + ".ghost");
}

} // namespace EchoTrails
