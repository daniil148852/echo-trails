#include <Geode/Geode.hpp>
#include <Geode/modify/PlayerObject.hpp>

using namespace geode::prelude;

// Режимы следа
enum class TrailMode {
    Classic = 0,
    Rainbow = 1,
    Neon = 2,
    Glitch = 3,
    Fire = 4,
    Ice = 5,
    Galaxy = 6,
    Chromatic = 7,
    Pulse = 8,
    Ghost = 9
};

// Утилиты для цветов
namespace ColorUtils {
    
    // HSV to RGB конвертация для радужного эффекта
    ccColor3B hsvToRgb(float h, float s, float v) {
        float c = v * s;
        float x = c * (1.0f - std::abs(std::fmod(h / 60.0f, 2.0f) - 1.0f));
        float m = v - c;
        
        float r, g, b;
        if (h < 60) { r = c; g = x; b = 0; }
        else if (h < 120) { r = x; g = c; b = 0; }
        else if (h < 180) { r = 0; g = c; b = x; }
        else if (h < 240) { r = 0; g = x; b = c; }
        else if (h < 300) { r = x; g = 0; b = c; }
        else { r = c; g = 0; b = x; }
        
        return ccc3(
            static_cast<GLubyte>((r + m) * 255),
            static_cast<GLubyte>((g + m) * 255),
            static_cast<GLubyte>((b + m) * 255)
        );
    }
    
    // Интерполяция цветов
    ccColor3B lerp(const ccColor3B& a, const ccColor3B& b, float t) {
        return ccc3(
            static_cast<GLubyte>(a.r + (b.r - a.r) * t),
            static_cast<GLubyte>(a.g + (b.g - a.g) * t),
            static_cast<GLubyte>(a.b + (b.b - a.b) * t)
        );
    }
    
    // Предустановленные цвета
    inline ccColor3B fireColors[] = {
        ccc3(255, 100, 0),   // Оранжевый
        ccc3(255, 200, 0),   // Жёлтый
        ccc3(255, 50, 0),    // Красный
        ccc3(255, 150, 50)   // Светло-оранжевый
    };
    
    inline ccColor3B iceColors[] = {
        ccc3(100, 200, 255), // Светло-голубой
        ccc3(150, 220, 255), // Ледяной
        ccc3(200, 240, 255), // Белый лёд
        ccc3(50, 150, 255)   // Синий
    };
    
    inline ccColor3B galaxyColors[] = {
        ccc3(138, 43, 226),  // Фиолетовый
        ccc3(75, 0, 130),    // Индиго
        ccc3(255, 20, 147),  // Розовый
        ccc3(0, 191, 255),   // Голубой
        ccc3(148, 0, 211)    // Тёмно-фиолетовый
    };
}

class $modify(GhostTrailPlayer, PlayerObject) {
    struct Fields {
        // Таймеры
        float m_ghostTimer = 0.0f;
        float m_settingsRefreshTimer = 0.0f;
        float m_globalTime = 0.0f;
        float m_rainbowHue = 0.0f;
        
        // Кэш настроек
        float m_cachedInterval = 0.05f;
        int m_cachedOpacity = 180;
        float m_cachedFadeDuration = 0.4f;
        float m_cachedScaleEnd = 0.7f;
        float m_cachedRotationSpeed = 0.0f;
        TrailMode m_cachedMode = TrailMode::Classic;
        bool m_cachedOnlyMoving = false;
        bool m_cachedAdditiveBlend = false;
        bool m_cachedDualColor = false;
        int m_cachedTrailLength = 10;
        float m_cachedRainbowSpeed = 2.0f;
        float m_cachedGlitchIntensity = 1.0f;
        bool m_cachedMirrorMode = false;
        bool m_cachedEnabled = true;
        
        // Отслеживание движения
        CCPoint m_lastPosition = CCPointZero;
        int m_ghostCounter = 0;
    };

    void update(float dt) {
        PlayerObject::update(dt);
        
        auto* playLayer = PlayLayer::get();
        if (!playLayer) return;
        if (m_isDead || m_isHidden) return;
        
        // Глобальное время для анимаций
        m_fields->m_globalTime += dt;
        
        // Обновляем радужный оттенок
        m_fields->m_rainbowHue += dt * m_fields->m_cachedRainbowSpeed * 100.0f;
        if (m_fields->m_rainbowHue >= 360.0f) {
            m_fields->m_rainbowHue -= 360.0f;
        }
        
        // Обновляем кэш настроек
        m_fields->m_settingsRefreshTimer += dt;
        if (m_fields->m_settingsRefreshTimer >= 0.5f) {
            m_fields->m_settingsRefreshTimer = 0.0f;
            refreshSettings();
        }
        
        if (!m_fields->m_cachedEnabled) return;
        if (m_fields->m_cachedInterval <= 0.0f) return;
        
        // Проверяем движение если нужно
        if (m_fields->m_cachedOnlyMoving) {
            CCPoint currentPos = this->getPosition();
            float distance = ccpDistance(currentPos, m_fields->m_lastPosition);
            m_fields->m_lastPosition = currentPos;
            
            if (distance < 1.0f) return;
        }
        
        m_fields->m_ghostTimer += dt;
        
        if (m_fields->m_ghostTimer >= m_fields->m_cachedInterval) {
            m_fields->m_ghostTimer = 0.0f;
            spawnGhostSprite();
            
            // Зеркальный режим - спавним второго призрака
            if (m_fields->m_cachedMirrorMode) {
                spawnMirrorGhostSprite();
            }
        }
    }
    
    void refreshSettings() {
        auto* mod = Mod::get();
        
        m_fields->m_cachedEnabled = mod->getSettingValue<bool>("enabled");
        
        int64_t spawnRate = mod->getSettingValue<int64_t>("spawn-rate");
        m_fields->m_cachedInterval = spawnRate > 0 ? 1.0f / static_cast<float>(spawnRate) : 0.0f;
        
        m_fields->m_cachedOpacity = static_cast<int>(mod->getSettingValue<int64_t>("opacity"));
        m_fields->m_cachedFadeDuration = static_cast<float>(mod->getSettingValue<double>("fade-duration"));
        m_fields->m_cachedScaleEnd = static_cast<float>(mod->getSettingValue<double>("scale-end"));
        m_fields->m_cachedRotationSpeed = static_cast<float>(mod->getSettingValue<double>("rotation-speed"));
        m_fields->m_cachedMode = static_cast<TrailMode>(mod->getSettingValue<int64_t>("trail-mode"));
        m_fields->m_cachedOnlyMoving = mod->getSettingValue<bool>("only-moving");
        m_fields->m_cachedAdditiveBlend = mod->getSettingValue<bool>("additive-blend");
        m_fields->m_cachedDualColor = mod->getSettingValue<bool>("dual-color");
        m_fields->m_cachedTrailLength = static_cast<int>(mod->getSettingValue<int64_t>("trail-length"));
        m_fields->m_cachedRainbowSpeed = static_cast<float>(mod->getSettingValue<double>("rainbow-speed"));
        m_fields->m_cachedGlitchIntensity = static_cast<float>(mod->getSettingValue<double>("glitch-intensity"));
        m_fields->m_cachedMirrorMode = mod->getSettingValue<bool>("mirror-mode");
    }
    
    CCSprite* getActivePlayerSprite() {
        if (m_iconSprite && m_iconSprite->isVisible() && m_iconSprite->getOpacity() > 0) {
            return m_iconSprite;
        }
        return nullptr;
    }
    
    ccColor3B getColorForMode(int index) {
        TrailMode mode = m_fields->m_cachedMode;
        
        switch (mode) {
            case TrailMode::Rainbow: {
                float hue = std::fmod(m_fields->m_rainbowHue + index * 15.0f, 360.0f);
                return ColorUtils::hsvToRgb(hue, 1.0f, 1.0f);
            }
            
            case TrailMode::Neon: {
                float hue = std::fmod(m_fields->m_rainbowHue, 360.0f);
                return ColorUtils::hsvToRgb(hue, 0.8f, 1.0f);
            }
            
            case TrailMode::Fire: {
                int colorIndex = (index + m_fields->m_ghostCounter) % 4;
                return ColorUtils::fireColors[colorIndex];
            }
            
            case TrailMode::Ice: {
                int colorIndex = (index + m_fields->m_ghostCounter) % 4;
                return ColorUtils::iceColors[colorIndex];
            }
            
            case TrailMode::Galaxy: {
                int colorIndex = (index + m_fields->m_ghostCounter) % 5;
                return ColorUtils::galaxyColors[colorIndex];
            }
            
            case TrailMode::Chromatic: {
                // RGB сдвиг
                if (index % 3 == 0) return ccc3(255, 50, 50);
                if (index % 3 == 1) return ccc3(50, 255, 50);
                return ccc3(50, 50, 255);
            }
            
            case TrailMode::Pulse: {
                float pulse = (std::sin(m_fields->m_globalTime * 8.0f) + 1.0f) / 2.0f;
                GLubyte intensity = static_cast<GLubyte>(155 + pulse * 100);
                return ccc3(intensity, intensity, 255);
            }
            
            case TrailMode::Ghost: {
                float flicker = (std::sin(m_fields->m_globalTime * 15.0f + index) + 1.0f) / 2.0f;
                GLubyte val = static_cast<GLubyte>(150 + flicker * 105);
                return ccc3(val, val, val);
            }
            
            case TrailMode::Glitch: {
                // Случайные цвета для глитч-эффекта
                float r = std::sin(m_fields->m_globalTime * 23.456f + index * 1.234f);
                float g = std::sin(m_fields->m_globalTime * 34.567f + index * 2.345f);
                float b = std::sin(m_fields->m_globalTime * 45.678f + index * 3.456f);
                return ccc3(
                    static_cast<GLubyte>((r + 1.0f) * 127.5f),
                    static_cast<GLubyte>((g + 1.0f) * 127.5f),
                    static_cast<GLubyte>((b + 1.0f) * 127.5f)
                );
            }
            
            case TrailMode::Classic:
            default: {
                return ccWHITE;
            }
        }
    }
    
    void applyGlitchEffect(CCSprite* ghost, CCPoint basePos) {
        if (m_fields->m_cachedMode != TrailMode::Glitch) return;
        
        float intensity = m_fields->m_cachedGlitchIntensity;
        float offsetX = (std::sin(m_fields->m_globalTime * 50.0f) * 5.0f) * intensity;
        float offsetY = (std::cos(m_fields->m_globalTime * 37.0f) * 3.0f) * intensity;
        
        ghost->setPosition(ccp(basePos.x + offsetX, basePos.y + offsetY));
    }

    void spawnGhostSprite() {
        CCSprite* sourceSprite = getActivePlayerSprite();
        if (!sourceSprite) return;
        
        CCSpriteFrame* frame = sourceSprite->displayFrame();
        if (!frame) return;
        
        CCNode* parent = this->getParent();
        if (!parent) return;
        
        m_fields->m_ghostCounter++;
        
        CCSprite* ghost = CCSprite::createWithSpriteFrame(frame);
        if (!ghost) return;
        
        // === Позиция и трансформации ===
        CCPoint pos = this->getPosition();
        ghost->setPosition(pos);
        
        float totalRotation = this->getRotation() + sourceSprite->getRotation();
        ghost->setRotation(totalRotation);
        
        float totalScale = this->getScale() * sourceSprite->getScale();
        ghost->setScale(totalScale);
        
        bool flipX = sourceSprite->isFlipX();
        if (m_isUpsideDown) flipX = !flipX;
        ghost->setFlipX(flipX);
        ghost->setFlipY(sourceSprite->isFlipY());
        
        ghost->setAnchorPoint({0.5f, 0.5f});
        
        // === Цвет и прозрачность ===
        GLubyte opacity = static_cast<GLubyte>(std::clamp(m_fields->m_cachedOpacity, 0, 255));
        ghost->setOpacity(opacity);
        
        // Применяем цвет режима или цвет игрока
        if (m_fields->m_cachedMode == TrailMode::Classic && m_fields->m_cachedDualColor) {
            ghost->setColor(sourceSprite->getColor());
        } else {
            ghost->setColor(getColorForMode(m_fields->m_ghostCounter));
        }
        
        // === Blend mode ===
        if (m_fields->m_cachedAdditiveBlend) {
            ghost->setBlendFunc({GL_SRC_ALPHA, GL_ONE});
        }
        
        // Глитч-эффект сдвигает позицию
        applyGlitchEffect(ghost, pos);
        
        // Добавляем на сцену
        parent->addChild(ghost, this->getZOrder() - 1);
        
        // === Создание анимации ===
        createGhostAnimation(ghost, totalScale, totalRotation);
        
        // Для хроматического режима создаём дополнительные спрайты
        if (m_fields->m_cachedMode == TrailMode::Chromatic) {
            spawnChromaticGhosts(frame, pos, totalScale, totalRotation, flipX, parent);
        }
    }
    
    void spawnChromaticGhosts(CCSpriteFrame* frame, CCPoint pos, float scale, 
                              float rotation, bool flipX, CCNode* parent) {
        // Красный канал - сдвиг влево
        CCSprite* redGhost = CCSprite::createWithSpriteFrame(frame);
        if (redGhost) {
            redGhost->setPosition(ccp(pos.x - 3.0f, pos.y));
            redGhost->setRotation(rotation);
            redGhost->setScale(scale);
            redGhost->setFlipX(flipX);
            redGhost->setAnchorPoint({0.5f, 0.5f});
            redGhost->setColor(ccc3(255, 0, 0));
            redGhost->setOpacity(100);
            redGhost->setBlendFunc({GL_SRC_ALPHA, GL_ONE});
            parent->addChild(redGhost, this->getZOrder() - 2);
            createGhostAnimation(redGhost, scale, rotation);
        }
        
        // Синий канал - сдвиг вправо
        CCSprite* blueGhost = CCSprite::createWithSpriteFrame(frame);
        if (blueGhost) {
            blueGhost->setPosition(ccp(pos.x + 3.0f, pos.y));
            blueGhost->setRotation(rotation);
            blueGhost->setScale(scale);
            blueGhost->setFlipX(flipX);
            blueGhost->setAnchorPoint({0.5f, 0.5f});
            blueGhost->setColor(ccc3(0, 0, 255));
            blueGhost->setOpacity(100);
            blueGhost->setBlendFunc({GL_SRC_ALPHA, GL_ONE});
            parent->addChild(blueGhost, this->getZOrder() - 2);
            createGhostAnimation(blueGhost, scale, rotation);
        }
    }
    
    void spawnMirrorGhostSprite() {
        CCSprite* sourceSprite = getActivePlayerSprite();
        if (!sourceSprite) return;
        
        CCSpriteFrame* frame = sourceSprite->displayFrame();
        if (!frame) return;
        
        CCNode* parent = this->getParent();
        if (!parent) return;
        
        CCSprite* ghost = CCSprite::createWithSpriteFrame(frame);
        if (!ghost) return;
        
        CCPoint pos = this->getPosition();
        
        // Зеркальная позиция (относительно центра экрана)
        auto* director = CCDirector::sharedDirector();
        float screenCenterY = director->getWinSize().height / 2.0f;
        float mirrorY = screenCenterY * 2.0f - pos.y;
        ghost->setPosition(ccp(pos.x, mirrorY));
        
        float totalRotation = this->getRotation() + sourceSprite->getRotation();
        ghost->setRotation(-totalRotation); // Инвертируем ротацию
        
        float totalScale = this->getScale() * sourceSprite->getScale();
        ghost->setScale(totalScale);
        
        bool flipX = sourceSprite->isFlipX();
        if (m_isUpsideDown) flipX = !flipX;
        ghost->setFlipX(flipX);
        ghost->setFlipY(!sourceSprite->isFlipY()); // Инвертируем flip
        
        ghost->setAnchorPoint({0.5f, 0.5f});
        
        GLubyte opacity = static_cast<GLubyte>(std::clamp(m_fields->m_cachedOpacity / 2, 0, 255));
        ghost->setOpacity(opacity);
        
        ghost->setColor(getColorForMode(m_fields->m_ghostCounter + 100));
        
        if (m_fields->m_cachedAdditiveBlend) {
            ghost->setBlendFunc({GL_SRC_ALPHA, GL_ONE});
        }
        
        parent->addChild(ghost, this->getZOrder() - 2);
        createGhostAnimation(ghost, totalScale, totalRotation);
    }
    
    void createGhostAnimation(CCSprite* ghost, float startScale, float startRotation) {
        float duration = m_fields->m_cachedFadeDuration;
        float endScale = startScale * m_fields->m_cachedScaleEnd;
        float rotationAmount = m_fields->m_cachedRotationSpeed * duration;
        
        CCArray* actions = CCArray::create();
        
        // Затухание
        actions->addObject(CCFadeOut::create(duration));
        
        // Масштабирование
        if (std::abs(endScale - startScale) > 0.01f) {
            actions->addObject(CCScaleTo::create(duration, endScale));
        }
        
        // Вращение
        if (std::abs(rotationAmount) > 0.1f) {
            actions->addObject(CCRotateBy::create(duration, rotationAmount));
        }
        
        // Особые эффекты для режимов
        switch (m_fields->m_cachedMode) {
            case TrailMode::Pulse: {
                // Пульсирующий масштаб
                CCScaleTo* scaleUp = CCScaleTo::create(duration * 0.3f, startScale * 1.2f);
                CCScaleTo* scaleDown = CCScaleTo::create(duration * 0.7f, endScale * 0.5f);
                actions->addObject(CCSequence::create(scaleUp, scaleDown, nullptr));
                break;
            }
            
            case TrailMode::Ghost: {
                // Подъём вверх
                CCMoveBy* moveUp = CCMoveBy::create(duration, ccp(0, 30.0f));
                CCEaseOut* easedMove = CCEaseOut::create(moveUp, 2.0f);
                actions->addObject(easedMove);
                break;
            }
            
            case TrailMode::Fire: {
                // Движение вверх как пламя
                CCMoveBy* moveUp = CCMoveBy::create(duration, ccp(0, 20.0f));
                float randomX = (std::sin(m_fields->m_globalTime * 10.0f) * 10.0f);
                CCMoveBy* moveWiggle = CCMoveBy::create(duration, ccp(randomX, 0));
                actions->addObject(moveUp);
                actions->addObject(moveWiggle);
                break;
            }
            
            case TrailMode::Ice: {
                // Падение вниз как снежинки
                CCMoveBy* moveDown = CCMoveBy::create(duration, ccp(0, -15.0f));
                actions->addObject(moveDown);
                break;
            }
            
            default:
                break;
        }
        
        // Объединяем все действия
        CCSpawn* spawn = CCSpawn::create(actions);
        CCRemoveSelf* removeSelf = CCRemoveSelf::create();
        CCSequence* sequence = CCSequence::create(spawn, removeSelf, nullptr);
        
        ghost->runAction(sequence);
    }
};
