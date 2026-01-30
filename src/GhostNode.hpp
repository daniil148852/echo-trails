#pragma once
#include <Geode/Geode.hpp>
#include "GhostData.hpp"

using namespace geode::prelude;

// Визуальное представление призрака
class GhostNode : public CCNode {
private:
    CCSprite* m_iconSprite = nullptr;    // Основной спрайт
    CCSprite* m_glowSprite = nullptr;    // Свечение (опционально)
    
    GhostRecording* m_recording = nullptr;
    size_t m_currentFrameIndex = 0;
    float m_playbackTime = 0.f;
    bool m_isPlaying = false;
    bool m_finished = false;
    
    GhostSettings m_settings;
    
    bool init(GhostRecording* recording, const GhostSettings& settings);
    
public:
    static GhostNode* create(GhostRecording* recording, const GhostSettings& settings);
    
    // Обновление каждый кадр
    void updateGhost(float dt);
    
    // Сброс на начало
    void reset();
    
    // Установить иконку как у игрока
    void setupIcon(int iconID, int color1, int color2);
    
    // Применить кадр к спрайту
    void applyFrame(const GhostFrame& frame);
    
    // Интерполяция между кадрами для плавности
    GhostFrame interpolateFrames(const GhostFrame& a, const GhostFrame& b, float t);
    
    bool isFinished() const { return m_finished; }
};
