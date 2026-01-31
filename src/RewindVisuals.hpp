#pragma once

#include <Geode/Geode.hpp>

using namespace geode::prelude;

class RewindVisuals {
private:
    RewindVisuals() = default;
    
public:
    static RewindVisuals* get();
    
    // Визуальные элементы
    CCLayerColor* m_tintLayer = nullptr;
    CCNode* m_effectContainer = nullptr;
    CCSprite* m_vhsLines = nullptr;
    std::vector<CCSprite*> m_ghostTrail;
    
    // Состояние
    bool m_active = false;
    float m_effectTime = 0.0f;
    PlayLayer* m_playLayer = nullptr;
    
    // Настройки эффектов
    ccColor3B m_rewindTint = ccc3(100, 150, 255);
    float m_chromaticAberration = 3.0f;
    float m_vhsIntensity = 0.5f;
    
    void startRewindEffect(PlayLayer* playLayer);
    void updateEffect(float progress);
    void stopRewindEffect();
    void cleanup();
    
private:
    void createTintOverlay();
    void createVHSEffect();
    void createGhostTrail();
    void updateGhostTrail(float progress);
    void updateVHSEffect(float progress);
};
