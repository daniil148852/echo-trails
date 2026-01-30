#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <fstream>
#include <vector>

using namespace geode::prelude;

// ============================================================================
// СТРУКТУРЫ ДАННЫХ
// ============================================================================

#pragma pack(push, 1)
struct GhostFrame {
    float time;
    
    // Player 1
    float posX;
    float posY;
    float rotation;
    float scale;
    uint8_t flags1; // bits: flipX, flipY, isUpsideDown, isHidden, isDashing, isHolding, isFalling, isOnGround
    uint8_t gameMode; // 0=cube, 1=ship, 2=ball, 3=ufo, 4=wave, 5=robot, 6=spider, 7=swing
    
    // Player 2 (dual mode)
    float pos2X;
    float pos2Y;
    float rotation2;
    uint8_t flags2;
    uint8_t gameMode2;
    bool hasDual;
    
    // Вспомогательные методы
    bool getFlipX() const { return flags1 & 0x01; }
    bool getFlipY() const { return flags1 & 0x02; }
    bool getUpsideDown() const { return flags1 & 0x04; }
    bool getHidden() const { return flags1 & 0x08; }
    bool getDashing() const { return flags1 & 0x10; }
    
    bool getFlipX2() const { return flags2 & 0x01; }
    bool getFlipY2() const { return flags2 & 0x02; }
    bool getUpsideDown2() const { return flags2 & 0x04; }
    
    void setFlags(bool flipX, bool flipY, bool upsideDown, bool hidden, bool dashing) {
        flags1 = (flipX ? 0x01 : 0) | (flipY ? 0x02 : 0) | 
                 (upsideDown ? 0x04 : 0) | (hidden ? 0x08 : 0) | (dashing ? 0x10 : 0);
    }
    
    void setFlags2(bool flipX, bool flipY, bool upsideDown) {
        flags2 = (flipX ? 0x01 : 0) | (flipY ? 0x02 : 0) | (upsideDown ? 0x04 : 0);
    }
};
#pragma pack(pop)

// Заголовок файла записи
struct GhostFileHeader {
    char magic[4] = {'E', 'C', 'H', 'O'};
    uint32_t version = 2;
    int32_t levelID;
    float bestTime;
    float recordFPS;
    uint32_t frameCount;
    uint32_t checksum;
};

// ============================================================================
// GHOST MANAGER
// ============================================================================

class GhostManager {
private:
    GhostManager() = default;
    
public:
    static GhostManager* get() {
        static GhostManager instance;
        return &instance;
    }
    
    // === Состояние записи ===
    std::vector<GhostFrame> m_recording;
    float m_recordTime = 0.0f;
    float m_lastRecordTime = 0.0f;
    bool m_isRecording = false;
    
    // === Данные призрака ===
    std::vector<GhostFrame> m_ghostFrames;
    float m_playbackTime = 0.0f;
    size_t m_currentFrameIdx = 0;
    bool m_hasGhost = false;
    float m_bestTime = 999999.0f;
    float m_ghostRecordFPS = 60.0f;
    
    // === Визуализация ===
    CCSprite* m_ghostSprite1 = nullptr;
    CCSprite* m_ghostSprite2 = nullptr;
    CCSprite* m_ghostGlow1 = nullptr;
    CCSprite* m_ghostGlow2 = nullptr;
    CCLabelBMFont* m_timeLabel = nullptr;
    CCNode* m_ghostContainer = nullptr;
    
    // === Кэш ===
    int m_levelID = 0;
    float m_recordInterval = 0.0f;
    float m_cachedOpacity = 0.5f;
    bool m_cachedEnabled = true;
    bool m_cachedShowTimeDiff = true;
    bool m_cachedGlowEffect = true;
    ccColor3B m_cachedGhostColor = ccc3(100, 200, 255);
    
    // === Методы ===
    
    void refreshSettings() {
        auto* mod = Mod::get();
        m_cachedEnabled = mod->getSettingValue<bool>("enabled");
        m_cachedOpacity = static_cast<float>(mod->getSettingValue<double>("ghost-opacity"));
        m_cachedShowTimeDiff = mod->getSettingValue<bool>("show-time-diff");
        m_cachedGlowEffect = mod->getSettingValue<bool>("glow-effect");
        
        int64_t fps = mod->getSettingValue<int64_t>("record-fps");
        m_recordInterval = fps > 0 ? 1.0f / static_cast<float>(fps) : 0.0f;
        
        // Парсим цвет
        auto colorStr = mod->getSettingValue<std::string>("ghost-color");
        parseColor(colorStr);
    }
    
    void parseColor(const std::string& colorStr) {
        if (colorStr == "blue") m_cachedGhostColor = ccc3(100, 180, 255);
        else if (colorStr == "red") m_cachedGhostColor = ccc3(255, 100, 100);
        else if (colorStr == "green") m_cachedGhostColor = ccc3(100, 255, 150);
        else if (colorStr == "purple") m_cachedGhostColor = ccc3(200, 100, 255);
        else if (colorStr == "yellow") m_cachedGhostColor = ccc3(255, 230, 100);
        else if (colorStr == "cyan") m_cachedGhostColor = ccc3(100, 255, 255);
        else if (colorStr == "white") m_cachedGhostColor = ccc3(255, 255, 255);
        else m_cachedGhostColor = ccc3(100, 180, 255);
    }
    
    std::filesystem::path getGhostDir() {
        auto dir = Mod::get()->getSaveDir() / "ghost_replays";
        std::error_code ec;
        std::filesystem::create_directories(dir, ec);
        return dir;
    }
    
    std::filesystem::path getGhostPath(int levelID) {
        return getGhostDir() / (fmt::format("level_{}.echo", levelID));
    }
    
    uint32_t calculateChecksum(const std::vector<GhostFrame>& frames) {
        uint32_t checksum = 0;
        for (size_t i = 0; i < frames.size(); i += 10) {
            checksum ^= static_cast<uint32_t>(frames[i].posX * 100);
            checksum ^= static_cast<uint32_t>(frames[i].posY * 100) << 8;
            checksum = (checksum << 3) | (checksum >> 29);
        }
        return checksum;
    }
    
    void startRecording(int levelID) {
        m_levelID = levelID;
        m_recording.clear();
        m_recording.reserve(10000); // Предаллокация
        m_recordTime = 0.0f;
        m_lastRecordTime = 0.0f;
        m_isRecording = true;
        
        // Сброс воспроизведения
        m_playbackTime = 0.0f;
        m_currentFrameIdx = 0;
    }
    
    void stopRecording() {
        m_isRecording = false;
    }
    
    void resetPlayback() {
        m_playbackTime = 0.0f;
        m_currentFrameIdx = 0;
        m_recordTime = 0.0f;
        m_lastRecordTime = 0.0f;
        m_recording.clear();
        m_recording.reserve(10000);
        m_isRecording = true;
    }
    
    uint8_t getGameMode(PlayerObject* player) {
        if (!player) return 0;
        if (player->m_isShip) return 1;
        if (player->m_isBall) return 2;
        if (player->m_isBird) return 3;  // UFO
        if (player->m_isDart) return 4;  // Wave
        if (player->m_isRobot) return 5;
        if (player->m_isSpider) return 6;
        if (player->m_isSwing) return 7;
        return 0; // Cube
    }
    
    void recordFrame(float dt, PlayerObject* player1, PlayerObject* player2) {
        if (!m_isRecording || !player1) return;
        if (m_recordInterval <= 0.0f) return;
        
        m_recordTime += dt;
        
        // Записываем с заданной частотой
        if (m_recordTime - m_lastRecordTime < m_recordInterval) return;
        m_lastRecordTime = m_recordTime;
        
        GhostFrame frame{};
        frame.time = m_recordTime;
        
        // Player 1
        CCPoint pos1 = player1->getPosition();
        frame.posX = pos1.x;
        frame.posY = pos1.y;
        frame.rotation = player1->getRotation();
        frame.scale = player1->getScale();
        frame.gameMode = getGameMode(player1);
        
        bool flipX1 = false;
        bool flipY1 = false;
        if (player1->m_iconSprite) {
            flipX1 = player1->m_iconSprite->isFlipX();
            flipY1 = player1->m_iconSprite->isFlipY();
        }
        if (player1->m_isUpsideDown) flipX1 = !flipX1;
        
        frame.setFlags(flipX1, flipY1, player1->m_isUpsideDown, 
                      player1->m_isHidden, player1->m_isDashing);
        
        // Player 2 (dual)
        frame.hasDual = player2 != nullptr && player2->isVisible();
        if (frame.hasDual) {
            CCPoint pos2 = player2->getPosition();
            frame.pos2X = pos2.x;
            frame.pos2Y = pos2.y;
            frame.rotation2 = player2->getRotation();
            frame.gameMode2 = getGameMode(player2);
            
            bool flipX2 = false;
            bool flipY2 = false;
            if (player2->m_iconSprite) {
                flipX2 = player2->m_iconSprite->isFlipX();
                flipY2 = player2->m_iconSprite->isFlipY();
            }
            if (player2->m_isUpsideDown) flipX2 = !flipX2;
            
            frame.setFlags2(flipX2, flipY2, player2->m_isUpsideDown);
        }
        
        m_recording.push_back(frame);
    }
    
    bool saveGhost(float completionTime) {
        if (m_recording.empty()) {
            log::warn("No recording to save");
            return false;
        }
        
        // Сохраняем только если лучше
        if (m_hasGhost && completionTime >= m_bestTime) {
            log::info("Current time {} not better than best {}", completionTime, m_bestTime);
            return false;
        }
        
        auto path = getGhostPath(m_levelID);
        
        std::ofstream file(path, std::ios::binary);
        if (!file.is_open()) {
            log::error("Failed to open file for writing: {}", path.string());
            return false;
        }
        
        GhostFileHeader header{};
        header.levelID = m_levelID;
        header.bestTime = completionTime;
        header.recordFPS = 1.0f / m_recordInterval;
        header.frameCount = static_cast<uint32_t>(m_recording.size());
        header.checksum = calculateChecksum(m_recording);
        
        file.write(reinterpret_cast<const char*>(&header), sizeof(header));
        file.write(reinterpret_cast<const char*>(m_recording.data()), 
                   m_recording.size() * sizeof(GhostFrame));
        
        m_bestTime = completionTime;
        m_ghostFrames = m_recording; // Копируем для немедленного воспроизведения
        m_hasGhost = true;
        
        log::info("Ghost saved! Time: {:.3f}s, Frames: {}", completionTime, m_recording.size());
        return true;
    }
    
    bool loadGhost(int levelID) {
        m_levelID = levelID;
        m_ghostFrames.clear();
        m_hasGhost = false;
        m_bestTime = 999999.0f;
        m_currentFrameIdx = 0;
        m_playbackTime = 0.0f;
        
        auto path = getGhostPath(levelID);
        
        if (!std::filesystem::exists(path)) {
            log::info("No ghost file for level {}", levelID);
            return false;
        }
        
        std::ifstream file(path, std::ios::binary);
        if (!file.is_open()) {
            log::error("Failed to open ghost file: {}", path.string());
            return false;
        }
        
        GhostFileHeader header{};
        file.read(reinterpret_cast<char*>(&header), sizeof(header));
        
        // Проверяем магию
        if (std::memcmp(header.magic, "ECHO", 4) != 0) {
            log::error("Invalid ghost file magic");
            return false;
        }
        
        if (header.version != 2) {
            log::error("Unsupported ghost file version: {}", header.version);
            return false;
        }
        
        m_bestTime = header.bestTime;
        m_ghostRecordFPS = header.recordFPS;
        
        m_ghostFrames.resize(header.frameCount);
        file.read(reinterpret_cast<char*>(m_ghostFrames.data()), 
                  header.frameCount * sizeof(GhostFrame));
        
        // Проверяем checksum
        if (calculateChecksum(m_ghostFrames) != header.checksum) {
            log::warn("Ghost file checksum mismatch, file may be corrupted");
        }
        
        m_hasGhost = !m_ghostFrames.empty();
        log::info("Ghost loaded! Best time: {:.3f}s, Frames: {}", m_bestTime, m_ghostFrames.size());
        
        return m_hasGhost;
    }
    
    // Интерполяция между кадрами
    GhostFrame interpolateFrames(const GhostFrame& a, const GhostFrame& b, float t) {
        GhostFrame result = a;
        
        result.posX = a.posX + (b.posX - a.posX) * t;
        result.posY = a.posY + (b.posY - a.posY) * t;
        
        // Интерполяция угла с учётом wrap-around
        float angleDiff = b.rotation - a.rotation;
        if (angleDiff > 180.0f) angleDiff -= 360.0f;
        if (angleDiff < -180.0f) angleDiff += 360.0f;
        result.rotation = a.rotation + angleDiff * t;
        
        result.scale = a.scale + (b.scale - a.scale) * t;
        
        if (a.hasDual && b.hasDual) {
            result.pos2X = a.pos2X + (b.pos2X - a.pos2X) * t;
            result.pos2Y = a.pos2Y + (b.pos2Y - a.pos2Y) * t;
            
            float angleDiff2 = b.rotation2 - a.rotation2;
            if (angleDiff2 > 180.0f) angleDiff2 -= 360.0f;
            if (angleDiff2 < -180.0f) angleDiff2 += 360.0f;
            result.rotation2 = a.rotation2 + angleDiff2 * t;
        }
        
        return result;
    }
    
    GhostFrame* getInterpolatedFrame(float time, GhostFrame& outFrame) {
        if (!m_hasGhost || m_ghostFrames.empty()) return nullptr;
        
        // Находим нужные кадры для интерполяции
        while (m_currentFrameIdx < m_ghostFrames.size() - 1 && 
               m_ghostFrames[m_currentFrameIdx + 1].time <= time) {
            m_currentFrameIdx++;
        }
        
        if (m_currentFrameIdx >= m_ghostFrames.size()) return nullptr;
        
        const GhostFrame& current = m_ghostFrames[m_currentFrameIdx];
        
        // Если это последний кадр или время точно совпадает
        if (m_currentFrameIdx >= m_ghostFrames.size() - 1 || current.time >= time) {
            outFrame = current;
            return &outFrame;
        }
        
        // Интерполяция
        const GhostFrame& next = m_ghostFrames[m_currentFrameIdx + 1];
        float t = (time - current.time) / (next.time - current.time);
        t = std::clamp(t, 0.0f, 1.0f);
        
        outFrame = interpolateFrames(current, next, t);
        return &outFrame;
    }
    
    void createGhostVisuals(CCNode* gameLayer, PlayerObject* realPlayer) {
        cleanupVisuals();
        
        if (!gameLayer || !realPlayer) return;
        
        // Контейнер для призрака
        m_ghostContainer = CCNode::create();
        if (!m_ghostContainer) return;
        gameLayer->addChild(m_ghostContainer, realPlayer->getZOrder() - 5);
        
        // Создаём спрайт призрака 1
        if (realPlayer->m_iconSprite) {
            CCSpriteFrame* frame = realPlayer->m_iconSprite->displayFrame();
            if (frame) {
                m_ghostSprite1 = CCSprite::createWithSpriteFrame(frame);
            }
        }
        
        if (!m_ghostSprite1) {
            m_ghostSprite1 = CCSprite::create("playerSquare_001.png"_spr);
        }
        
        if (m_ghostSprite1) {
            m_ghostSprite1->setColor(m_cachedGhostColor);
            m_ghostSprite1->setOpacity(static_cast<GLubyte>(m_cachedOpacity * 255));
            m_ghostContainer->addChild(m_ghostSprite1, 1);
            
            // Эффект свечения
            if (m_cachedGlowEffect) {
                m_ghostGlow1 = CCSprite::createWithSpriteFrame(
                    m_ghostSprite1->displayFrame()
                );
                if (m_ghostGlow1) {
                    m_ghostGlow1->setColor(m_cachedGhostColor);
                    m_ghostGlow1->setOpacity(static_cast<GLubyte>(m_cachedOpacity * 100));
                    m_ghostGlow1->setScale(1.3f);
                    m_ghostGlow1->setBlendFunc({GL_SRC_ALPHA, GL_ONE});
                    m_ghostContainer->addChild(m_ghostGlow1, 0);
                }
            }
        }
        
        // Метка разницы времени
        if (m_cachedShowTimeDiff) {
            m_timeLabel = CCLabelBMFont::create("", "bigFont.fnt");
            if (m_timeLabel) {
                m_timeLabel->setScale(0.4f);
                m_timeLabel->setOpacity(200);
                m_ghostContainer->addChild(m_timeLabel, 10);
            }
        }
    }
    
    void updateGhostVisuals(float currentTime, PlayerObject* realPlayer) {
        m_playbackTime = currentTime;
        
        if (!m_cachedEnabled || !m_hasGhost || !m_ghostSprite1) {
            if (m_ghostContainer) m_ghostContainer->setVisible(false);
            return;
        }
        
        GhostFrame interpolated;
        GhostFrame* frame = getInterpolatedFrame(currentTime, interpolated);
        
        if (!frame) {
            m_ghostContainer->setVisible(false);
            return;
        }
        
        m_ghostContainer->setVisible(true);
        
        // Проверяем видимость (не показываем скрытый призрак)
        if (frame->getHidden()) {
            m_ghostSprite1->setVisible(false);
            if (m_ghostGlow1) m_ghostGlow1->setVisible(false);
            return;
        }
        
        m_ghostSprite1->setVisible(true);
        m_ghostSprite1->setPosition(ccp(frame->posX, frame->posY));
        m_ghostSprite1->setRotation(frame->rotation);
        m_ghostSprite1->setScale(frame->scale);
        m_ghostSprite1->setFlipX(frame->getFlipX());
        m_ghostSprite1->setFlipY(frame->getFlipY());
        
        // Обновляем текстуру если режим сменился
        updateGhostTexture(realPlayer, frame->gameMode);
        
        // Обновляем свечение
        if (m_ghostGlow1) {
            m_ghostGlow1->setVisible(true);
            m_ghostGlow1->setPosition(m_ghostSprite1->getPosition());
            m_ghostGlow1->setRotation(m_ghostSprite1->getRotation());
            m_ghostGlow1->setScale(frame->scale * 1.3f);
            m_ghostGlow1->setFlipX(frame->getFlipX());
            m_ghostGlow1->setFlipY(frame->getFlipY());
        }
        
        // Dual mode player 2
        if (frame->hasDual && m_ghostSprite2) {
            m_ghostSprite2->setVisible(true);
            m_ghostSprite2->setPosition(ccp(frame->pos2X, frame->pos2Y));
            m_ghostSprite2->setRotation(frame->rotation2);
            m_ghostSprite2->setFlipX(frame->getFlipX2());
            m_ghostSprite2->setFlipY(frame->getFlipY2());
        } else if (m_ghostSprite2) {
            m_ghostSprite2->setVisible(false);
        }
        
        // Разница времени
        updateTimeLabel(realPlayer, frame->posX);
    }
    
    void updateGhostTexture(PlayerObject* realPlayer, uint8_t gameMode) {
        if (!m_ghostSprite1 || !realPlayer) return;
        
        CCSprite* sourceSprite = nullptr;
        
        // Выбираем нужный спрайт в зависимости от режима
        switch (gameMode) {
            case 1: // Ship
                sourceSprite = realPlayer->m_iconSpriteSecondary;
                if (!sourceSprite) sourceSprite = realPlayer->m_iconSprite;
                break;
            case 2: // Ball
            case 3: // UFO
            case 4: // Wave
            case 5: // Robot
            case 6: // Spider
            case 7: // Swing
            default:
                sourceSprite = realPlayer->m_iconSprite;
                break;
        }
        
        if (sourceSprite) {
            CCSpriteFrame* frame = sourceSprite->displayFrame();
            if (frame) {
                m_ghostSprite1->setDisplayFrame(frame);
                if (m_ghostGlow1) {
                    m_ghostGlow1->setDisplayFrame(frame);
                }
            }
        }
    }
    
    void updateTimeLabel(PlayerObject* realPlayer, float ghostX) {
        if (!m_timeLabel || !m_cachedShowTimeDiff || !realPlayer) return;
        
        float playerX = realPlayer->getPositionX();
        float diff = playerX - ghostX;
        
        // Позиционируем метку над призраком
        m_timeLabel->setPosition(ccp(ghostX, m_ghostSprite1->getPositionY() + 50.0f));
        
        // Форматируем текст
        std::string text;
        ccColor3B color;
        
        if (diff > 5.0f) {
            // Игрок впереди (хорошо)
            text = fmt::format("+{:.0f}", diff);
            color = ccc3(100, 255, 100);
        } else if (diff < -5.0f) {
            // Игрок позади (плохо)
            text = fmt::format("{:.0f}", diff);
            color = ccc3(255, 100, 100);
        } else {
            // Примерно равны
            text = "=";
            color = ccc3(255, 255, 100);
        }
        
        m_timeLabel->setString(text.c_str());
        m_timeLabel->setColor(color);
    }
    
    void cleanupVisuals() {
        if (m_ghostContainer) {
            m_ghostContainer->removeFromParent();
            m_ghostContainer = nullptr;
        }
        m_ghostSprite1 = nullptr;
        m_ghostSprite2 = nullptr;
        m_ghostGlow1 = nullptr;
        m_ghostGlow2 = nullptr;
        m_timeLabel = nullptr;
    }
    
    void fullCleanup() {
        cleanupVisuals();
        m_recording.clear();
        m_ghostFrames.clear();
        m_hasGhost = false;
        m_isRecording = false;
        m_currentFrameIdx = 0;
        m_recordTime = 0.0f;
        m_playbackTime = 0.0f;
    }
    
    bool deleteGhost(int levelID) {
        auto path = getGhostPath(levelID);
        std::error_code ec;
        if (std::filesystem::remove(path, ec)) {
            log::info("Ghost deleted for level {}", levelID);
            if (levelID == m_levelID) {
                m_ghostFrames.clear();
                m_hasGhost = false;
                m_bestTime = 999999.0f;
            }
            return true;
        }
        return false;
    }
};

// ============================================================================
// PLAYLAYER HOOKS
// ============================================================================

class $modify(EchoPlayLayer, PlayLayer) {
    struct Fields {
        float m_sessionTime = 0.0f;
        float m_settingsTimer = 0.0f;
        bool m_initialized = false;
        bool m_completedLevel = false;
    };
    
    bool init(GJGameLevel* level, bool useReplay, bool dontCreateObjects) {
        if (!PlayLayer::init(level, useReplay, dontCreateObjects)) {
            return false;
        }
        
        auto* gm = GhostManager::get();
        gm->refreshSettings();
        
        if (!gm->m_cachedEnabled) return true;
        
        // Получаем ID уровня
        int levelID = level->m_levelID.value();
        if (levelID == 0) {
            // Для кастомных уровней используем хэш
            levelID = static_cast<int>(std::hash<std::string>{}(
                std::string(level->m_levelName) + std::to_string(level->m_levelRev)
            ) & 0x7FFFFFFF);
        }
        
        // Загружаем призрака
        gm->loadGhost(levelID);
        
        // Начинаем запись
        gm->startRecording(levelID);
        
        m_fields->m_initialized = true;
        m_fields->m_sessionTime = 0.0f;
        m_fields->m_completedLevel = false;
        
        log::info("Echo Trails initialized for level {}", levelID);
        
        return true;
    }
    
    void update(float dt) {
        PlayLayer::update(dt);
        
        auto* gm = GhostManager::get();
        
        if (!m_fields->m_initialized || !gm->m_cachedEnabled) return;
        if (m_player1->m_isDead) return;
        
        // Обновляем настройки периодически
        m_fields->m_settingsTimer += dt;
        if (m_fields->m_settingsTimer >= 1.0f) {
            m_fields->m_settingsTimer = 0.0f;
            gm->refreshSettings();
        }
        
        // Записываем кадр
        m_fields->m_sessionTime += dt;
        gm->recordFrame(dt, m_player1, m_player2);
        
        // Создаём визуализацию если ещё не создана
        if (!gm->m_ghostContainer && gm->m_hasGhost) {
            gm->createGhostVisuals(m_objectLayer, m_player1);
        }
        
        // Обновляем визуализацию
        gm->updateGhostVisuals(m_fields->m_sessionTime, m_player1);
    }
    
    void resetLevel() {
        PlayLayer::resetLevel();
        
        auto* gm = GhostManager::get();
        
        if (!gm->m_cachedEnabled) return;
        
        m_fields->m_sessionTime = 0.0f;
        m_fields->m_completedLevel = false;
        gm->resetPlayback();
        
        // Пересоздаём визуализацию
        if (gm->m_hasGhost) {
            gm->cleanupVisuals();
            gm->createGhostVisuals(m_objectLayer, m_player1);
        }
    }
    
    void levelComplete() {
        auto* gm = GhostManager::get();
        
        if (gm->m_cachedEnabled && !m_fields->m_completedLevel) {
            m_fields->m_completedLevel = true;
            gm->stopRecording();
            
            float completionTime = m_fields->m_sessionTime;
            
            if (gm->saveGhost(completionTime)) {
                // Показываем уведомление
                auto* notification = Notification::create(
                    fmt::format("New Best Ghost! {:.3f}s", completionTime),
                    NotificationIcon::Success,
                    2.0f
                );
                notification->show();
            }
        }
        
        PlayLayer::levelComplete();
    }
    
    void onQuit() {
        GhostManager::get()->fullCleanup();
        PlayLayer::onQuit();
    }
    
    ~EchoPlayLayer() {
        GhostManager::get()->cleanupVisuals();
    }
};

// ============================================================================
// МЕНЮ УДАЛЕНИЯ ПРИЗРАКА (в паузе)
// ============================================================================

#include <Geode/modify/PauseLayer.hpp>

class $modify(EchoPauseLayer, PauseLayer) {
    void customSetup() {
        PauseLayer::customSetup();
        
        auto* gm = GhostManager::get();
        if (!gm->m_hasGhost) return;
        
        // Кнопка удаления призрака
        auto* menu = this->getChildByID("right-button-menu");
        if (!menu) {
            menu = CCMenu::create();
            menu->setPosition(CCDirector::sharedDirector()->getWinSize().width - 50.0f, 80.0f);
            this->addChild(menu, 10);
        }
        
        auto* deleteBtn = CCMenuItemSpriteExtra::create(
            CCSprite::createWithSpriteFrameName("GJ_deleteIcon_001.png"),
            this,
            menu_selector(EchoPauseLayer::onDeleteGhost)
        );
        deleteBtn->setScale(0.8f);
        
        if (menu->getChildByID("delete-ghost-btn"_spr)) return;
        
        deleteBtn->setID("delete-ghost-btn"_spr);
        menu->addChild(deleteBtn);
        menu->updateLayout();
    }
    
    void onDeleteGhost(CCObject* sender) {
        auto* gm = GhostManager::get();
        
        geode::createQuickPopup(
            "Delete Ghost",
            fmt::format("Delete ghost replay for this level?\n\nBest Time: {:.3f}s", gm->m_bestTime),
            "Cancel", "Delete",
            [](auto, bool confirmed) {
                if (confirmed) {
                    auto* gm = GhostManager::get();
                    if (gm->deleteGhost(gm->m_levelID)) {
                        Notification::create("Ghost deleted!", NotificationIcon::Success)->show();
                    }
                }
            }
        );
    }
};
