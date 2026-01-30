#include <Geode/Geode.hpp>
#include <Geode/modify/PlayerObject.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <deque>
#include <cmath>

using namespace geode::prelude;

// ============================================================================
// 🎨 ECHO TRAIL SYSTEM
// ============================================================================

struct EchoSnapshot {
    CCPoint position;
    float rotation;
    float scale;
    double timestamp;
    bool isShip;
    bool isBall;
    bool isUfo;
    bool isWave;
    bool isRobot;
    bool isSpider;
    bool isSwing;
    bool flipX;
    bool flipY;
};

struct EchoGhost {
    CCSprite* sprite = nullptr;
    CCSprite* glowSprite = nullptr;
    float opacity = 255.0f;
    ccColor3B color = {255, 255, 255};
};

class EchoTrailManager {
private:
    static EchoTrailManager* s_instance;
    
    std::deque<EchoSnapshot> m_snapshots;
    std::vector<EchoGhost> m_ghosts;
    CCNode* m_trailContainer = nullptr;
    double m_lastSnapshotTime = 0.0;
    float m_pulsePhase = 0.0f;
    float m_rainbowHue = 0.0f;
    bool m_initialized = false;
    
public:
    static EchoTrailManager* get() {
        if (!s_instance) {
            s_instance = new EchoTrailManager();
        }
        return s_instance;
    }
    
    // Convert HSV to RGB for rainbow effect
    ccColor3B hsvToRgb(float h, float s, float v) {
        float c = v * s;
        float x = c * (1 - std::abs(std::fmod(h / 60.0f, 2) - 1));
        float m = v - c;
        
        float r, g, b;
        if (h < 60) { r = c; g = x; b = 0; }
        else if (h < 120) { r = x; g = c; b = 0; }
        else if (h < 180) { r = 0; g = c; b = x; }
        else if (h < 240) { r = 0; g = x; b = c; }
        else if (h < 300) { r = x; g = 0; b = c; }
        else { r = c; g = 0; b = x; }
        
        return {
            static_cast<GLubyte>((r + m) * 255),
            static_cast<GLubyte>((g + m) * 255),
            static_cast<GLubyte>((b + m) * 255)
        };
    }
    
    void initialize(CCLayer* gameLayer) {
        cleanup();
        
        m_trailContainer = CCNode::create();
        m_trailContainer->setZOrder(-10);
        gameLayer->addChild(m_trailContainer);
        
        int trailCount = Mod::get()->getSettingValue<int64_t>("trail-count");
        m_ghosts.resize(trailCount);
        
        for (int i = 0; i < trailCount; i++) {
            // Main ghost sprite
            auto ghost = CCSprite::create("playerSquare_001.png");
            if (!ghost) {
                ghost = CCSprite::create("square.png");
            }
            if (ghost) {
                ghost->setVisible(false);
                ghost->setBlendFunc({GL_SRC_ALPHA, GL_ONE}); // Additive blending for glow
                m_trailContainer->addChild(ghost, trailCount - i);
                m_ghosts[i].sprite = ghost;
                
                // Glow layer
                float glowIntensity = Mod::get()->getSettingValue<double>("glow-intensity");
                if (glowIntensity > 0) {
                    auto glow = CCSprite::create("playerSquare_001.png");
                    if (!glow) glow = CCSprite::create("square.png");
                    if (glow) {
                        glow->setVisible(false);
                        glow->setBlendFunc({GL_SRC_ALPHA, GL_ONE});
                        glow->setScale(1.3f);
                        glow->setOpacity(100);
                        m_trailContainer->addChild(glow, trailCount - i - 1);
                        m_ghosts[i].glowSprite = glow;
                    }
                }
            }
        }
        
        m_initialized = true;
        m_snapshots.clear();
        m_lastSnapshotTime = 0.0;
    }
    
    void cleanup() {
        if (m_trailContainer) {
            m_trailContainer->removeFromParent();
            m_trailContainer = nullptr;
        }
        m_ghosts.clear();
        m_snapshots.clear();
        m_initialized = false;
    }
    
    void captureSnapshot(PlayerObject* player, double currentTime) {
        if (!m_initialized || !player) return;
        if (!Mod::get()->getSettingValue<bool>("enabled")) return;
        
        float delay = Mod::get()->getSettingValue<double>("trail-delay");
        if (currentTime - m_lastSnapshotTime < delay) return;
        
        EchoSnapshot snapshot;
        snapshot.position = player->getPosition();
        snapshot.rotation = player->getRotation();
        snapshot.scale = player->getScale();
        snapshot.timestamp = currentTime;
        snapshot.isShip = player->m_isShip;
        snapshot.isBall = player->m_isBall;
        snapshot.isUfo = player->m_isBird;
        snapshot.isWave = player->m_isDart;
        snapshot.isRobot = player->m_isRobot;
        snapshot.isSpider = player->m_isSpider;
        snapshot.isSwing = player->m_isSwing;
        snapshot.flipX = player->isFlipX();
        snapshot.flipY = player->isFlipY();
        
        m_snapshots.push_front(snapshot);
        
        int maxSnapshots = Mod::get()->getSettingValue<int64_t>("trail-count") + 5;
        while (m_snapshots.size() > maxSnapshots) {
            m_snapshots.pop_back();
        }
        
        m_lastSnapshotTime = currentTime;
    }
    
    void updateTrails(float dt) {
        if (!m_initialized || !Mod::get()->getSettingValue<bool>("enabled")) {
            for (auto& ghost : m_ghosts) {
                if (ghost.sprite) ghost.sprite->setVisible(false);
                if (ghost.glowSprite) ghost.glowSprite->setVisible(false);
            }
            return;
        }
        
        int trailCount = m_ghosts.size();
        bool rainbowMode = Mod::get()->getSettingValue<bool>("rainbow-mode");
        bool pulseEffect = Mod::get()->getSettingValue<bool>("pulse-effect");
        int baseOpacity = Mod::get()->getSettingValue<int64_t>("base-opacity");
        float glowIntensity = Mod::get()->getSettingValue<double>("glow-intensity");
        std::string style = Mod::get()->getSettingValue<std::string>("trail-style");
        
        // Update animation phases
        m_pulsePhase += dt * 4.0f;
        m_rainbowHue += dt * 60.0f;
        if (m_rainbowHue >= 360.0f) m_rainbowHue -= 360.0f;
        
        for (int i = 0; i < trailCount; i++) {
            if (i >= m_snapshots.size()) {
                if (m_ghosts[i].sprite) m_ghosts[i].sprite->setVisible(false);
                if (m_ghosts[i].glowSprite) m_ghosts[i].glowSprite->setVisible(false);
                continue;
            }
            
            auto& snapshot = m_snapshots[i];
            auto& ghost = m_ghosts[i];
            
            if (!ghost.sprite) continue;
            
            ghost.sprite->setVisible(true);
            ghost.sprite->setPosition(snapshot.position);
            ghost.sprite->setRotation(snapshot.rotation);
            
            // Calculate opacity based on position in trail
            float progress = static_cast<float>(i) / static_cast<float>(trailCount);
            float opacity = baseOpacity * (1.0f - progress);
            
            // Apply style-specific modifications
            if (style == "sharp") {
                opacity = (i < trailCount / 2) ? baseOpacity * 0.8f : baseOpacity * 0.2f;
            } else if (style == "ethereal") {
                opacity *= 0.6f + 0.4f * std::sin(progress * 3.14159f);
            } else if (style == "neon") {
                opacity = baseOpacity * 0.9f;
            }
            
            // Pulse effect
            if (pulseEffect) {
                float pulse = 0.8f + 0.2f * std::sin(m_pulsePhase + i * 0.5f);
                opacity *= pulse;
                ghost.sprite->setScale(snapshot.scale * (0.95f + 0.1f * pulse));
            } else {
                ghost.sprite->setScale(snapshot.scale * (1.0f - progress * 0.3f));
            }
            
            ghost.sprite->setOpacity(static_cast<GLubyte>(std::max(0.0f, std::min(255.0f, opacity))));
            
            // Color calculation
            ccColor3B color;
            if (rainbowMode) {
                float hue = std::fmod(m_rainbowHue + i * (360.0f / trailCount), 360.0f);
                color = hsvToRgb(hue, 0.9f, 1.0f);
            } else {
                // Gradient from cyan to purple
                float t = progress;
                color = {
                    static_cast<GLubyte>(100 + 155 * t),
                    static_cast<GLubyte>(200 * (1 - t) + 100 * t),
                    static_cast<GLubyte>(255)
                };
            }
            ghost.sprite->setColor(color);
            
            // Update glow sprite
            if (ghost.glowSprite && glowIntensity > 0) {
                ghost.glowSprite->setVisible(true);
                ghost.glowSprite->setPosition(snapshot.position);
                ghost.glowSprite->setRotation(snapshot.rotation);
                ghost.glowSprite->setScale(ghost.sprite->getScale() * (1.2f + glowIntensity * 0.2f));
                ghost.glowSprite->setOpacity(static_cast<GLubyte>(opacity * 0.4f * glowIntensity));
                ghost.glowSprite->setColor(color);
            }
        }
    }
    
    void createDeathBurst(CCPoint position, CCLayer* layer) {
        if (!Mod::get()->getSettingValue<bool>("death-burst")) return;
        if (!layer) return;
        
        // Create spectacular particle burst
        int particleCount = 24;
        for (int i = 0; i < particleCount; i++) {
            auto particle = CCSprite::create("playerSquare_001.png");
            if (!particle) particle = CCSprite::create("square.png");
            if (!particle) continue;
            
            particle->setPosition(position);
            particle->setScale(0.3f + (rand() % 100) / 200.0f);
            particle->setBlendFunc({GL_SRC_ALPHA, GL_ONE});
            
            float hue = (360.0f / particleCount) * i;
            particle->setColor(hsvToRgb(hue, 1.0f, 1.0f));
            particle->setOpacity(200);
            particle->setZOrder(100);
            layer->addChild(particle);
            
            // Random direction
            float angle = (360.0f / particleCount) * i + (rand() % 30 - 15);
            float radians = angle * 3.14159f / 180.0f;
            float distance = 80.0f + rand() % 60;
            
            CCPoint targetPos = {
                position.x + std::cos(radians) * distance,
                position.y + std::sin(radians) * distance
            };
            
            // Animate particle
            auto move = CCEaseOut::create(CCMoveTo::create(0.6f, targetPos), 2.0f);
            auto scale = CCScaleTo::create(0.6f, 0.0f);
            auto fade = CCFadeOut::create(0.6f);
            auto rotate = CCRotateBy::create(0.6f, 360.0f + rand() % 360);
            auto spawn = CCSpawn::create(move, scale, fade, rotate, nullptr);
            auto remove = CCCallFunc::create(particle, callfunc_selector(CCSprite::removeFromParent));
            particle->runAction(CCSequence::create(spawn, remove, nullptr));
        }
        
        // Screen flash effect
        auto flash = CCLayerColor::create({255, 255, 255, 100});
        flash->setZOrder(1000);
        layer->addChild(flash);
        flash->runAction(CCSequence::create(
            CCFadeOut::create(0.3f),
            CCCallFunc::create(flash, callfunc_selector(CCLayerColor::removeFromParent)),
            nullptr
        ));
    }
    
    void createJumpFlash(CCPoint position, CCLayer* layer) {
        if (!Mod::get()->getSettingValue<bool>("jump-flash")) return;
        if (!layer) return;
        
        // Small flash ring on jump
        auto ring = CCSprite::create("playerSquare_001.png");
        if (!ring) ring = CCSprite::create("square.png");
        if (!ring) return;
        
        ring->setPosition(position);
        ring->setScale(0.5f);
        ring->setBlendFunc({GL_SRC_ALPHA, GL_ONE});
        ring->setColor(hsvToRgb(m_rainbowHue, 0.8f, 1.0f));
        ring->setOpacity(180);
        ring->setZOrder(50);
        layer->addChild(ring);
        
        auto scale = CCScaleTo::create(0.2f, 1.5f);
        auto fade = CCFadeOut::create(0.2f);
        auto spawn = CCSpawn::create(scale, fade, nullptr);
        auto remove = CCCallFunc::create(ring, callfunc_selector(CCSprite::removeFromParent));
        ring->runAction(CCSequence::create(spawn, remove, nullptr));
    }
    
    void reset() {
        m_snapshots.clear();
        m_lastSnapshotTime = 0.0;
        for (auto& ghost : m_ghosts) {
            if (ghost.sprite) ghost.sprite->setVisible(false);
            if (ghost.glowSprite) ghost.glowSprite->setVisible(false);
        }
    }
};

EchoTrailManager* EchoTrailManager::s_instance = nullptr;

// ============================================================================
// 🎮 GAME HOOKS
// ============================================================================

class $modify(EchoPlayLayer, PlayLayer) {
    
    bool init(GJGameLevel* level, bool useReplay, bool dontCreateObjects) {
        if (!PlayLayer::init(level, useReplay, dontCreateObjects)) {
            return false;
        }
        
        EchoTrailManager::get()->initialize(this);
        
        // Schedule update
        this->schedule(schedule_selector(EchoPlayLayer::echoUpdate));
        
        return true;
    }
    
    void echoUpdate(float dt) {
        EchoTrailManager::get()->updateTrails(dt);
    }
    
    void resetLevel() {
        PlayLayer::resetLevel();
        EchoTrailManager::get()->reset();
    }
    
    void onQuit() {
        EchoTrailManager::get()->cleanup();
        PlayLayer::onQuit();
    }
    
    void destroyPlayer(PlayerObject* player, GameObject* obj) {
        if (player) {
            EchoTrailManager::get()->createDeathBurst(player->getPosition(), this);
        }
        PlayLayer::destroyPlayer(player, obj);
    }
};

class $modify(EchoPlayerObject, PlayerObject) {
    
    void update(float dt) {
        PlayerObject::update(dt);
        
        auto playLayer = PlayLayer::get();
        if (playLayer && this == playLayer->m_player1) {
            double time = playLayer->m_gameState.m_currentProgress;
            EchoTrailManager::get()->captureSnapshot(this, time);
        }
    }
    
    void pushButton(PlayerButton btn) {
        PlayerObject::pushButton(btn);
        
        auto playLayer = PlayLayer::get();
        if (playLayer && this == playLayer->m_player1) {
            EchoTrailManager::get()->createJumpFlash(this->getPosition(), playLayer);
        }
    }
};

// ============================================================================
// 🚀 MOD INITIALIZATION  
// ============================================================================

$on_mod(Loaded) {
    log::info("✨ Echo Trails loaded! Enjoy the beautiful ghost effects!");
}
