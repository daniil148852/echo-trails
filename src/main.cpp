/**
 * ========================================
 *          ECHO TRAILS MOD
 *     Призрачные следы для Geometry Dash
 * ========================================
 * 
 * Мод создаёт полупрозрачные копии иконки игрока,
 * которые следуют за ним с небольшой задержкой,
 * создавая эффект "эха" или "следа".
 */

#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <deque>
#include <vector>

using namespace geode::prelude;

// ============================================
// СТРУКТУРА ДЛЯ ХРАНЕНИЯ СОСТОЯНИЯ ИГРОКА
// ============================================

/**
 * Хранит полную информацию о позиции и трансформации
 * игрока в определённый момент времени
 */
struct PlayerState {
    cocos2d::CCPoint position;   // Позиция на уровне
    float rotation;               // Угол поворота (градусы)
    float scaleX;                 // Масштаб по X
    float scaleY;                 // Масштаб по Y
    bool flipX;                   // Отражение по X
    bool flipY;                   // Отражение по Y
    bool visible;                 // Видимость спрайта
    cocos2d::ccColor3B color;     // Цвет игрока
};

// ============================================
// МОДИФИКАЦИЯ PLAYLAYER
// ============================================

class $modify(EchoPlayLayer, PlayLayer) {
    
    /**
     * Структура Fields хранит все данные мода
     * для каждого экземпляра PlayLayer
     */
    struct Fields {
        // История позиций игрока (очередь)
        std::deque<PlayerState> positionHistory;
        
        // Спрайты призраков
        std::vector<cocos2d::CCSprite*> ghosts;
        
        // Настройки из mod.json
        int ghostCount = 5;
        int frameDelay = 4;
        int baseOpacity = 180;
        bool useRainbow = false;
        bool enabled = true;
        
        // Флаг инициализации
        bool initialized = false;
        
        // Счётчик кадров для радужного режима
        float rainbowHue = 0.0f;
    };

    // ----------------------------------------
    // ИНИЦИАЛИЗАЦИЯ УРОВНЯ
    // ----------------------------------------
    
    bool init(GJGameLevel* level, bool useReplay, bool dontCreateObjects) {
        // Вызываем оригинальный метод
        if (!PlayLayer::init(level, useReplay, dontCreateObjects)) {
            return false;
        }
        
        // Загружаем настройки из mod.json
        loadSettings();
        
        // Логируем для отладки
        log::info("Echo Trails: Инициализация уровня '{}'", 
                  level->m_levelName.c_str());
        
        return true;
    }

    // ----------------------------------------
    // ЗАГРУЗКА НАСТРОЕК
    // ----------------------------------------
    
    void loadSettings() {
        auto mod = Mod::get();
        
        m_fields->enabled = mod->getSettingValue<bool>("enabled");
        m_fields->ghostCount = static_cast<int>(mod->getSettingValue<int64_t>("ghost-count"));
        m_fields->frameDelay = static_cast<int>(mod->getSettingValue<int64_t>("frame-delay"));
        m_fields->baseOpacity = static_cast<int>(mod->getSettingValue<int64_t>("base-opacity"));
        m_fields->useRainbow = mod->getSettingValue<bool>("use-rainbow");
        
        log::debug("Echo Trails: Настройки загружены - {} призраков, задержка {}", 
                   m_fields->ghostCount, m_fields->frameDelay);
    }

    // ----------------------------------------
    // СОЗДАНИЕ ПРИЗРАКОВ
    // ----------------------------------------
    
    void createGhosts() {
        // Проверяем наличие игрока
        if (!m_player1) {
            log::warn("Echo Trails: Игрок не найден!");
            return;
        }
        
        // Очищаем предыдущих призраков (если были)
        cleanupGhosts();
        
        for (int i = 0; i < m_fields->ghostCount; i++) {
            // Создаём спрайт призрака
            // Используем текстуру круга как основу
            auto ghost = cocos2d::CCSprite::create("square.png");
            
            if (!ghost) {
                // Альтернативная текстура
                ghost = cocos2d::CCSprite::create("GJ_square01.png");
            }
            
            if (ghost) {
                // === НАСТРОЙКА ПРОЗРАЧНОСТИ ===
                // Каждый следующий призрак более прозрачный
                float opacityFactor = 1.0f - (static_cast<float>(i) / m_fields->ghostCount);
                GLubyte opacity = static_cast<GLubyte>(m_fields->baseOpacity * opacityFactor);
                ghost->setOpacity(opacity);
                
                // === НАЧАЛЬНОЕ СОСТОЯНИЕ ===
                ghost->setVisible(false);
                ghost->setZOrder(-10 - i);  // Позади игрока
                
                // === ЦВЕТ ПРИЗРАКА ===
                // Копируем цвет с игрока
                ghost->setColor(m_player1->m_playerColor1);
                
                // === МАСШТАБ ===
                // Делаем призраков немного меньше оригинала
                float scaleFactor = 0.95f - (i * 0.02f);
                ghost->setScale(m_player1->getScale() * scaleFactor);
                
                // Добавляем на слой объектов
                if (m_objectLayer) {
                    m_objectLayer->addChild(ghost);
                } else {
                    this->addChild(ghost);
                }
                
                m_fields->ghosts.push_back(ghost);
            }
        }
        
        m_fields->initialized = true;
        log::info("Echo Trails: Создано {} призраков", m_fields->ghosts.size());
    }

    // ----------------------------------------
    // ГЛАВНЫЙ ЦИКЛ ОБНОВЛЕНИЯ
    // ----------------------------------------
    
    void update(float dt) {
        // Вызываем оригинальный update
        PlayLayer::update(dt);
        
        // Проверяем, включён ли эффект
        if (!m_fields->enabled) return;
        
        // Ленивая инициализация призраков
        // (создаём только когда игрок точно существует)
        if (!m_fields->initialized && m_player1) {
            createGhosts();
        }
        
        // Проверяем готовность
        if (!m_fields->initialized || !m_player1) return;
        
        // === СОХРАНЯЕМ ТЕКУЩЕЕ СОСТОЯНИЕ ИГРОКА ===
        PlayerState currentState;
        currentState.position = m_player1->getPosition();
        currentState.rotation = m_player1->getRotation();
        currentState.scaleX = m_player1->getScaleX();
        currentState.scaleY = m_player1->getScaleY();
        currentState.flipX = m_player1->isFlipX();
        currentState.flipY = m_player1->isFlipY();
        currentState.visible = m_player1->isVisible() && !m_player1->m_isDead;
        currentState.color = m_player1->m_playerColor1;
        
        // Добавляем в начало очереди
        m_fields->positionHistory.push_front(currentState);
        
        // === ОГРАНИЧИВАЕМ РАЗМЕР ИСТОРИИ ===
        // Храним только необходимое количество кадров
        size_t maxHistorySize = static_cast<size_t>(
            (m_fields->ghostCount + 1) * m_fields->frameDelay + 1
        );
        
        while (m_fields->positionHistory.size() > maxHistorySize) {
            m_fields->positionHistory.pop_back();
        }
        
        // === ОБНОВЛЯЕМ ПОЗИЦИИ ПРИЗРАКОВ ===
        updateGhostPositions(dt);
    }

    // ----------------------------------------
    // ОБНОВЛЕНИЕ ПОЗИЦИЙ ПРИЗРАКОВ
    // ----------------------------------------
    
    void updateGhostPositions(float dt) {
        // Обновляем радужный оттенок
        if (m_fields->useRainbow) {
            m_fields->rainbowHue += dt * 60.0f;  // Скорость смены цвета
            if (m_fields->rainbowHue >= 360.0f) {
                m_fields->rainbowHue -= 360.0f;
            }
        }
        
        for (size_t i = 0; i < m_fields->ghosts.size(); i++) {
            auto ghost = m_fields->ghosts[i];
            if (!ghost) continue;
            
            // Вычисляем индекс в истории для этого призрака
            // Чем дальше призрак, тем более старую позицию он использует
            size_t historyIndex = (i + 1) * m_fields->frameDelay;
            
            // Проверяем, достаточно ли данных в истории
            if (historyIndex < m_fields->positionHistory.size()) {
                const PlayerState& state = m_fields->positionHistory[historyIndex];
                
                // === ПРИМЕНЯЕМ ТРАНСФОРМАЦИИ ===
                ghost->setPosition(state.position);
                ghost->setRotation(state.rotation);
                ghost->setScaleX(state.scaleX * (0.95f - i * 0.02f));
                ghost->setScaleY(state.scaleY * (0.95f - i * 0.02f));
                ghost->setFlipX(state.flipX);
                ghost->setFlipY(state.flipY);
                ghost->setVisible(state.visible);
                
                // === ЦВЕТ ===
                if (m_fields->useRainbow) {
                    // Радужный режим: каждый призрак имеет свой оттенок
                    float hue = m_fields->rainbowHue + (i * 30.0f);
                    ghost->setColor(hsvToRgb(hue, 0.8f, 1.0f));
                } else {
                    // Обычный режим: копируем цвет игрока
                    ghost->setColor(state.color);
                }
                
            } else {
                // Недостаточно истории — скрываем призрака
                ghost->setVisible(false);
            }
        }
    }

    // ----------------------------------------
    // ВСПОМОГАТЕЛЬНАЯ ФУНКЦИЯ: HSV -> RGB
    // ----------------------------------------
    
    cocos2d::ccColor3B hsvToRgb(float h, float s, float v) {
        // Нормализуем оттенок
        while (h >= 360.0f) h -= 360.0f;
        while (h < 0.0f) h += 360.0f;
        
        float c = v * s;
        float x = c * (1.0f - std::abs(fmod(h / 60.0f, 2.0f) - 1.0f));
        float m = v - c;
        
        float r, g, b;
        
        if (h < 60.0f)       { r = c; g = x; b = 0; }
        else if (h < 120.0f) { r = x; g = c; b = 0; }
        else if (h < 180.0f) { r = 0; g = c; b = x; }
        else if (h < 240.0f) { r = 0; g = x; b = c; }
        else if (h < 300.0f) { r = x; g = 0; b = c; }
        else                 { r = c; g = 0; b = x; }
        
        return cocos2d::ccc3(
            static_cast<GLubyte>((r + m) * 255),
            static_cast<GLubyte>((g + m) * 255),
            static_cast<GLubyte>((b + m) * 255)
        );
    }

    // ----------------------------------------
    // ОЧИСТКА ПРИЗРАКОВ
    // ----------------------------------------
    
    void cleanupGhosts() {
        for (auto ghost : m_fields->ghosts) {
            if (ghost && ghost->getParent()) {
                ghost->removeFromParent();
            }
        }
        m_fields->ghosts.clear();
        m_fields->positionHistory.clear();
    }

    // ----------------------------------------
    // СБРОС ПРИ СМЕРТИ/РЕСТАРТЕ
    // ----------------------------------------
    
    void resetLevel() {
        // Очищаем историю при рестарте
        m_fields->positionHistory.clear();
        
        // Скрываем всех призраков
        for (auto ghost : m_fields->ghosts) {
            if (ghost) {
                ghost->setVisible(false);
            }
        }
        
        // Вызываем оригинальный метод
        PlayLayer::resetLevel();
        
        log::debug("Echo Trails: Уровень сброшен");
    }

    // ----------------------------------------
    // ВЫХОД ИЗ УРОВНЯ
    // ----------------------------------------
    
    void onQuit() {
        // Полная очистка ресурсов
        cleanupGhosts();
        m_fields->initialized = false;
        
        log::info("Echo Trails: Выход из уровня, ресурсы очищены");
        
        // Вызываем оригинальный метод
        PlayLayer::onQuit();
    }
};

// ============================================
// ТОЧКА ВХОДА МОДА
// ============================================

$on_mod(Loaded) {
    log::info("=================================");
    log::info("   Echo Trails Mod v1.0.0");
    log::info("   Призрачные следы загружены!");
    log::info("=================================");
}
