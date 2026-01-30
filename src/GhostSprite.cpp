#include "GhostSprite.hpp"

using namespace cocos2d;

namespace EchoTrails {

GhostSprite* GhostSprite::create(const PlayerVisuals& visuals) {
    auto* ret = new GhostSprite();
    if (ret && ret->init(visuals)) {
        ret->autorelease();
        return ret;
    }
    CC_SAFE_DELETE(ret);
    return nullptr;
}

bool GhostSprite::init(const PlayerVisuals& visuals) {
    if (!CCNode::init()) return false;
    
    m_visuals = visuals;
    createPlayerSprites();
    
    // Применяем начальные настройки
    auto ghostColor = Mod::get()->getSettingValue<ccColor4B>("ghost-color");
    setGhostColor(ccc3(ghostColor.r, ghostColor.g, ghostColor.b));
    setGhostOpacity(Mod::get()->getSettingValue<double>("ghost-opacity"));
    
    updateGameMode(GameMode::Cube);
    
    return true;
}

void GhostSprite::createPlayerSprites() {
    // Создаем простые спрайты для каждого режима
    // В реальной реализации нужно использовать правильные текстуры иконок
    
    auto createModeSprite = [this](const std::string& frameName) -> CCSprite* {
        auto* sprite = CCSprite::createWithSpriteFrameName(frameName.c_str());
        if (!sprite) {
            // Fallback на простой квадрат
            sprite = CCSprite::create("square.png");
        }
        if (sprite) {
            sprite->setVisible(false);
            this->addChild(sprite);
        }
        return sprite;
    };
    
    m_cubeSprite = createModeSprite(fmt::format("player_{:02d}_001.png", m_visuals.iconID));
    m_shipSprite = createModeSprite(fmt::format("ship_{:02d}_001.png", m_visuals.shipID));
    m_ballSprite = createModeSprite(fmt::format("player_ball_{:02d}_001.png", m_visuals.ballID));
    m_ufoSprite = createModeSprite(fmt::format("bird_{:02d}_001.png", m_visuals.ufoID));
    m_waveSprite = createModeSprite(fmt::format("dart_{:02d}_001.png", m_visuals.waveID));
    m_robotSprite = createModeSprite(fmt::format("robot_{:02d}_001.png", m_visuals.robotID));
    m_spiderSprite = createModeSprite(fmt::format("spider_{:02d}_001.png", m_visuals.spiderID));
    m_swingSprite = createModeSprite(fmt::format("swing_{:02d}_001.png", m_visuals.swingID));
}

void GhostSprite::hideAllModes() {
    if (m_cubeSprite) m_cubeSprite->setVisible(false);
    if (m_shipSprite) m_shipSprite->setVisible(false);
    if (m_ballSprite) m_ballSprite->setVisible(false);
    if (m_ufoSprite) m_ufoSprite->setVisible(false);
    if (m_waveSprite) m_waveSprite->setVisible(false);
    if (m_robotSprite) m_robotSprite->setVisible(false);
    if (m_spiderSprite) m_spiderSprite->setVisible(false);
    if (m_swingSprite) m_swingSprite->setVisible(false);
}

void GhostSprite::updateGameMode(GameMode mode) {
    if (m_currentMode == mode) return;
    
    hideAllModes();
    m_currentMode = mode;
    
    CCSprite* activeSprite = nullptr;
    
    switch (mode) {
        case GameMode::Cube: activeSprite = m_cubeSprite; break;
        case GameMode::Ship: activeSprite = m_shipSprite; break;
        case GameMode::Ball: activeSprite = m_ballSprite; break;
        case GameMode::UFO: activeSprite = m_ufoSprite; break;
        case GameMode::Wave: activeSprite = m_waveSprite; break;
        case GameMode::Robot: activeSprite = m_robotSprite; break;
        case GameMode::Spider: activeSprite = m_spiderSprite; break;
        case GameMode::Swing: activeSprite = m_swingSprite; break;
    }
    
    if (activeSprite) {
        activeSprite->setVisible(true);
    }
}

void GhostSprite::updateFromFrame(const GhostFrame& frame) {
    setPosition(ccp(frame.posX, frame.posY));
    setRotation(frame.rotation);
    setScaleX(frame.scaleX);
    setScaleY(frame.scaleY);
    setVisible(frame.isVisible);
    
    updateGameMode(frame.gameMode);
    setMini(frame.isMini);
    setUpsideDown(frame.isUpsideDown);
}

void GhostSprite::setGhostOpacity(float opacity) {
    m_opacity = opacity;
    GLubyte opacityValue = static_cast<GLubyte>(opacity * 255);
    
    auto setChildOpacity = [opacityValue](CCSprite* sprite) {
        if (sprite) {
            sprite->setOpacity(opacityValue);
        }
    };
    
    setChildOpacity(m_cubeSprite);
    setChildOpacity(m_shipSprite);
    setChildOpacity(m_ballSprite);
    setChildOpacity(m_ufoSprite);
    setChildOpacity(m_waveSprite);
    setChildOpacity(m_robotSprite);
    setChildOpacity(m_spiderSprite);
    setChildOpacity(m_swingSprite);
}

void GhostSprite::setGhostColor(const ccColor3B& color) {
    auto setChildColor = [&color](CCSprite* sprite) {
        if (sprite) {
            sprite->setColor(color);
        }
    };
    
    setChildColor(m_cubeSprite);
    setChildColor(m_shipSprite);
    setChildColor(m_ballSprite);
    setChildColor(m_ufoSprite);
    setChildColor(m_waveSprite);
    setChildColor(m_robotSprite);
    setChildColor(m_spiderSprite);
    setChildColor(m_swingSprite);
}

void GhostSprite::setMini(bool mini) {
    if (m_isMini == mini) return;
    m_isMini = mini;
    
    float scale = mini ? 0.6f : 1.0f;
    setScale(scale);
}

void GhostSprite::setUpsideDown(bool upsideDown) {
    if (m_isUpsideDown == upsideDown) return;
    m_isUpsideDown = upsideDown;
    
    setScaleY(upsideDown ? -std::abs(getScaleY()) : std::abs(getScaleY()));
}

} // namespace EchoTrails
