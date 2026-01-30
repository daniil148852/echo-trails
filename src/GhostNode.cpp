#include "GhostNode.hpp"

GhostNode* GhostNode::create(GhostRecording* recording, const GhostSettings& settings) {
    auto ret = new GhostNode();
    if (ret && ret->init(recording, settings)) {
        ret->autorelease();
        return ret;
    }
    CC_SAFE_DELETE(ret);
    return nullptr;
}

bool GhostNode::init(GhostRecording* recording, const GhostSettings& settings) {
    if (!CCNode::init()) return false;
    
    m_recording = recording;
    m_settings = settings;
    
    // Создаём спрайт призрака
    // Используем простой квадрат как заглушку, потом можно заменить на иконку
    m_iconSprite = CCSprite::create("player_ball_01_001.png"); // базовая иконка
    if (!m_iconSprite) {
        // Fallback на простой квадрат
        m_iconSprite = CCSprite::create("square.png");
    }
    
    if (m_iconSprite) {
        m_iconSprite->setColor(m_settings.ghostColor);
        m_iconSprite->setOpacity(static_cast<GLubyte>(m_settings.ghostOpacity * 255));
        this->addChild(m_iconSprite);
    }
    
    // Опционально: добавляем свечение
    m_glowSprite = CCSprite::create("square.png");
    if (m_glowSprite) {
        m_glowSprite->setScale(1.5f);
        m_glowSprite->setColor(m_settings.ghostColor);
        m_glowSprite->setOpacity(static_cast<GLubyte>(m_settings.ghostOpacity * 100));
        this->addChild(m_glowSprite, -1);
    }
    
    return true;
}

void GhostNode::reset() {
    m_currentFrameIndex = 0;
    m_playbackTime = 0.f;
    m_isPlaying = true;
    m_finished = false;
    this->setVisible(true);
}

void GhostNode::updateGhost(float dt) {
    if (!m_isPlaying || m_finished || !m_recording || m_recording->frames.empty()) {
        return;
    }
    
    m_playbackTime += dt;
    
    // Ищем нужные кадры для текущего времени
    while (m_currentFrameIndex < m_recording->frames.size() - 1) {
        const auto& nextFrame = m_recording->frames[m_currentFrameIndex + 1];
        if (nextFrame.timestamp > m_playbackTime) break;
        m_currentFrameIndex++;
    }
    
    // Проверяем конец записи
    if (m_currentFrameIndex >= m_recording->frames.size() - 1) {
        m_finished = true;
        this->setVisible(false);
        return;
    }
    
    // Интерполяция для плавности
    const auto& frameA = m_recording->frames[m_currentFrameIndex];
    const auto& frameB = m_recording->frames[m_currentFrameIndex + 1];
    
    float frameDuration = frameB.timestamp - frameA.timestamp;
    float t = (frameDuration > 0) ? (m_playbackTime - frameA.timestamp) / frameDuration : 0.f;
    t = std::clamp(t, 0.f, 1.f);
    
    GhostFrame interpolated = interpolateFrames(frameA, frameB, t);
    applyFrame(interpolated);
}

GhostFrame GhostNode::interpolateFrames(const GhostFrame& a, const GhostFrame& b, float t) {
    GhostFrame result;
    
    // Линейная интерполяция позиции
    result.xPos = a.xPos + (b.xPos - a.xPos) * t;
    result.yPos = a.yPos + (b.yPos - a.yPos) * t;
    
    // Интерполяция угла (с учётом перехода через 360°)
    float angleDiff = b.rotation - a.rotation;
    if (angleDiff > 180.f) angleDiff -= 360.f;
    if (angleDiff < -180.f) angleDiff += 360.f;
    result.rotation = a.rotation + angleDiff * t;
    
    // Дискретные значения берём от ближайшего кадра
    result.gameMode = (t < 0.5f) ? a.gameMode : b.gameMode;
    result.isUpsideDown = (t < 0.5f) ? a.isUpsideDown : b.isUpsideDown;
    result.isMini = (t < 0.5f) ? a.isMini : b.isMini;
    
    return result;
}

void GhostNode::applyFrame(const GhostFrame& frame) {
    this->setPosition(ccp(frame.xPos, frame.yPos));
    
    if (m_iconSprite) {
        m_iconSprite->setRotation(frame.rotation);
        
        // Масштаб для мини-режима
        float scale = frame.isMini ? 0.6f : 1.0f;
        m_iconSprite->setScale(scale);
        
        // Переворот
        m_iconSprite->setFlipY(frame.isUpsideDown);
    }
    
    if (m_glowSprite) {
        m_glowSprite->setRotation(frame.rotation);
    }
}

void GhostNode::setupIcon(int iconID, int color1, int color2) {
    // Здесь можно загрузить реальную иконку игрока
    // Пока используем базовый спрайт с цветом
    // TODO: Использовать SimplePlayer или аналог
}
