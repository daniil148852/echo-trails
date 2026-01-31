#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/PlayerObject.hpp>
#include <Geode/modify/GameObject.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <cmath>
#include <random>
#include <unordered_map>

using namespace geode::prelude;

// ============================================================================
// МАТЕМАТИКА И КОНСТАНТЫ
// ============================================================================

namespace DriftMath {
    constexpr float PI = 3.14159265358979f;
    constexpr float TAU = PI * 2.0f;
    
    constexpr float Z_MIN = -10.0f;
    constexpr float Z_MAX = 10.0f;
    constexpr float Z_RANGE = Z_MAX - Z_MIN;
    
    // Плавная интерполяция
    inline float lerp(float a, float b, float t) {
        return a + (b - a) * std::clamp(t, 0.0f, 1.0f);
    }
    
    // Сглаженный шаг
    inline float smoothstep(float edge0, float edge1, float x) {
        float t = std::clamp((x - edge0) / (edge1 - edge0), 0.0f, 1.0f);
        return t * t * (3.0f - 2.0f * t);
    }
    
    // Easing функции
    inline float easeInOutSine(float t) {
        return -(std::cos(PI * t) - 1.0f) / 2.0f;
    }
    
    inline float easeOutBack(float t) {
        constexpr float c1 = 1.70158f;
        constexpr float c3 = c1 + 1.0f;
        return 1.0f + c3 * std::pow(t - 1.0f, 3.0f) + c1 * std::pow(t - 1.0f, 2.0f);
    }
}

// ============================================================================
// РЕЖИМЫ КАМЕРЫ
// ============================================================================

enum class CameraMode : int {
    Classic2D = 0,      // Обычный GD
    Orthographic3D = 1, // Статичная 3D перспектива
    Drift = 2,          // Синусоидальный дрифт
    Cosmic = 3,         // Медленные волны
    Hyper = 4,          // Быстрый хаотичный дрифт
    BeatSync = 5        // Полная синхронизация с BPM
};

const char* cameraModeNames[] = {
    "Classic 2D",
    "Ortho 3D", 
    "Drift",
    "Cosmic",
    "Hyper",
    "Beat Sync"
};

// ============================================================================
// КОНФИГУРАЦИЯ DIMENSION DRIFT
// ============================================================================

struct DriftConfig {
    // Глобальные настройки
    bool enabled = true;
    CameraMode cameraMode = CameraMode::Drift;
    int seed = 0;
    bool autoSeed = true;
    
    // Z-распределение
    float zSpread = 1.0f;           // Множитель разброса (0.0 - 2.0)
    float playerZLayer = 0.0f;       // Z позиция игрока
    bool dynamicPlayerZ = false;     // Игрок тоже дрифтит
    
    // Камера
    float driftAmplitude = 5.0f;     // Амплитуда качания
    float driftFrequency = 0.5f;     // Частота (Hz)
    float perspectiveStrength = 0.3f;// Сила перспективы
    float fov = 60.0f;               // Поле зрения
    
    // Визуалы  
    float depthFadeStart = 5.0f;     // Начало затухания
    float depthFadeEnd = 10.0f;      // Полное затухание
    bool depthGlow = true;           // Свечение близких объектов
    float glowIntensity = 0.5f;
    
    // Физика
    bool zAffectsGravity = true;     // Z влияет на гравитацию
    float gravityZMult = 0.1f;       // Множитель влияния
    bool zCollisionLayers = true;    // Коллизии по слоям
    float collisionZTolerance = 2.0f;// Допуск по Z для коллизий
    
    // BPM синхронизация
    float bpm = 0.0f;                // 0 = авто-детект
    float beatOffset = 0.0f;         // Смещение бита
    float beatReactivity = 1.0f;     // Реактивность на биты
};

// ============================================================================
// ГЛОБАЛЬНЫЙ МЕНЕДЖЕР
// ============================================================================

class DimensionDriftManager {
private:
    DimensionDriftManager() = default;
    
public:
    static DimensionDriftManager* get() {
        static DimensionDriftManager instance;
        return &instance;
    }
    
    // === Состояние ===
    DriftConfig config;
    bool isActive = false;
    float gameTime = 0.0f;
    float musicTime = 0.0f;
    float currentBeat = 0.0f;
    
    // === Камера ===
    float cameraZ = 0.0f;           // Текущая Z позиция камеры
    float targetCameraZ = 0.0f;     // Целевая Z
    float cameraZVelocity = 0.0f;   // Для плавности
    float cameraShake = 0.0f;       // Тряска камеры
    
    // === Z-слои объектов ===
    std::unordered_map<int, float> objectZLayers;  // objectID -> Z
    std::mt19937 rng;
    
    // === Кэш для производительности ===
    float cachedSinTime = 0.0f;
    float cachedCosTime = 0.0f;
    int lastUpdateFrame = -1;
    
    // === Методы ===
    
    void loadSettings() {
        auto* mod = Mod::get();
        
        config.enabled = mod->getSettingValue<bool>("enabled");
        config.cameraMode = static_cast<CameraMode>(
            mod->getSettingValue<int64_t>("camera-mode")
        );
        config.zSpread = static_cast<float>(mod->getSettingValue<double>("z-spread"));
        config.driftAmplitude = static_cast<float>(mod->getSettingValue<double>("drift-amplitude"));
        config.driftFrequency = static_cast<float>(mod->getSettingValue<double>("drift-frequency"));
        config.perspectiveStrength = static_cast<float>(mod->getSettingValue<double>("perspective"));
        config.depthFadeStart = static_cast<float>(mod->getSettingValue<double>("fade-start"));
        config.depthFadeEnd = static_cast<float>(mod->getSettingValue<double>("fade-end"));
        config.depthGlow = mod->getSettingValue<bool>("depth-glow");
        config.glowIntensity = static_cast<float>(mod->getSettingValue<double>("glow-intensity"));
        config.zAffectsGravity = mod->getSettingValue<bool>("z-gravity");
        config.zCollisionLayers = mod->getSettingValue<bool>("z-collisions");
        config.collisionZTolerance = static_cast<float>(mod->getSettingValue<double>("collision-tolerance"));
        config.autoSeed = mod->getSettingValue<bool>("auto-seed");
        config.beatReactivity = static_cast<float>(mod->getSettingValue<double>("beat-reactivity"));
        
        if (!config.autoSeed) {
            config.seed = static_cast<int>(mod->getSettingValue<int64_t>("manual-seed"));
        }
    }
    
    void initializeForLevel(GJGameLevel* level) {
        if (!level) return;
        
        loadSettings();
        
        // Генерируем сид на основе уровня
        if (config.autoSeed) {
            config.seed = static_cast<int>(level->m_levelID.value());
            if (config.seed == 0) {
                // Для кастомных уровней — хэш имени
                std::hash<std::string> hasher;
                config.seed = static_cast<int>(hasher(std::string(level->m_levelName)));
            }
        }
        
        rng.seed(static_cast<unsigned>(config.seed));
        objectZLayers.clear();
        
        // Сброс состояния
        gameTime = 0.0f;
        musicTime = 0.0f;
        cameraZ = 0.0f;
        targetCameraZ = 0.0f;
        cameraZVelocity = 0.0f;
        currentBeat = 0.0f;
        isActive = config.enabled;
        
        // Пытаемся получить BPM из уровня
        // (В реальности нужен анализ аудио или метаданные)
        config.bpm = 120.0f; // Дефолт
        
        log::info("DimensionDrift initialized: seed={}, mode={}", 
                  config.seed, static_cast<int>(config.cameraMode));
    }
    
    // Присваиваем Z-слой объекту
    float assignZLayer(GameObject* obj) {
        if (!obj) return 0.0f;
        
        int objID = obj->m_uniqueID;
        
        // Проверяем кэш
        auto it = objectZLayers.find(objID);
        if (it != objectZLayers.end()) {
            return it->second;
        }
        
        // Генерируем новый Z на основе типа объекта
        float z = generateZForObject(obj);
        objectZLayers[objID] = z;
        
        return z;
    }
    
    float generateZForObject(GameObject* obj) {
        // Распределение Гаусса для естественного вида
        std::normal_distribution<float> dist(0.0f, 3.0f * config.zSpread);
        float z = dist(rng);
        
        // Модификаторы по типу объекта
        GameObjectType type = obj->m_objectType;
        
        switch (type) {
            case GameObjectType::Hazard:
                // Опасности ближе к игроку для драматизма
                z = std::clamp(z * 0.5f, -3.0f, 3.0f);
                break;
                
            case GameObjectType::Solid:
                // Платформы на разных слоях
                z = std::clamp(z, -5.0f, 5.0f);
                break;
                
            case GameObjectType::Decoration:
                // Декорации — весь диапазон
                z = std::clamp(z, DriftMath::Z_MIN, DriftMath::Z_MAX);
                break;
                
            case GameObjectType::Portal:
                // Порталы всегда на слое игрока
                z = 0.0f;
                break;
                
            default:
                z = std::clamp(z, -7.0f, 7.0f);
                break;
        }
        
        // Некоторые объекты привязаны к Z=0 (игрок)
        if (obj->m_hasBeenActivated || obj->m_isDisabled) {
            z = 0.0f;
        }
        
        return z;
    }
    
    void update(float dt, float musicPosition) {
        if (!isActive) return;
        
        gameTime += dt;
        musicTime = musicPosition;
        
        // Кэшируем тригонометрию
        cachedSinTime = std::sin(gameTime * DriftMath::TAU * config.driftFrequency);
        cachedCosTime = std::cos(gameTime * DriftMath::TAU * config.driftFrequency);
        
        // Обновляем текущий бит
        if (config.bpm > 0.0f) {
            float beatsPerSecond = config.bpm / 60.0f;
            currentBeat = musicTime * beatsPerSecond + config.beatOffset;
        }
        
        // Обновляем камеру по режиму
        updateCamera(dt);
    }
    
    void updateCamera(float dt) {
        switch (config.cameraMode) {
            case CameraMode::Classic2D:
                targetCameraZ = 0.0f;
                break;
                
            case CameraMode::Orthographic3D:
                targetCameraZ = -5.0f; // Статичная позиция
                break;
                
            case CameraMode::Drift:
                targetCameraZ = cachedSinTime * config.driftAmplitude;
                break;
                
            case CameraMode::Cosmic:
                // Медленные многослойные волны
                targetCameraZ = std::sin(gameTime * 0.3f) * config.driftAmplitude * 0.7f
                              + std::sin(gameTime * 0.7f) * config.driftAmplitude * 0.3f;
                break;
                
            case CameraMode::Hyper:
                // Быстрый хаотичный дрифт
                targetCameraZ = std::sin(gameTime * 3.0f) * config.driftAmplitude * 0.5f
                              + std::cos(gameTime * 5.0f) * config.driftAmplitude * 0.3f
                              + std::sin(gameTime * 7.0f) * config.driftAmplitude * 0.2f;
                break;
                
            case CameraMode::BeatSync: {
                // Синхронизация с битами
                float beatPhase = std::fmod(currentBeat, 1.0f);
                float beatPulse = DriftMath::easeOutBack(1.0f - beatPhase);
                targetCameraZ = beatPulse * config.driftAmplitude * config.beatReactivity;
                
                // На каждый 4-й бит — резкий сдвиг
                int beatNum = static_cast<int>(currentBeat);
                if (beatNum % 4 == 0 && beatPhase < 0.1f) {
                    cameraShake = config.driftAmplitude * 0.5f;
                }
                break;
            }
        }
        
        // Добавляем тряску
        if (cameraShake > 0.01f) {
            targetCameraZ += (std::sin(gameTime * 50.0f) * cameraShake);
            cameraShake *= 0.9f;
        }
        
        // Плавное следование (spring physics)
        float springK = 8.0f;
        float damping = 0.8f;
        
        float force = (targetCameraZ - cameraZ) * springK;
        cameraZVelocity += force * dt;
        cameraZVelocity *= damping;
        cameraZ += cameraZVelocity * dt;
    }
    
    // Вычисляем визуальные параметры объекта на основе его Z
    struct ZVisuals {
        float scale;        // Масштаб (перспектива)
        float opacity;      // Прозрачность (глубина)
        float offsetY;      // Вертикальный сдвиг
        float glowAmount;   // Количество свечения
        bool visible;       // Виден ли вообще
        ccColor3B tint;     // Оттенок глубины
    };
    
    ZVisuals calculateVisuals(float objectZ) {
        ZVisuals v;
        
        // Относительный Z (от камеры)
        float relZ = objectZ - cameraZ;
        
        // Перспективный масштаб
        // Объекты дальше от камеры (большой Z) — меньше
        float perspectiveFactor = 1.0f / (1.0f + relZ * config.perspectiveStrength * 0.1f);
        v.scale = std::clamp(perspectiveFactor, 0.3f, 2.0f);
        
        // Затухание по глубине
        float absZ = std::abs(relZ);
        if (absZ < config.depthFadeStart) {
            v.opacity = 1.0f;
        } else if (absZ > config.depthFadeEnd) {
            v.opacity = 0.0f;
        } else {
            v.opacity = 1.0f - DriftMath::smoothstep(
                config.depthFadeStart, config.depthFadeEnd, absZ
            );
        }
        
        v.visible = v.opacity > 0.01f;
        
        // Y-offset для 3D эффекта
        v.offsetY = relZ * config.perspectiveStrength * 5.0f;
        
        // Свечение близких объектов
        if (config.depthGlow && relZ < 0) {
            v.glowAmount = std::clamp(-relZ / 5.0f, 0.0f, 1.0f) * config.glowIntensity;
        } else {
            v.glowAmount = 0.0f;
        }
        
        // Цветовой оттенок по глубине
        if (relZ < 0) {
            // Близко — тёплые тона
            v.tint = ccc3(255, 
                         static_cast<GLubyte>(255 - std::abs(relZ) * 10),
                         static_cast<GLubyte>(255 - std::abs(relZ) * 20));
        } else {
            // Далеко — холодные тона  
            v.tint = ccc3(static_cast<GLubyte>(255 - relZ * 10),
                         static_cast<GLubyte>(255 - relZ * 5),
                         255);
        }
        
        return v;
    }
    
    // Модификация гравитации по Z
    float getGravityModifier(float objectZ) {
        if (!config.zAffectsGravity) return 1.0f;
        
        float relZ = objectZ - cameraZ;
        
        // Косинусоидальная кривая гравитации
        // Z=0 — нормальная, Z далеко — слабее
        float modifier = std::cos(relZ * config.gravityZMult);
        return std::clamp(modifier, 0.5f, 1.5f);
    }
    
    // Проверка коллизии с учётом Z
    bool shouldCollide(float playerZ, float objectZ) {
        if (!config.zCollisionLayers) return true;
        
        float diff = std::abs(playerZ - objectZ);
        return diff <= config.collisionZTolerance;
    }
    
    void cleanup() {
        objectZLayers.clear();
        isActive = false;
        cameraZ = 0.0f;
        gameTime = 0.0f;
    }
};

// ============================================================================
// МОДИФИКАЦИЯ GAMEOBJECT — Z-слои
// ============================================================================

class $modify(DriftGameObject, GameObject) {
    struct Fields {
        float m_zLayer = 0.0f;
        float m_baseScale = 1.0f;
        float m_baseOpacity = 255.0f;
        ccColor3B m_baseColor = ccWHITE;
        float m_basePosY = 0.0f;
        bool m_zInitialized = false;
        
        // Для свечения
        CCSprite* m_glowSprite = nullptr;
    };
    
    void customSetup() {
        GameObject::customSetup();
        
        if (!m_fields->m_zInitialized) {
            m_fields->m_baseScale = this->getScale();
            m_fields->m_baseOpacity = this->getOpacity();
            m_fields->m_basePosY = this->getPositionY();
            m_fields->m_zInitialized = true;
        }
    }
    
    void activateObject() {
        auto* dm = DimensionDriftManager::get();
        
        if (dm->isActive && !m_fields->m_zInitialized) {
            initializeZLayer();
        }
        
        GameObject::activateObject();
    }
    
    void initializeZLayer() {
        auto* dm = DimensionDriftManager::get();
        
        m_fields->m_zLayer = dm->assignZLayer(this);
        m_fields->m_baseScale = this->getScale();
        m_fields->m_baseOpacity = this->getOpacity();
        m_fields->m_basePosY = this->getPositionY();
        m_fields->m_baseColor = ccWHITE;
        m_fields->m_zInitialized = true;
        
        // Создаём спрайт свечения если нужно
        if (dm->config.depthGlow && m_fields->m_zLayer < 0) {
            createGlowSprite();
        }
    }
    
    void createGlowSprite() {
        if (m_fields->m_glowSprite) return;
        
        // Создаём копию текстуры для свечения
        CCSprite* mainSprite = dynamic_cast<CCSprite*>(this);
        if (!mainSprite) return;
        
        CCSpriteFrame* frame = mainSprite->displayFrame();
        if (!frame) return;
        
        m_fields->m_glowSprite = CCSprite::createWithSpriteFrame(frame);
        if (!m_fields->m_glowSprite) return;
        
        m_fields->m_glowSprite->setBlendFunc({GL_SRC_ALPHA, GL_ONE});
        m_fields->m_glowSprite->setOpacity(0);
        m_fields->m_glowSprite->setScale(1.2f);
        m_fields->m_glowSprite->setPosition(this->getContentSize() / 2);
        
        this->addChild(m_fields->m_glowSprite, -1);
    }
    
    void updateZVisuals() {
        auto* dm = DimensionDriftManager::get();
        if (!dm->isActive) return;
        
        auto visuals = dm->calculateVisuals(m_fields->m_zLayer);
        
        // Применяем визуалы
        this->setVisible(visuals.visible);
        
        if (visuals.visible) {
            // Масштаб
            this->setScale(m_fields->m_baseScale * visuals.scale);
            
            // Прозрачность
            GLubyte newOpacity = static_cast<GLubyte>(
                m_fields->m_baseOpacity * visuals.opacity
            );
            this->setOpacity(newOpacity);
            
            // Y-смещение для глубины
            this->setPositionY(m_fields->m_basePosY + visuals.offsetY);
            
            // Cocos2d Z-order для правильной сортировки
            float renderZ = -m_fields->m_zLayer * 10.0f;
            this->setVertexZ(renderZ);
            
            // Обновляем свечение
            if (m_fields->m_glowSprite) {
                GLubyte glowOpacity = static_cast<GLubyte>(visuals.glowAmount * 150);
                m_fields->m_glowSprite->setOpacity(glowOpacity);
                m_fields->m_glowSprite->setColor(visuals.tint);
            }
        }
    }
};

// ============================================================================
// МОДИФИКАЦИЯ PLAYEROBJECT — Z-физика
// ============================================================================

class $modify(DriftPlayer, PlayerObject) {
    struct Fields {
        float m_zPosition = 0.0f;
        float m_zVelocity = 0.0f;
        float m_targetZ = 0.0f;
        bool m_inZPortal = false;
    };
    
    void update(float dt) {
        auto* dm = DimensionDriftManager::get();
        
        if (dm->isActive && dm->config.dynamicPlayerZ) {
            // Плавное движение к целевому Z
            float zDiff = m_fields->m_targetZ - m_fields->m_zPosition;
            m_fields->m_zVelocity += zDiff * 5.0f * dt;
            m_fields->m_zVelocity *= 0.9f;
            m_fields->m_zPosition += m_fields->m_zVelocity * dt;
        }
        
        PlayerObject::update(dt);
        
        // Применяем модификатор гравитации
        if (dm->isActive && dm->config.zAffectsGravity) {
            float gravMod = dm->getGravityModifier(m_fields->m_zPosition);
            // m_gravity уже обновлена в базовом update
            // Мы модифицируем вертикальную скорость
            // (Упрощённая версия — полная требует больше хуков)
        }
    }
    
    // Переопределяем проверку коллизий
    // (В реальности нужен хук на checkCollisions или hitGround)
    void collidedWithObject(float dt, GameObject* obj, CCRect rect, bool idk) {
        auto* dm = DimensionDriftManager::get();
        
        if (dm->isActive && dm->config.zCollisionLayers) {
            // Получаем Z объекта
            auto* driftObj = static_cast<DriftGameObject*>(obj);
            if (driftObj) {
                float objZ = driftObj->m_fields->m_zLayer;
                
                // Проверяем, должны ли мы коллизировать
                if (!dm->shouldCollide(m_fields->m_zPosition, objZ)) {
                    return; // Пропускаем коллизию — разные Z-слои!
                }
            }
        }
        
        PlayerObject::collidedWithObject(dt, obj, rect, idk);
    }
    
    // Порталы Z
    void portalZTransition(float targetZ, float duration) {
        m_fields->m_targetZ = targetZ;
        m_fields->m_inZPortal = true;
        
        // Запускаем анимацию перехода
        auto* dm = DimensionDriftManager::get();
        
        // Визуальный эффект портала
        this->runAction(CCSequence::create(
            CCDelayTime::create(duration),
            CCCallFunc::create(this, [this]() {
                static_cast<DriftPlayer*>(this)->m_fields->m_inZPortal = false;
            }),
            nullptr
        ));
    }
};

// ============================================================================
// МОДИФИКАЦИЯ PLAYLAYER — Главный цикл
// ============================================================================

class $modify(DriftPlayLayer, PlayLayer) {
    struct Fields {
        float m_settingsTimer = 0.0f;
        bool m_driftInitialized = false;
        
        // UI элементы
        CCLabelBMFont* m_zLabel = nullptr;
        CCLabelBMFont* m_modeLabel = nullptr;
        CCLabelBMFont* m_seedLabel = nullptr;
    };
    
    bool init(GJGameLevel* level, bool useReplay, bool dontCreateObjects) {
        if (!PlayLayer::init(level, useReplay, dontCreateObjects)) {
            return false;
        }
        
        auto* dm = DimensionDriftManager::get();
        dm->initializeForLevel(level);
        
        if (dm->isActive) {
            // Инициализируем Z для всех объектов
            initializeAllObjectsZ();
            
            // Создаём UI
            createDriftUI();
            
            m_fields->m_driftInitialized = true;
            
            log::info("DimensionDrift PlayLayer initialized");
        }
        
        return true;
    }
    
    void initializeAllObjectsZ() {
        auto* dm = DimensionDriftManager::get();
        
        // Проходим по всем объектам уровня
        if (m_objects) {
            CCObject* obj;
            CCARRAY_FOREACH(m_objects, obj) {
                auto* gameObj = dynamic_cast<DriftGameObject*>(obj);
                if (gameObj) {
                    gameObj->initializeZLayer();
                }
            }
        }
    }
    
    void createDriftUI() {
        auto* dm = DimensionDriftManager::get();
        auto winSize = CCDirector::sharedDirector()->getWinSize();
        
        // Контейнер UI
        CCNode* uiContainer = CCNode::create();
        uiContainer->setPosition(ccp(10.0f, winSize.height - 10.0f));
        this->addChild(uiContainer, 1000);
        
        // Метка режима
        m_fields->m_modeLabel = CCLabelBMFont::create(
            cameraModeNames[static_cast<int>(dm->config.cameraMode)],
            "bigFont.fnt"
        );
        m_fields->m_modeLabel->setScale(0.3f);
        m_fields->m_modeLabel->setAnchorPoint({0.0f, 1.0f});
        m_fields->m_modeLabel->setOpacity(150);
        m_fields->m_modeLabel->setPosition(ccp(0, 0));
        uiContainer->addChild(m_fields->m_modeLabel);
        
        // Метка Z камеры
        m_fields->m_zLabel = CCLabelBMFont::create("Z: 0.0", "chatFont.fnt");
        m_fields->m_zLabel->setScale(0.5f);
        m_fields->m_zLabel->setAnchorPoint({0.0f, 1.0f});
        m_fields->m_zLabel->setOpacity(120);
        m_fields->m_zLabel->setPosition(ccp(0, -15.0f));
        uiContainer->addChild(m_fields->m_zLabel);
        
        // Метка сида
        std::string seedStr = fmt::format("Seed: {}", dm->config.seed);
        m_fields->m_seedLabel = CCLabelBMFont::create(seedStr.c_str(), "chatFont.fnt");
        m_fields->m_seedLabel->setScale(0.4f);
        m_fields->m_seedLabel->setAnchorPoint({0.0f, 1.0f});
        m_fields->m_seedLabel->setOpacity(100);
        m_fields->m_seedLabel->setPosition(ccp(0, -30.0f));
        uiContainer->addChild(m_fields->m_seedLabel);
    }
    
    void update(float dt) {
        PlayLayer::update(dt);
        
        auto* dm = DimensionDriftManager::get();
        
        if (!dm->isActive) return;
        
        // Получаем текущую позицию музыки
        float musicPos = m_gameState.m_currentProgress;
        
        // Обновляем менеджер
        dm->update(dt, musicPos);
        
        // Обновляем визуалы всех видимых объектов
        updateVisibleObjectsZ();
        
        // Обновляем UI
        updateDriftUI();
        
        // Периодически перечитываем настройки
        m_fields->m_settingsTimer += dt;
        if (m_fields->m_settingsTimer >= 2.0f) {
            m_fields->m_settingsTimer = 0.0f;
            dm->loadSettings();
        }
    }
    
    void updateVisibleObjectsZ() {
        // Обновляем только видимые объекты для производительности
        if (!m_objects) return;
        
        CCRect visibleRect = this->getVisibleBounds();
        
        CCObject* obj;
        CCARRAY_FOREACH(m_objects, obj) {
            auto* gameObj = dynamic_cast<DriftGameObject*>(obj);
            if (gameObj && gameObj->isVisible()) {
                // Проверяем, в видимой области ли объект
                CCPoint pos = gameObj->getPosition();
                if (visibleRect.containsPoint(pos)) {
                    gameObj->updateZVisuals();
                }
            }
        }
    }
    
    CCRect getVisibleBounds() {
        auto winSize = CCDirector::sharedDirector()->getWinSize();
        CCPoint cameraPos = this->m_cameraPosition;
        
        // Расширяем область для предзагрузки
        float margin = 200.0f;
        return CCRectMake(
            cameraPos.x - margin,
            cameraPos.y - margin,
            winSize.width + margin * 2,
            winSize.height + margin * 2
        );
    }
    
    void updateDriftUI() {
        auto* dm = DimensionDriftManager::get();
        
        if (m_fields->m_zLabel) {
            std::string zStr = fmt::format("Z: {:.1f}", dm->cameraZ);
            m_fields->m_zLabel->setString(zStr.c_str());
        }
    }
    
    // Кастомный рендер с 3D перспективой
    void visit() {
        auto* dm = DimensionDriftManager::get();
        
        if (!dm->isActive || dm->config.cameraMode == CameraMode::Classic2D) {
            PlayLayer::visit();
            return;
        }
        
        // Сохраняем текущую матрицу
        kmGLPushMatrix();
        
        // Применяем перспективную трансформацию
        applyPerspectiveTransform();
        
        // Вызываем базовый рендер
        PlayLayer::visit();
        
        // Восстанавливаем матрицу
        kmGLPopMatrix();
    }
    
    void applyPerspectiveTransform() {
        auto* dm = DimensionDriftManager::get();
        auto winSize = CCDirector::sharedDirector()->getWinSize();
        
        // Центр экрана
        float centerX = winSize.width / 2.0f;
        float centerY = winSize.height / 2.0f;
        
        // Сдвигаем к центру
        kmGLTranslatef(centerX, centerY, 0.0f);
        
        // Применяем лёгкий наклон по Z
        float tiltX = dm->cachedSinTime * dm->config.perspectiveStrength * 0.05f;
        float tiltY = dm->cachedCosTime * dm->config.perspectiveStrength * 0.03f;
        
        kmGLRotatef(tiltY * 5.0f, 1.0f, 0.0f, 0.0f);  // Наклон по X
        kmGLRotatef(tiltX * 3.0f, 0.0f, 1.0f, 0.0f);  // Наклон по Y
        
        // Возвращаем обратно
        kmGLTranslatef(-centerX, -centerY, 0.0f);
    }
    
    void resetLevel() {
        auto* dm = DimensionDriftManager::get();
        
        // Сброс камеры
        dm->cameraZ = 0.0f;
        dm->targetCameraZ = 0.0f;
        dm->cameraZVelocity = 0.0f;
        dm->gameTime = 0.0f;
        
        PlayLayer::resetLevel();
    }
    
    void onQuit() {
        DimensionDriftManager::get()->cleanup();
        PlayLayer::onQuit();
    }
};

// ============================================================================
// КНОПКА В МЕНЮ ПАУЗЫ
// ============================================================================

#include <Geode/modify/PauseLayer.hpp>

class $modify(DriftPauseLayer, PauseLayer) {
    void customSetup() {
        PauseLayer::customSetup();
        
        auto* dm = DimensionDriftManager::get();
        
        // Добавляем кнопку переключения режима
        auto winSize = CCDirector::sharedDirector()->getWinSize();
        
        auto* menu = CCMenu::create();
        menu->setPosition(ccp(winSize.width - 60.0f, 50.0f));
        this->addChild(menu, 100);
        
        // Кнопка Dimension Drift
        auto* driftBtn = CCMenuItemSpriteExtra::create(
            CCSprite::createWithSpriteFrameName("GJ_optionsBtn_001.png"),
            this,
            menu_selector(DriftPauseLayer::onDriftSettings)
        );
        driftBtn->setScale(0.7f);
        menu->addChild(driftBtn);
        
        // Метка
        auto* label = CCLabelBMFont::create("DRIFT", "bigFont.fnt");
        label->setScale(0.25f);
        label->setPosition(driftBtn->getContentSize() / 2 + CCSize(0, -25.0f));
        driftBtn->addChild(label);
    }
    
    void onDriftSettings(CCObject* sender) {
        // Открываем настройки мода
        geode::openSettingsPopup(Mod::get());
    }
};

// ============================================================================
// КОПИРОВАНИЕ СИДА
// ============================================================================

$execute {
    // Добавляем кнопку копирования сида в уведомление
    Loader::get()->queueInMainThread([]() {
        log::info("DimensionDrift mod loaded!");
    });
}
