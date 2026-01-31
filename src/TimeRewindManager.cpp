#include "TimeRewindManager.hpp"
#include "RewindVisuals.hpp"
#include <Geode/binding/FMODAudioEngine.hpp>
#include <Geode/binding/GJBaseGameLayer.hpp>

namespace TimeRewind {

    TimeRewindManager* TimeRewindManager::s_instance = nullptr;

    TimeRewindManager::TimeRewindManager()
        : m_state(RewindState::Idle)
        , m_rewindCharges(3)
        , m_maxCharges(3)
        , m_recordInterval(1.0f / 60.0f)
        , m_timeSinceLastRecord(0.0f)
        , m_rewindDuration(2.0f)
        , m_rewindSpeed(2.0f)
        , m_currentRewindTime(0.0f)
        , m_rewindFrameIndex(0)
        , m_recordFPS(60)
        , m_vhsEffectEnabled(true)
        , m_grayscaleEnabled(true)
        , m_playLayer(nullptr)
        , m_initialized(false)
        , m_maxBufferSize(600)
    {}

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

    void TimeRewindManager::loadSettings() {
        m_maxCharges = Mod::get()->getSettingValue<int64_t>("rewind-charges");
        m_rewindCharges = m_maxCharges;
        m_rewindDuration = Mod::get()->getSettingValue<double>("rewind-duration");
        m_rewindSpeed = Mod::get()->getSettingValue<double>("rewind-speed");
        m_recordFPS = Mod::get()->getSettingValue<int64_t>("record-fps");
        m_vhsEffectEnabled = Mod::get()->getSettingValue<bool>("vhs-effect");
        m_grayscaleEnabled = Mod::get()->getSettingValue<bool>("grayscale-effect");
        
        m_recordInterval = 1.0f / static_cast<float>(m_recordFPS);
        m_maxBufferSize = static_cast<size_t>(m_rewindDuration * m_recordFPS * 2);
        
        log::info("TimeRewind settings loaded: charges={}, duration={}, speed={}, fps={}", 
            m_maxCharges, m_rewindDuration, m_rewindSpeed, m_recordFPS);
    }

    void TimeRewindManager::init(PlayLayer* playLayer) {
        m_playLayer = playLayer;
        loadSettings();
        reset();
        m_initialized = true;
        m_state = RewindState::Recording;
        
        log::info("TimeRewindManager initialized for level");
    }

    void TimeRewindManager::reset() {
        clearBuffer();
        m_rewindCharges = m_maxCharges;
        m_state = RewindState::Idle;
        m_timeSinceLastRecord = 0.0f;
        m_currentRewindTime = 0.0f;
        m_rewindFrameIndex = 0;
        
        log::debug("TimeRewindManager reset");
    }

    void TimeRewindManager::clearBuffer() {
        m_frameBuffer.clear();
        log::debug("Frame buffer cleared");
    }

    PlayerFrameState TimeRewindManager::capturePlayerState(PlayerObject* player) {
        PlayerFrameState state;
        
        if (!player) {
            return state;
        }
        
        state.position = player->getPosition();
        state.xVelocity = player->m_xVelocity;
        state.yVelocity = player->m_yVelocity;
        state.rotation = player->getRotation();
        
        state.isUpsideDown = player->m_isUpsideDown;
        state.isOnGround = player->m_isOnGround;
        state.isHolding = player->m_isHolding;
        state.isDashing = player->m_isDashing;
        state.isSliding = player->m_isSliding;
        state.isRising = player->m_isRising;
        
        if (player->m_isShip) state.gamemode = 1;
        else if (player->m_isBall) state.gamemode = 2;
        else if (player->m_isBird) state.gamemode = 3;
        else if (player->m_isDart) state.gamemode = 4;
        else if (player->m_isRobot) state.gamemode = 5;
        else if (player->m_isSpider) state.gamemode = 6;
        else if (player->m_isSwing) state.gamemode = 7;
        else state.gamemode = 0;
        
        state.isMini = player->m_vehicleSize < 1.0f;
        state.playerSpeed = player->m_playerSpeed;
        
        state.iconScale = player->getScale();
        state.iconOpacity = player->getOpacity();
        state.isVisible = player->isVisible();
        
        state.flipX = player->isFlipX();
        state.flipY = player->isFlipY();
        
        return state;
    }

    void TimeRewindManager::restorePlayerState(PlayerObject* player, const PlayerFrameState& state) {
        if (!player) return;
        
        player->setPosition(state.position);
        player->m_xVelocity = state.xVelocity;
        player->m_yVelocity = state.yVelocity;
        player->setRotation(state.rotation);
        
        player->m_isUpsideDown = state.isUpsideDown;
        player->m_isOnGround = state.isOnGround;
        player->m_isHolding = state.isHolding;
        player->m_isDashing = state.isDashing;
        player->m_isSliding = state.isSliding;
        player->m_isRising = state.isRising;
        
        player->setScale(state.iconScale);
        player->setOpacity(static_cast<unsigned char>(state.iconOpacity));
        player->setVisible(state.isVisible);
        
        player->setFlipX(state.flipX);
        player->setFlipY(state.flipY);
    }

    void TimeRewindManager::recordFrame(float dt) {
        if (!m_initialized || !m_playLayer || isRewinding()) {
            return;
        }
        
        m_timeSinceLastRecord += dt;
        
        if (m_timeSinceLastRecord < m_recordInterval) {
            return;
        }
        
        m_timeSinceLastRecord = 0.0f;
        
        auto player1 = m_playLayer->m_player1;
        auto player2 = m_playLayer->m_player2;
        
        if (!player1) return;
        
        if (player1->m_isDead) return;
        
        FrameState frame;
        
        frame.gameTime = m_playLayer->m_gameState.m_currentProgress;
        frame.levelTime = m_playLayer->m_totalTime;
        frame.frameNumber = static_cast<int>(m_frameBuffer.size());
        
        frame.player1 = capturePlayerState(player1);
        frame.player2 = capturePlayerState(player2);
        frame.isDualMode = m_playLayer->m_gameState.m_isDualMode;
        
        frame.cameraPosition = m_playLayer->m_gameState.m_cameraPosition;
        frame.cameraZoom = m_playLayer->m_gameState.m_cameraZoom;
        frame.cameraRotation = m_playLayer->m_gameState.m_cameraAngle;
        
        auto audioEngine = FMODAudioEngine::sharedEngine();
        frame.musicTimeMS = static_cast<unsigned int>(audioEngine->getMusicTimeMS());
        
        frame.levelProgress = m_playLayer->getCurrentPercent();
        frame.isPlatformer = m_playLayer->m_isPlatformer;
        
        m_frameBuffer.push_back(frame);
        
        while (m_frameBuffer.size() > m_maxBufferSize) {
            m_frameBuffer.pop_front();
        }
    }

    bool TimeRewindManager::canRewind() const {
        return m_rewindCharges > 0 && 
               m_frameBuffer.size() > 10 && 
               m_state != RewindState::Rewinding &&
               m_initialized;
    }

    bool TimeRewindManager::startRewind() {
        if (!canRewind()) {
            log::warn("Cannot start rewind: charges={}, bufferSize={}, state={}", 
                m_rewindCharges, m_frameBuffer.size(), static_cast<int>(m_state));
            return false;
        }
        
        log::info("Starting rewind with {} frames in buffer", m_frameBuffer.size());
        
        m_rewindCharges--;
        m_state = RewindState::Rewinding;
        m_currentRewindTime = 0.0f;
        m_rewindFrameIndex = m_frameBuffer.size() - 1;
        
        if (m_playLayer) {
            if (m_playLayer->m_player1) {
                m_playLayer->m_player1->m_isDead = false;
            }
            if (m_playLayer->m_player2) {
                m_playLayer->m_player2->m_isDead = false;
            }
        }
        
        if (onRewindStart) {
            onRewindStart();
        }
        
        return true;
    }

    float TimeRewindManager::easeInOutCubic(float t) {
        return t < 0.5f ? 4.0f * t * t * t : 1.0f - std::pow(-2.0f * t + 2.0f, 3.0f) / 2.0f;
    }

    void TimeRewindManager::interpolateStates(const FrameState& from, const FrameState& to, float t, FrameState& result) {
        result.player1.position = ccpLerp(from.player1.position, to.player1.position, t);
        result.player1.rotation = from.player1.rotation + (to.player1.rotation - from.player1.rotation) * t;
        result.player1.xVelocity = from.player1.xVelocity + (to.player1.xVelocity - from.player1.xVelocity) * t;
        result.player1.yVelocity = from.player1.yVelocity + (to.player1.yVelocity - from.player1.yVelocity) * t;
        
        result.player1.isUpsideDown = to.player1.isUpsideDown;
        result.player1.isOnGround = to.player1.isOnGround;
        result.player1.gamemode = to.player1.gamemode;
        result.player1.isMini = to.player1.isMini;
        result.player1.flipX = to.player1.flipX;
        result.player1.flipY = to.player1.flipY;
        
        if (from.isDualMode) {
            result.player2.position = ccpLerp(from.player2.position, to.player2.position, t);
            result.player2.rotation = from.player2.rotation + (to.player2.rotation - from.player2.rotation) * t;
            result.player2.xVelocity = from.player2.xVelocity + (to.player2.xVelocity - from.player2.xVelocity) * t;
            result.player2.yVelocity = from.player2.yVelocity + (to.player2.yVelocity - from.player2.yVelocity) * t;
            result.player2.isUpsideDown = to.player2.isUpsideDown;
            result.player2.isOnGround = to.player2.isOnGround;
        }
        
        result.cameraPosition = ccpLerp(from.cameraPosition, to.cameraPosition, t);
        result.cameraZoom = from.cameraZoom + (to.cameraZoom - from.cameraZoom) * t;
        result.cameraRotation = from.cameraRotation + (to.cameraRotation - from.cameraRotation) * t;
        
        result.musicTimeMS = static_cast<unsigned int>(
            from.musicTimeMS + (static_cast<float>(to.musicTimeMS) - static_cast<float>(from.musicTimeMS)) * t
        );
        
        result.isDualMode = from.isDualMode;
        result.isPlatformer = from.isPlatformer;
    }

    void TimeRewindManager::updateRewind(float dt) {
        if (m_state != RewindState::Rewinding || !m_playLayer || m_frameBuffer.empty()) {
            return;
        }
        
        size_t framesToRewind = static_cast<size_t>(m_rewindDuration * m_recordFPS);
        if (framesToRewind >= m_frameBuffer.size()) {
            framesToRewind = m_frameBuffer.size() - 1;
        }
        
        float animationDuration = m_rewindDuration / m_rewindSpeed;
        m_currentRewindTime += dt;
        
        float progress = m_currentRewindTime / animationDuration;
        progress = std::min(progress, 1.0f);
        
        float easedProgress = easeInOutCubic(progress);
        
        size_t targetFrameOffset = static_cast<size_t>(easedProgress * framesToRewind);
        size_t currentFrameIndex = m_frameBuffer.size() - 1 - targetFrameOffset;
        
        currentFrameIndex = std::max(currentFrameIndex, size_t(0));
        currentFrameIndex = std::min(currentFrameIndex, m_frameBuffer.size() - 1);
        
        size_t nextFrameIndex = (currentFrameIndex > 0) ? currentFrameIndex - 1 : 0;
        
        const FrameState& currentFrame = m_frameBuffer[currentFrameIndex];
        const FrameState& nextFrame = m_frameBuffer[nextFrameIndex];
        
        float frameProgress = std::fmod(easedProgress * framesToRewind, 1.0f);
        
        FrameState interpolatedFrame;
        interpolateStates(currentFrame, nextFrame, frameProgress, interpolatedFrame);
        
        restorePlayerState(m_playLayer->m_player1, interpolatedFrame.player1);
        
        if (interpolatedFrame.isDualMode && m_playLayer->m_player2) {
            restorePlayerState(m_playLayer->m_player2, interpolatedFrame.player2);
        }
        
        auto audioEngine = FMODAudioEngine::sharedEngine();
        audioEngine->setMusicTimeMS(interpolatedFrame.musicTimeMS);
        
        if (onRewindProgress) {
            onRewindProgress(progress);
        }
        
        if (progress >= 1.0f) {
            finishRewind();
        }
    }

    void TimeRewindManager::finishRewind() {
        if (m_state != RewindState::Rewinding) {
            return;
        }
        
        log::info("Finishing rewind");
        
        m_state = RewindState::Resuming;
        
        size_t framesToRewind = static_cast<size_t>(m_rewindDuration * m_recordFPS);
        if (framesToRewind >= m_frameBuffer.size()) {
            framesToRewind = m_frameBuffer.size() - 1;
        }
        
        size_t targetIndex = m_frameBuffer.size() - 1 - framesToRewind;
        targetIndex = std::max(targetIndex, size_t(0));
        
        const FrameState& targetFrame = m_frameBuffer[targetIndex];
        
        if (m_playLayer && m_playLayer->m_player1) {
            restorePlayerState(m_playLayer->m_player1, targetFrame.player1);
            m_playLayer->m_player1->m_isDead = false;
            
            if (targetFrame.isDualMode && m_playLayer->m_player2) {
                restorePlayerState(m_playLayer->m_player2, targetFrame.player2);
                m_playLayer->m_player2->m_isDead = false;
            }
        }
        
        auto audioEngine = FMODAudioEngine::sharedEngine();
        audioEngine->setMusicTimeMS(targetFrame.musicTimeMS);
        
        while (m_frameBuffer.size() > targetIndex + 1) {
            m_frameBuffer.pop_back();
        }
        
        if (onRewindEnd) {
            onRewindEnd();
        }
        
        m_state = RewindState::Recording;
        m_currentRewindTime = 0.0f;
    }

    void TimeRewindManager::cancelRewind() {
        if (m_state == RewindState::Rewinding) {
            m_state = RewindState::Recording;
            m_currentRewindTime = 0.0f;
            m_rewindCharges++;
            
            if (onRewindEnd) {
                onRewindEnd();
            }
        }
    }

    float TimeRewindManager::getRewindProgress() const {
        if (m_state != RewindState::Rewinding) {
            return 0.0f;
        }
        
        float animationDuration = m_rewindDuration / m_rewindSpeed;
        return std::min(m_currentRewindTime / animationDuration, 1.0f);
    }

}
