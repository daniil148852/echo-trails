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
    std::vector<CCNode*> m_vhsLines;
    std::vector<CCNode*> m_glitchBars;
    
    // Состояние
    bool m_active = false;
    float m_effectTime = 0.0f;
    PlayLayer* m_playLayer = nullptr;
    
    void startRewindEffect(PlayLayer* playLayer);
    void updateEffect(float progress);
    void stopRewindEffect();
    void cleanup();
    
private:
    void createTintOverlay();
    void createVHSEffect();
    void updateVHSLines();
};
