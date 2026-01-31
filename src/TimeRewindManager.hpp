#pragma once

#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <deque>
#include <vector>
#include <functional>

using namespace geode::prelude;

/**
 * @brief Stores the complete state of a PlayerObject at a given frame
 */
struct PlayerFrameState {
    // Transform
    cocos2d::CCPoint position = {0, 0};
    double rotation = 0.0;
    float scaleX = 1.0f;
    float scaleY = 1.0f;
    
    // Physics
    double yVelocity = 0.0;
    double xVelocity = 0.0;  // Platformer X velocity
    float playerSpeed = 1.0f;
    
    // State flags
    bool isUpsideDown = false;
    bool isOnGround = false;
    bool isDashing = false;
    bool isSliding = false;
    bool isRising = false;  // Calculated from velocity
    bool isFalling = false; // Calculated from velocity
    bool isDead = false;
    bool isVisible = true;
    bool isLocked = false;
    
    // Gamemode flags
    bool isCube = true;
    bool isShip = false;
    bool isBall = false;
    bool isUFO = false;
    bool isWave = false;
    bool isRobot = false;
    bool isSpider = false;
    bool isSwing = false;
    
    // Size
    bool isMini = false;
    bool isDualMode = false;
    
    // Trail state
    bool hasGhostTrail = false;
    
    PlayerFrameState() = default;
};

/**
 * @brief Complete game state at a single frame
 */
struct FrameState {
    // Player states
    PlayerFrameState player1;
    PlayerFrameState player2;
    
    // Timing
    double musicTimeMS = 0.0;
    double levelTime = 0.0;
    float attemptTime = 0.0f;
    unsigned int frameNumber = 0;
    
    // Camera state
    cocos2d::CCPoint cameraPosition = {0, 0};
    float cameraZoom = 1.0f;
    float cameraAngle = 0.0f;
    
    // Level state
    bool isDualMode = false;
    bool isPlatformer = false;
    float gameSpeed = 1.0f;
    
    // Checkpoints (for platformer)
    int lastCheckpointID = -1;
    
    FrameState() = default;
};

/**
 * @brief State of the rewind system
 */
enum class RewindState {
    Idle,       // Not active
    Recording,  // Actively recording frames
    Rewinding,  // Playing back frames in reverse
    Resuming,   // Transitioning back to gameplay
    Paused      // Temporarily paused (game paused)
};

/**
 * @brief Singleton manager for the Time Rewind system
 */
class TimeRewindManager {
private:
    static TimeRewindManager* s_instance;
    
    // Frame buffer (circular buffer using deque)
    std::deque<FrameState> m_frameBuffer;
    size_t m_maxFrameCount;
    
    // Recording settings
    float m_recordInterval;      // Time between frame recordings
    float m_timeSinceLastRecord;
    unsigned int m_frameCounter;
    
    // Rewind settings
    int m_rewindCharges;
    int m_maxRewindCharges;
    float m_rewindDurationSec;   // How far back to rewind
    float m_rewindSpeed;         // Playback speed multiplier
    
    // Current rewind state
    RewindState m_state;
    size_t m_rewindStartIndex;
    size_t m_rewindEndIndex;
    size_t m_currentRewindIndex;
    float m_rewindProgress;      // 0.0 to 1.0
    float m_rewindTimer;
    float m_totalRewindTime;
    
    // Control flags
    bool m_enabled;
    bool m_inputBlocked;
    bool m_useAudioSync;
    bool m_useVisualEffects;
    
    // Cached references
    PlayLayer* m_cachedPlayLayer;
    
    // Private constructor (singleton)
    TimeRewindManager();
    ~TimeRewindManager();
    
public:
    /**
     * @brief Get the singleton instance
     */
    static TimeRewindManager* get();
    
    /**
     * @brief Destroy the singleton instance
     */
    static void destroy();
    
    // ==================== Lifecycle ====================
    
    void initialize(PlayLayer* playLayer);
    void reset();
    void cleanup();
    
    // ==================== Recording ====================
    
    bool shouldRecordFrame(float deltaTime);
    void recordFrame(PlayLayer* playLayer);
    PlayerFrameState capturePlayerState(PlayerObject* player);
    FrameState captureGameState(PlayLayer* playLayer);
    
    // ==================== Rewind ====================
    
    bool canRewind() const;
    bool startRewind(PlayLayer* playLayer);
    void updateRewind(PlayLayer* playLayer, float deltaTime);
    void finishRewind(PlayLayer* playLayer);
    void cancelRewind(PlayLayer* playLayer);
    
    // ==================== State Application ====================
    
    void applyFrameState(PlayLayer* playLayer, const FrameState& state);
    void applyPlayerState(PlayerObject* player, const PlayerFrameState& state);
    FrameState interpolateFrames(const FrameState& from, const FrameState& to, float t);
    PlayerFrameState interpolatePlayerStates(const PlayerFrameState& from, 
                                              const PlayerFrameState& to, float t);
    
    // ==================== Audio Sync ====================
    
    void syncMusicTime(double timeMS);
    void setMusicPaused(bool paused);
    
    // ==================== Getters/Setters ====================
    
    // State
    RewindState getState() const { return m_state; }
    bool isRewinding() const { return m_state == RewindState::Rewinding; }
    bool isRecording() const { return m_state == RewindState::Recording; }
    bool isInputBlocked() const { return m_inputBlocked; }
    
    // Charges
    int getCharges() const { return m_rewindCharges; }
    int getMaxCharges() const { return m_maxRewindCharges; }
    void setMaxCharges(int charges);
    void resetCharges();
    void useCharge();
    void addCharge();
    
    // Settings
    bool isEnabled() const { return m_enabled; }
    void setEnabled(bool enabled) { m_enabled = enabled; }
    
    void setRecordInterval(float interval) { m_recordInterval = interval; }
    float getRecordInterval() const { return m_recordInterval; }
    
    void setRewindDuration(float duration) { m_rewindDurationSec = duration; }
    float getRewindDuration() const { return m_rewindDurationSec; }
    
    void setRewindSpeed(float speed) { m_rewindSpeed = speed; }
    float getRewindSpeed() const { return m_rewindSpeed; }
    
    void setAudioSync(bool sync) { m_useAudioSync = sync; }
    bool getAudioSync() const { return m_useAudioSync; }
    
    void setVisualEffects(bool effects) { m_useVisualEffects = effects; }
    bool getVisualEffects() const { return m_useVisualEffects; }
    
    // Buffer info
    size_t getBufferSize() const { return m_frameBuffer.size(); }
    size_t getMaxBufferSize() const { return m_maxFrameCount; }
    float getBufferDurationSeconds() const;
    float getRewindProgress() const { return m_rewindProgress; }
    
    // ==================== Settings Sync ====================
    
    void loadSettings();
    void saveSettings();
};
