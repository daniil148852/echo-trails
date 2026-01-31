#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/GameObject.hpp>
#include <Geode/modify/PauseLayer.hpp>

// Исправляем ошибку с заголовками
#include <fmt/format.h>
#include <kazmath/kazmath.h>
#include <kazmath/GL/matrix.h>
#include <random>

using namespace geode::prelude;

// ============================================================================
// КОНФИГУРАЦИЯ И МАТЕМАТИКА
// ============================================================================
enum class CameraMode { Classic2D, Ortho3D, Drift, BeatSync };

struct DriftConfig {
    bool enabled = true;
    CameraMode cameraMode = CameraMode::Drift;
    float zSpread = 1.0f;
    float driftAmplitude = 5.0f;
    float driftFrequency = 0.5f;
};

// ============================================================================
// МЕНЕДЖЕР (СИНГЛТОН)
// ============================================================================
class DimensionDriftManager {
public:
    static DimensionDriftManager* get() {
        static DimensionDriftManager instance;
        return &instance;
    }

    DriftConfig config;
    float cameraZ = 0.0f;
    float gameTime = 0.0f;
    std::mt19937 rng;

    void update(float dt) {
        if (!config.enabled) return;
        gameTime += dt;
        
        if (config.cameraMode == CameraMode::Drift) {
            cameraZ = std::sin(gameTime * 3.14159f * config.driftFrequency) * config.driftAmplitude;
        }
    }

    float getZForObject(GameObject* obj) {
        // Детерминированный Z на основе ID объекта
        std::seed_seq seed{static_cast<int>(obj->m_uniqueID)};
        std::mt19937 objRng(seed);
        std::uniform_real_distribution<float> dist(-10.0f * config.zSpread, 10.0f * config.zSpread);
        
        // Порталы и важные объекты оставляем на Z=0
        if (obj->m_objectType == GameObjectType::Portal) return 0.0f;
        return dist(objRng);
    }
};

// ============================================================================
// ХУК ОБЪЕКТА (ОПТИМИЗИРОВАННЫЙ)
// ============================================================================
class $modify(DriftGameObject, GameObject) {
    struct Fields {
        float m_zLayer = 0.0f;
        bool m_initialized = false;
        float m_origScale = 1.0f;
    };

    void customSetup() {
        GameObject::customSetup();
        if (!m_fields->m_initialized) {
            m_fields->m_zLayer = DimensionDriftManager::get()->getZForObject(this);
            m_fields->m_origScale = this->getScale();
            m_fields->m_initialized = true;
        }
    }

    // Вместо тяжелого цикла в PlayLayer, обновляем объект при отрисовке
    void updateZVisuals() {
        auto* dm = DimensionDriftManager::get();
        float relZ = m_fields->m_zLayer - dm->cameraZ;
        
        // Масштаб по перспективе
        float pScale = 1.0f / (1.0f + relZ * 0.05f);
        this->setScale(m_fields->m_origScale * std::clamp(pScale, 0.5f, 1.5f));
        
        // Затухание (Opacity)
        float opacity = 255.0f / (1.0f + std::abs(relZ) * 0.1f);
        this->setOpacity(static_cast<GLubyte>(opacity));
    }
};

// ============================================================================
// ХУК СЛОЯ ИГРЫ (ОПТИМИЗАЦИЯ И РЕНДЕР)
// ============================================================================
class $modify(DriftPlayLayer, PlayLayer) {
    void update(float dt) {
        PlayLayer::update(dt);
        DimensionDriftManager::get()->update(dt);
        
        // Оптимизация: обновляем только видимые объекты через дочерние узлы
        auto children = this->m_batchNodeAdd->getChildren();
        if (children) {
            for (int i = 0; i < children->count(); ++i) {
                if (auto obj = dynamic_cast<DriftGameObject*>(children->objectAtIndex(i))) {
                    obj->updateZVisuals();
                }
            }
        }
    }

    // Исправленный метод visit для изоляции 3D-эффекта от UI
    void visit() {
        auto dm = DimensionDriftManager::get();
        if (!dm->config.enabled || dm->config.cameraMode == CameraMode::Classic2D) {
            PlayLayer::visit();
            return;
        }

        kmGLPushMatrix(); // Сохраняем матрицу

        auto winSize = CCDirector::sharedDirector()->getWinSize();
        kmGLTranslatef(winSize.width / 2, winSize.height / 2, 0);
        
        // Небольшой наклон камеры
        float tilt = std::cos(dm->gameTime) * 2.0f;
        kmGLRotatef(tilt, 0, 1, 0); 
        
        kmGLTranslatef(-winSize.width / 2, -winSize.height / 2, 0);

        PlayLayer::visit(); // Рисуем уровень

        kmGLPopMatrix(); // Сбрасываем матрицу, чтобы UI не уплыл
    }
};

// ============================================================================
// КНОПКА В МЕНЮ ПАУЗЫ
// ============================================================================
class $modify(DriftPauseLayer, PauseLayer) {
    void customSetup() {
        PauseLayer::customSetup();
        
        auto winSize = CCDirector::sharedDirector()->getWinSize();
        auto menu = this->getChildByID("right-button-menu");
        
        auto sprite = CCSprite::createWithSpriteFrameName("GJ_plusBtn_001.png");
        auto btn = CCMenuItemSpriteExtra::create(
            sprite, this, menu_selector(DriftPauseLayer::onDriftToggle)
        );
        
        if (menu) menu->addChild(btn);
    }

    void onDriftToggle(CCObject*) {
        auto dm = DimensionDriftManager::get();
        dm->config.enabled = !dm->config.enabled;
        FLAlertLayer::create("Drift", dm->config.enabled ? "Enabled" : "Disabled", "OK")->show();
    }
};
