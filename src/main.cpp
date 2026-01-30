#include <Geode/Geode.hpp>
#include <Geode/modify/PlayerObject.hpp>

using namespace geode::prelude;

class $modify(GhostTrailPlayer, PlayerObject) {
    struct Fields {
        float m_ghostTimer = 0.0f;
        float m_cachedInterval = 0.0f;
        int m_cachedOpacity = 150;
        bool m_cachedUseColor = true;
        float m_settingsRefreshTimer = 0.0f;
    };

    void update(float dt) {
        PlayerObject::update(dt);
        
        // Проверяем что мы в игровом процессе
        auto* playLayer = PlayLayer::get();
        if (!playLayer) return;
        if (m_isDead || m_isHidden) return;
        
        // Обновляем кэш настроек каждые 0.5 сек для оптимизации на ARM64
        m_fields->m_settingsRefreshTimer += dt;
        if (m_fields->m_settingsRefreshTimer >= 0.5f) {
            m_fields->m_settingsRefreshTimer = 0.0f;
            refreshSettings();
        }
        
        // Пропускаем если частота = 0
        if (m_fields->m_cachedInterval <= 0.0f) return;
        
        m_fields->m_ghostTimer += dt;
        
        if (m_fields->m_ghostTimer >= m_fields->m_cachedInterval) {
            m_fields->m_ghostTimer = 0.0f;
            spawnGhostSprite();
        }
    }
    
    void refreshSettings() {
        auto* mod = Mod::get();
        
        int64_t spawnRate = mod->getSettingValue<int64_t>("spawn-rate");
        if (spawnRate > 0) {
            m_fields->m_cachedInterval = 1.0f / static_cast<float>(spawnRate);
        } else {
            m_fields->m_cachedInterval = 0.0f;
        }
        
        m_fields->m_cachedOpacity = static_cast<int>(
            mod->getSettingValue<int64_t>("opacity")
        );
        m_fields->m_cachedUseColor = mod->getSettingValue<bool>("use-player-color");
    }
    
    CCSprite* getActivePlayerSprite() {
        // Определяем активный спрайт игрока
        // m_iconSprite - основной спрайт для куба/шара/робота/паука
        if (m_iconSprite && m_iconSprite->isVisible() && 
            m_iconSprite->getOpacity() > 0) {
            return m_iconSprite;
        }
        return nullptr;
    }

    void spawnGhostSprite() {
        CCSprite* sourceSprite = getActivePlayerSprite();
        if (!sourceSprite) return;
        
        // Получаем текущий кадр спрайта
        CCSpriteFrame* frame = sourceSprite->displayFrame();
        if (!frame) return;
        
        // Создаём призрачную копию
        CCSprite* ghost = CCSprite::createWithSpriteFrame(frame);
        if (!ghost) return;
        
        // Получаем родителя для добавления призрака
        CCNode* parent = this->getParent();
        if (!parent) return;
        
        // === Копируем трансформации ===
        ghost->setPosition(this->getPosition());
        
        // Суммируем ротацию игрока и спрайта
        float totalRotation = this->getRotation() + sourceSprite->getRotation();
        ghost->setRotation(totalRotation);
        
        // Масштаб
        float totalScale = this->getScale() * sourceSprite->getScale();
        ghost->setScale(totalScale);
        
        // Отражение с учётом состояния "вверх ногами"
        bool flipX = sourceSprite->isFlipX();
        if (m_isUpsideDown) flipX = !flipX;
        ghost->setFlipX(flipX);
        ghost->setFlipY(sourceSprite->isFlipY());
        
        // Начальная прозрачность
        GLubyte opacity = static_cast<GLubyte>(
            std::clamp(m_fields->m_cachedOpacity, 0, 255)
        );
        ghost->setOpacity(opacity);
        
        // Центрируем якорь
        ghost->setAnchorPoint({0.5f, 0.5f});
        
        // === Применяем цвет ===
        if (m_fields->m_cachedUseColor) {
            ghost->setColor(sourceSprite->getColor());
        } else {
            ghost->setColor(ccWHITE);
        }
        
        // Добавляем на слой ниже игрока
        parent->addChild(ghost, this->getZOrder() - 1);
        
        // === Анимация затухания ===
        // Используем CCRemoveSelf для безопасного удаления памяти
        // Это критично для Android64 - предотвращает краши
        constexpr float FADE_DURATION = 0.35f;
        
        CCFiniteTimeAction* fadeOut = CCFadeOut::create(FADE_DURATION);
        CCFiniteTimeAction* scaleDown = CCScaleTo::create(FADE_DURATION, totalScale * 0.8f);
        CCFiniteTimeAction* spawn = CCSpawn::create(fadeOut, scaleDown, nullptr);
        CCFiniteTimeAction* removeSelf = CCRemoveSelf::create();
        
        CCSequence* sequence = CCSequence::create(spawn, removeSelf, nullptr);
        ghost->runAction(sequence);
    }
};
