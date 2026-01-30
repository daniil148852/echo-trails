#pragma once

#include "GhostData.hpp"
#include <Geode/Geode.hpp>

namespace EchoTrails {

class GhostSprite : public cocos2d::CCNode {
public:
    static GhostSprite* create(const PlayerVisuals& visuals);
    bool init(const PlayerVisuals& visuals);
    
    void updateFromFrame(const GhostFrame& frame);
    void setGhostOpacity(float opacity);
    void setGhostColor(const cocos2d::ccColor3B& color);
    
    void updateGameMode(GameMode mode);
    void setMini(bool mini);
    void setUpsideDown(bool upsideDown);
    
private:
    void createPlayerSprites();
    void hideAllModes();
    cocos2d::CCSprite* createIconSprite(const std::string& frameName);
    std::string getIconFrameName(GameMode mode, int iconID);
    
    PlayerVisuals m_visuals;
    GameMode m_currentMode = GameMode::Cube;
    float m_opacity = 0.5f;
    bool m_isMini = false;
    bool m_isUpsideDown = false;
    
    // Спрайты для разных режимов
    cocos2d::CCSprite* m_cubeSprite = nullptr;
    cocos2d::CCSprite* m_shipSprite = nullptr;
    cocos2d::CCSprite* m_ballSprite = nullptr;
    cocos2d::CCSprite* m_ufoSprite = nullptr;
    cocos2d::CCSprite* m_waveSprite = nullptr;
    cocos2d::CCSprite* m_robotSprite = nullptr;
    cocos2d::CCSprite* m_spiderSprite = nullptr;
    cocos2d::CCSprite* m_swingSprite = nullptr;
    
    // Дополнительные элементы
    cocos2d::CCSprite* m_glowSprite = nullptr;
    cocos2d::CCSprite* m_secondarySprite = nullptr; // Для second color
};

} // namespace EchoTrails
