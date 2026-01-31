#include "TimeRewindManager.hpp"
#include "RewindVisuals.hpp"

#include <Geode/binding/FMODAudioEngine.hpp>
#include <Geode/binding/GameManager.hpp>
#include <Geode/binding/PlayLayer.hpp>
#include <Geode/binding/PlayerObject.hpp>
#include <Geode/binding/GJGameLevel.hpp>

using namespace geode::prelude;

// Static instance
TimeRewindManager* TimeRewindManager::s_instance = nullptr;

// ============================================================
// Constructor / Destructor
// ============================================================

TimeRewindManager::TimeRewindManager()
    : m_maxFrameCount(600)           // 10 seconds at 60 FPS
    , m_recordInterval(1.0f / 60.0f) // 60 Hz recording
    , m_timeSinceLastRecord(0.0f)
    , m_frameCounter(0)
    , m_rewindCharges(3)
    , m_maxRewindCharges(3)
    , m_rewindDurationSec(3.0f)
    , m_rewindSpeed(2.0f)
    , m_state(RewindState::Idle)
    , m_rewindStartIndex(0)
    , m_rewindEndIndex(0)
    , m_currentRewindIndex(0)
    , m_rewindProgress(0.0f)
    , m_rewindTimer(0.0f)
    , m_totalRewindTime(0.0f)
    , m_enabled(true)
    , m_inputBlocked(false)
    , m_useAudioSync(true)
    , m_useVisualEffects(true)
    , m_cachedPlayLayer(nullptr)
{
    loadSettings();
}

TimeRewindManager::~TimeRewindManager() {
    cleanup();
}

// ============================================================
// Singleton Management
// ============================================================

TimeRewindManager* TimeRewindManager::get() {
    if (!s_instance) {
        s_instance = new TimeRewindManager();
    }
    return s_instance;
}

void TimeRewindManager::destroy() {
    if (s_instance) {
        delete s_instance;
        s_instance = nullptr;
    }
}

// ============================================================
// Lifecycle
// ============================================================

void TimeRewindManager::initialize(PlayLayer* playLayer) {
    m_cachedPlayLayer = playLayer;
    reset();
    m_state = RewindState::Recording;
    
    log::info("[TimeRewind] Initialized for level");
}

void TimeRewindManager::reset() {
    m_frameBuffer.clear();
    m_timeSinceLastRecord = 0.0f;
    m_frameCounter = 0;
    m_rewindCharges = m_maxRewindCharges;
    m_state = RewindState::Idle;
    m_rewindProgress = 0.0f;
    m_rewindTimer = 0.0f;
    m_inputBlocked = false;
    
    log::debug("[TimeRewind] Reset complete");
}

void TimeRewindManager::cleanup() {
    m_frameBuffer.clear();
    m_cachedPlayLayer = nullptr;
}

// ============================================================
// Recording
// ============================================================

bool TimeRewindManager::shouldRecordFrame(float deltaTime) {
    if (m_state != RewindState::Recording) {
        return false;
    }
    
    m_timeSinceLastRecord += deltaTime;
    
    if (m_timeSinceLastRecord >= m_recordInterval) {
        m_timeSinceLastRecord -= m_recordInterval;
        return true;
    }
    
    return false;
}

void TimeRewindManager::recordFrame(PlayLayer* playLayer) {
    if (!playLayer || !m_enabled) {
        return;
    }
    
    FrameState frame = captureGameState(playLayer);
    frame.frameNumber = m_frameCounter++;
    
    m_frameBuffer.push_back(frame);
    
    // Maintain max buffer size (circular buffer behavior)
    while (m_frameBuffer.size() > m_maxFrameCount) {
        m_frameBuffer.pop_front();
    }
}

PlayerFrameState TimeRewindManager::capturePlayerState(PlayerObject* player) {
    PlayerFrameState state;
    
    if (!player) {
        return state;
    }
    
    // Transform
    state.position = player->getPosition();
    state.rotation = player->getRotation();
    state.scaleX = player->getScaleX();
    state.scaleY = player->getScaleY();
    
    // Physics - using the correct member names for GD 2.2074
    state.yVelocity = player->m_yVelocity;
    state.xVelocity = player->m_platformerXVelocity;
    state.playerSpeed = player->m_playerSpeed;
    
    // State flags
    state.isUpsideDown = player->m_isUpsideDown;
    state.isOnGround = player->m_isOnGround;
    state.isHolding = player->m_isHolding;
    state.isDashing = player->m_isDashing;
    state.isSliding = player->m_isSliding;
    state.isRising = player->m_isRising;
    state.isFalling = !player->m_isRising && !player->m_isOnGround;
    state.isDead = player->m_isDead;
    state.isVisible = player->isVisible();
    state.isLocked = player->m_isLocked;
    
    // Gamemode flags
    state.isShip = player->m_isShip;
    state.isBall = player->m_isBall;
    state.isUFO = player->m_isBird;      // UFO is internally called "bird"
    state.isWave = player->m_isDart;     // Wave is internally called "dart"
    state.isRobot = player->m_isRobot;
    state.isSpider = player->m_isSpider;
    state.isSwing = player->m_isSwing;
    
    // If none of the above are true, it's cube mode
    state.isCube = !state.isShip && !state.isBall && !state.isUFO && 
                   !state.isWave && !state.isRobot && !state.isSpider && !state.isSwing;
    
    // Size
    state.isMini = player->m_vehicleSize == 0.6f;
    
    // Trail/effects
    state.hasGhostTrail = player->m_hasGhostTrail;
    
    return state;
}

FrameState TimeRewindManager::captureGameState(PlayLayer* playLayer) {
    FrameState state;
    
    if (!playLayer) {
        return state;
    }
    
    // Capture player states
    if (playLayer->m_player1) {
        state.player1 = capturePlayerState(playLayer->m_player1);
    }
    
    if (playLayer->m_player2) {
        state.player2 = capturePlayerState(playLayer->m_player2);
    }
    
    // Timing
    auto fmod = FMODAudioEngine::sharedEngine();
    if (fmod) {
        state.musicTimeMS = static_cast<double>(fmod->getMusicTimeMS(0));
    }
    
    state.levelTime = playLayer->m_gameState.m_levelTime;
    state.attemptTime = playLayer->m_attemptTime;
    
    // Camera
    if (playLayer->m_gameState.m_cameraPosition.x != 0 || 
        playLayer->m_gameState.m_cameraPosition.y != 0) {
        state.cameraPosition = playLayer->m_gameState.m_cameraPosition;
    } else {
        state.cameraPosition = playLayer->getPosition();
    }
    state.cameraZoom = playLayer->m_gameState.m_cameraZoom;
    state.cameraAngle = playLayer->m_gameState.m_cameraAngle;
    
    // Level state
    state.isDualMode = playLayer->m_gameState.m_isDualMode;
    state.isPlatformer = playLayer->m_isPlatformer;
    state.isMirrored = playLayer->m_gameState.m_mirrorMode;
    state.gameSpeed = playLayer->m_gameState.m_timeModRelated;
    
    return state;
}

// ============================================================
// Rewind Logic
// ============================================================

bool TimeRewindManager::canRewind() const {
    if (!m_enabled) return false;
    if (m_rewindCharges <= 0) return false;
    if (m_state == RewindState::Rewinding) return false;
    
    // Need at least 30 frames (0.5 seconds at 60 FPS) to rewind
    if (m_frameBuffer.size() < 30) {
        log::debug("[TimeRewind] Not enough frames to rewind: {}", m_frameBuffer.size());
        return false;
    }
    
    return true;
}

bool TimeRewindManager::startRewind(PlayLayer* playLayer) {
    if (!canRewind() || !playLayer) {
        log::warn("[TimeRewind] Cannot start rewind");
        return false;
    }
    
    log::info("[TimeRewind] Starting rewind with {} frames in buffer", m_frameBuffer.size());
    
    // Calculate frame range for rewind
    m_rewindStartIndex = m_frameBuffer.size() - 1;
    
    // Calculate how many frames correspond to rewind duration
    size_t framesToRewind = static_cast<size_t>(m_rewindDurationSec / m_recordInterval);
    framesToRewind = std::min(framesToRewind, m_frameBuffer.size() - 1);
    
    m_rewindEndIndex = m_rewindStartIndex - framesToRewind;
    m_currentRewindIndex = m_rewindStartIndex;
    
    // Calculate total rewind animation time
    m_totalRewindTime = static_cast<float>(framesToRewind) * m_recordInterval / m_rewindSpeed;
    m_rewindTimer = 0.0f;
    m_rewindProgress = 0.0f;
    
    // Change state
    m_state = RewindState::Rewinding;
    m_inputBlocked = true;
    
    // Prevent death immediately
    if (playLayer->m_player1) {
        playLayer->m_player1->m_isDead = false;
        playLayer->m_player1->setVisible(true);
    }
    if (playLayer->m_player2) {
        playLayer->m_player2->m_isDead = false;
        playLayer->m_player2->setVisible(true);
    }
    
    // Pause game music (we'll scrub it manually)
    setMusicPaused(true);
    
    // Show visual effects
    if (m_useVisualEffects) {
        RewindVisuals::get()->showRewindOverlay(playLayer);
    }
    
    // Use a rewind charge
    useCharge();
    
    log::info("[TimeRewind] Rewinding from frame {} to {} ({:.2f}s animation)",
              m_rewindStartIndex, m_rewindEndIndex, m_totalRewindTime);
    
    return true;
}

void TimeRewindManager::updateRewind(PlayLayer* playLayer, float deltaTime) {
    if (m_state != RewindState::Rewinding || !playLayer) {
        return;
    }
    
    m_rewindTimer += deltaTime;
    m_rewindProgress = std::min(m_rewindTimer / m_totalRewindTime, 1.0f);
    
    // Calculate current frame index using smooth interpolation
    float frameProgress = m_rewindProgress;
    // Apply easing (ease out for smoother ending)
    frameProgress = 1.0f - (1.0f - frameProgress) * (1.0f - frameProgress);
    
    size_t totalFrames = m_rewindStartIndex - m_rewindEndIndex;
    size_t currentOffset = static_cast<size_t>(totalFrames * frameProgress);
    m_currentRewindIndex = m_rewindStartIndex - currentOffset;
    
    // Clamp to valid range
    m_currentRewindIndex = std::max(m_currentRewindIndex, m_rewindEndIndex);
    m_currentRewindIndex = std::min(m_currentRewindIndex, m_rewindStartIndex);
    
    // Get frames for interpolation
    if (m_currentRewindIndex < m_frameBuffer.size()) {
        const FrameState& currentFrame = m_frameBuffer[m_currentRewindIndex];
        
        // Calculate interpolation factor between frames
        float subFrameProgress = (totalFrames * frameProgress) - static_cast<float>(currentOffset);
        
        if (m_currentRewindIndex > 0 && subFrameProgress > 0.0f) {
            const FrameState& prevFrame = m_frameBuffer[m_currentRewindIndex - 1];
            FrameState interpolated = interpolateFrames(currentFrame, prevFrame, subFrameProgress);
            applyFrameState(playLayer, interpolated);
        } else {
            applyFrameState(playLayer, currentFrame);
        }
        
        // Sync audio (scrub backwards)
        if (m_useAudioSync) {
            syncMusicTime(currentFrame.musicTimeMS);
        }
    }
    
    // Update visual effects
    if (m_useVisualEffects) {
        RewindVisuals::get()->updateRewindProgress(m_rewindProgress);
    }
    
    // Check if rewind is complete
    if (m_rewindProgress >= 1.0f) {
        finishRewind(playLayer);
    }
}

void TimeRewindManager::finishRewind(PlayLayer* playLayer) {
    if (!playLayer) return;
    
    log::info("[TimeRewind] Finishing rewind");
    
    m_state = RewindState::Resuming;
    
    // Apply the final frame state
    if (m_rewindEndIndex < m_frameBuffer.size()) {
        const FrameState& finalFrame = m_frameBuffer[m_rewindEndIndex];
        applyFrameState(playLayer, finalFrame);
        
        // Sync to final music position
        if (m_useAudioSync) {
            syncMusicTime(finalFrame.musicTimeMS);
        }
    }
    
    // Remove rewound frames from buffer (we're now at an earlier point)
    while (m_frameBuffer.size() > m_rewindEndIndex + 1) {
        m_frameBuffer.pop_back();
    }
    
    // Ensure players are alive and can move
    if (playLayer->m_player1) {
        playLayer->m_player1->m_isDead = false;
        playLayer->m_player1->setVisible(true);
    }
    if (playLayer->m_player2) {
        playLayer->m_player2->m_isDead = false;
        playLayer->m_player2->setVisible(true);
    }
    
    // Hide visual effects
    if (m_useVisualEffects) {
        RewindVisuals::get()->hideRewindOverlay(playLayer);
    }
    
    // Resume music
    setMusicPaused(false);
    
    // Unblock input and resume recording
    m_inputBlocked = false;
    m_state = RewindState::Recording;
    m_rewindProgress = 0.0f;
    m_rewindTimer = 0.0f;
    
    log::info("[TimeRewind] Resumed gameplay, {} frames remaining in buffer", m_frameBuffer.size());
}

void TimeRewindManager::cancelRewind(PlayLayer* playLayer) {
    if (m_state != RewindState::Rewinding) return;
    
    log::info("[TimeRewind] Rewind cancelled");
    
    // Hide visuals
    if (m_useVisualEffects && playLayer) {
        RewindVisuals::get()->hideRewindOverlay(playLayer);
    }
    
    // Resume music
    setMusicPaused(false);
    
    // Reset state
    m_state = RewindState::Recording;
    m_inputBlocked = false;
    m_rewindProgress = 0.0f;
    m_rewindTimer = 0.0f;
}

// ============================================================
// State Application
// ============================================================

void TimeRewindManager::applyFrameState(PlayLayer* playLayer, const FrameState& state) {
    if (!playLayer) return;
    
    // Apply player 1 state
    if (playLayer->m_player1) {
        applyPlayerState(playLayer->m_player1, state.player1);
    }
    
    // Apply player 2 state (dual mode)
    if (playLayer->m_player2 && state.isDualMode) {
        applyPlayerState(playLayer->m_player2, state.player2);
    }
    
    // Apply game timing
    playLayer->m_gameState.m_levelTime = state.levelTime;
    playLayer->m_attemptTime = state.attemptTime;
}

void TimeRewindManager::applyPlayerState(PlayerObject* player, const PlayerFrameState& state) {
    if (!player) return;
    
    // Transform
    player->setPosition(state.position);
    player->setRotation(static_cast<float>(state.rotation));
    player->setScaleX(state.scaleX);
    player->setScaleY(state.scaleY);
    
    // Physics
    player->m_yVelocity = state.yVelocity;
    player->m_platformerXVelocity = state.xVelocity;
    player->m_playerSpeed = state.playerSpeed;
    
    // State flags
    player->m_isUpsideDown = state.isUpsideDown;
    player->m_isOnGround = state.isOnGround;
    player->m_isDashing = state.isDashing;
    player->m_isSliding = state.isSliding;
    player->m_isRising = state.isRising;
    
    // Don't restore holding state - player should re-input
    player->m_isHolding = false;
    
    // Ensure player is alive
    player->m_isDead = false;
    player->setVisible(state.isVisible);
}

FrameState TimeRewindManager::interpolateFrames(const FrameState& from, 
                                                  const FrameState& to, float t) {
    FrameState result = from;
    
    // Interpolate player 1
    result.player1 = interpolatePlayerStates(from.player1, to.player1, t);
    
    // Interpolate player 2
    result.player2 = interpolatePlayerStates(from.player2, to.player2, t);
    
    // Interpolate timing
    result.musicTimeMS = from.musicTimeMS + (to.musicTimeMS - from.musicTimeMS) * t;
    result.levelTime = from.levelTime + (to.levelTime - from.levelTime) * t;
    
    // Interpolate camera
    result.cameraPosition = from.cameraPosition + (to.cameraPosition - from.cameraPosition) * t;
    result.cameraZoom = from.cameraZoom + (to.cameraZoom - from.cameraZoom) * t;
    result.cameraAngle = from.cameraAngle + (to.cameraAngle - from.cameraAngle) * t;
    
    return result;
}

PlayerFrameState TimeRewindManager::interpolatePlayerStates(const PlayerFrameState& from,
                                                             const PlayerFrameState& to, float t) {
    PlayerFrameState result = from;
    
    // Interpolate continuous values
    result.position = from.position + (to.position - from.position) * t;
    result.rotation = from.rotation + (to.rotation - from.rotation) * t;
    result.yVelocity = from.yVelocity + (to.yVelocity - from.yVelocity) * t;
    result.xVelocity = from.xVelocity + (to.xVelocity - from.xVelocity) * t;
    result.scaleX = from.scaleX + (to.scaleX - from.scaleX) * t;
    result.scaleY = from.scaleY + (to.scaleY - from.scaleY) * t;
    
    // Use nearest neighbor for booleans (snap at 0.5)
    if (t > 0.5f) {
        result.isUpsideDown = to.isUpsideDown;
        result.isOnGround = to.isOnGround;
        result.isShip = to.isShip;
        result.isBall = to.isBall;
        result.isUFO = to.isUFO;
        result.isWave = to.isWave;
        result.isRobot = to.isRobot;
        result.isSpider = to.isSpider;
        result.isSwing = to.isSwing;
        result.isCube = to.isCube;
    }
    
    // Always keep player alive during interpolation
    result.isDead = false;
    result.isVisible = true;
    
    return result;
}

// ============================================================
// Audio Sync
// ============================================================

void TimeRewindManager::syncMusicTime(double timeMS) {
    auto fmod = FMODAudioEngine::sharedEngine();
    if (fmod) {
        // Set music time for main game music (channel 0)
        fmod->setMusicTimeMS(static_cast<unsigned int>(std::max(0.0, timeMS)), true, 0);
    }
}

void TimeRewindManager::setMusicPaused(bool paused) {
    auto fmod = FMODAudioEngine::sharedEngine();
    if (fmod) {
        if (paused) {
            fmod->pauseMusic(0);
        } else {
            fmod->resumeMusic(0);
        }
    }
}

// ============================================================
// Charges
// ============================================================

void TimeRewindManager::setMaxCharges(int charges) {
    m_maxRewindCharges = std::max(1, charges);
    m_rewindCharges = std::min(m_rewindCharges, m_maxRewindCharges);
}

void TimeRewindManager::resetCharges() {
    m_rewindCharges = m_maxRewindCharges;
}

void TimeRewindManager::useCharge() {
    if (m_rewindCharges > 0) {
        m_rewindCharges--;
    }
}

void TimeRewindManager::addCharge() {
    if (m_rewindCharges < m_maxRewindCharges) {
        m_rewindCharges++;
    }
}

// ============================================================
// Utility
// ============================================================

float TimeRewindManager::getBufferDurationSeconds() const {
    return static_cast<float>(m_frameBuffer.size()) * m_recordInterval;
}

// ============================================================
// Settings
// ============================================================

void TimeRewindManager::loadSettings() {
    auto mod = Mod::get();
    
    m_enabled = mod->getSettingValue<bool>("enabled");
    m_maxRewindCharges = mod->getSettingValue<int64_t>("max-charges");
    m_rewindCharges = m_maxRewindCharges;
    m_rewindDurationSec = static_cast<float>(mod->getSettingValue<double>("rewind-duration"));
    m_rewindSpeed = static_cast<float>(mod->getSettingValue<double>("rewind-speed"));
    m_useVisualEffects = mod->getSettingValue<bool>("visual-effects");
    m_useAudioSync = mod->getSettingValue<bool>("audio-sync");
    
    // Calculate max frame count based on rewind duration + buffer
    m_maxFrameCount = static_cast<size_t>((m_rewindDurationSec + 5.0f) / m_recordInterval);
    
    log::info("[TimeRewind] Settings loaded - Charges: {}, Duration: {:.1f}s, Speed: {:.1f}x",
              m_maxRewindCharges, m_rewindDurationSec, m_rewindSpeed);
}

void TimeRewindManager::saveSettings() {
    auto mod = Mod::get();
    
    mod->setSettingValue<bool>("enabled", m_enabled);
    mod->setSettingValue<int64_t>("max-charges", m_maxRewindCharges);
    mod->setSettingValue<double>("rewind-duration", m_rewindDurationSec);
    mod->setSettingValue<double>("rewind-speed", m_rewindSpeed);
    mod->setSettingValue<bool>("visual-effects", m_useVisualEffects);
    mod->setSettingValue<bool>("audio-sync", m_useAudioSync);
}
