#include "GhostPlayer.hpp"
#include <Geode/loader/Mod.hpp>

namespace EchoTrails {

GhostPlayer& GhostPlayer::get() {
    static GhostPlayer instance;
    return instance;
}

bool GhostPlayer::loadGhostForLevel(const std::string& levelID) {
    unloadGhost();
    
    auto path = getRecordingPath(levelID);
    if (!std::filesystem::exists(path)) {
        log::debug("EchoTrails: No ghost recording found for level {}", levelID);
        return false;
    }
    
    auto recording = GhostRecording::loadFromFile(path);
    if (!recording) {
        log::error("EchoTrails: Failed to load ghost recording from {}", path.string());
        return false;
    }
    
    m_recording = std::move(*recording);
    m_hasGhost = true;
    
    log::info("EchoTrails: Loaded ghost for level {}. Frames: {}, Best: {}%",
              m_recording.levelName, m_recording.frameCount, m_recording.bestPercent);
    
    return true;
}

void GhostPlayer::unloadGhost() {
    m_hasGhost = false;
    m_isPlaying = false;
    m_recording = GhostRecording();
    removeSprite();
}

void GhostPlayer::createSprite(cocos2d::CCNode* parent) {
    if (!m_hasGhost || !parent) return;
    
    removeSprite();
    
    m_sprite = GhostSprite::create(m_recording.visuals);
    if (m_sprite) {
        m_sprite->setZOrder(-1); // Позади игрока
        parent->addChild(m_sprite);
    }
    
    // Создаем второго игрока если dual
    if (!m_recording.player2Frames.empty()) {
        m_player2Sprite = GhostSprite::create(m_recording.visuals);
        if (m_player2Sprite) {
            m_player2Sprite->setZOrder(-1);
            parent->addChild(m_player2Sprite);
        }
    }
}

void GhostPlayer::removeSprite() {
    if (m_sprite) {
        m_sprite->removeFromParent();
        m_sprite = nullptr;
    }
    if (m_player2Sprite) {
        m_player2Sprite->removeFromParent();
        m_player2Sprite = nullptr;
    }
}

void GhostPlayer::update(float time) {
    if (!m_isPlaying || !m_hasGhost || m_isPaused) return;
    
    m_currentTime = time;
    
    // Получаем интерполированный кадр
    GhostFrame frame = getInterpolatedFrame(time);
    
    if (m_sprite) {
        m_sprite->updateFromFrame(frame);
    }
    
    // Обновляем второго игрока
    if (m_player2Sprite && !m_recording.player2Frames.empty()) {
        // Используем ту же логику для player2
        size_t frameIndex = static_cast<size_t>(time * m_recording.fps);
        if (frameIndex < m_recording.player2Frames.size()) {
            m_player2Sprite->updateFromFrame(m_recording.player2Frames[frameIndex]);
        }
    }
}

GhostFrame GhostPlayer::getInterpolatedFrame(float time) {
    if (m_recording.frames.empty()) {
        return GhostFrame();
    }
    
    // Вычисляем индекс кадра
    float frameTime = time * m_recording.fps;
    size_t frameIndex = static_cast<size_t>(frameTime);
    float t = frameTime - static_cast<float>(frameIndex);
    
    // Проверка границ
    if (frameIndex >= m_recording.frames.size() - 1) {
        return m_recording.frames.back();
    }
    
    // Интерполяция между кадрами
    const GhostFrame& frameA = m_recording.frames[frameIndex];
    const GhostFrame& frameB = m_recording.frames[frameIndex + 1];
    
    return interpolateFrames(frameA, frameB, t);
}

void GhostPlayer::reset() {
    m_currentTime = 0.0f;
    m_currentFrameIndex = 0;
    
    // Сбрасываем спрайт на начальную позицию
    if (m_hasGhost && m_sprite && !m_recording.frames.empty()) {
        m_sprite->updateFromFrame(m_recording.frames[0]);
    }
    if (m_player2Sprite && !m_recording.player2Frames.empty()) {
        m_player2Sprite->updateFromFrame(m_recording.player2Frames[0]);
    }
}

void GhostPlayer::start() {
    if (!m_hasGhost) return;
    m_isPlaying = true;
    m_isPaused = false;
    reset();
}

void GhostPlayer::stop() {
    m_isPlaying = false;
    m_isPaused = false;
}

void GhostPlayer::pause() {
    m_isPaused = true;
}

void GhostPlayer::resume() {
    m_isPaused = false;
}

std::filesystem::path GhostPlayer::getRecordingPath(const std::string& levelID) {
    return Mod::get()->getSaveDir() / "ghosts" / (levelID + ".ghost");
}

} // namespace EchoTrails
