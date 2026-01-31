#pragma once

#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <deque>
#include <vector>
#include <functional>

using namespace geode::prelude;

namespace TimeRewind {

    // Complete player state for a single frame
    struct PlayerFrameState {
        // Position and physics
        CCPoint position;
        float xVelocity;
        float yVelocity;
        double rotation;
        
        // State flags
        bool isUpsideDown;
        bool isOnGround;
        bool isHolding;
        bool isDashing;
        bool isSliding;
        bool isRising;
        
        // Gamemode state
        int gamemode;  // 0=cube, 1=ship, 2=ball, 3=ufo, 4=wave, 5=robot, 6=spider, 7=swing
        bool isMini;
        float playerSpeed;
        
        // Visual state
        float iconScale;
        float iconOpacity;
        bool isVisible;
        
        // Animation state
        int currentFrame;
        bool flipX;
        bool flipY;
    };
    
    // Complete frame state including both players and game state
    struct FrameState {
        // Timestamps
        double gameTime;           // m_gameState.m_currentProgress or time
        float levelTime;           // Time in level
        int frameNumber;
        
        // Player states
        PlayerFrameState player1;
        PlayerFrameState player2;
        bool isDualMode;
        
        // Camera state
        CCPoint cameraPosition;
        float cameraZoom;
        float cameraRotation;
        
        // Music/Audio state
        unsigned int musicTimeMS;
        
        // Level state
        float levelProgress;
        bool isPlatformer;
        
        // Checkpoint data (for platformer)
        CCPoint lastCheckpointPos;
    };
    
    // Rewind state enum
    enum class RewindState {
        Idle,           // Normal gameplay
        Recording,      // Recording states (always on during play)
        Rewinding,      // Playing back rewind animation
        Resuming        // Brief transition back to gameplay
    };

    class TimeRewindManager {
    private:
        static TimeRewindManager* s_instance;
        
        // State buffer
        std::deque<FrameState> m_frameBuffer;
        size_t m_maxBufferSize;
        
        // Rewind state
        RewindState m_state;
        int m_rewindCharges;
        int m_maxCharges;
        
        // Timing
        float m_recordInterval;
        float m_timeSinceLastRecord;
        float m_rewindDuration;
        float m_rewindSpeed;
        float m_currentRewindTime;
        size_t m_rewindFrameIndex;
        
        // Settings
        int m_recordFPS;
        bool m_vhsEffectEnabled;
        bool m_grayscaleEnabled;
        
        // References
        PlayLayer* m_playLayer;
        bool m_initialized;
        
        // Private constructor for singleton
        TimeRewindManager();
        
        // Helper methods
        PlayerFrameState capturePlayerState(PlayerObject* player);
        void restorePlayerState(PlayerObject* player, const PlayerFrameState& state);
        void interpolateStates(const FrameState& from, const FrameState& to, float t, FrameState& result);
        float easeInOutCubic(float t);
        
    public:
        // Singleton access
        static TimeRewindManager* get();
        static void destroy();
        
        // Lifecycle
        void init(PlayLayer* playLayer);
        void reset();
        void loadSettings();
        
        // Recording
        void recordFrame(float dt);
        void clearBuffer();
        
        // Rewind control
        bool canRewind() const;
        bool startRewind();
        void updateRewind(float dt);
        void finishRewind();
        void cancelRewind();
        
        // State queries
        RewindState getState() const { return m_state; }
        bool isRewinding() const { return m_state == RewindState::Rewinding; }
        bool isRecording() const { return m_state == RewindState::Recording || m_state == RewindState::Idle; }
        int getCharges() const { return m_rewindCharges; }
        int getMaxCharges() const { return m_maxCharges; }
        float getRewindProgress() const;
        size_t getBufferSize() const { return m_frameBuffer.size(); }
        
        // Settings access
        bool isVHSEffectEnabled() const { return m_vhsEffectEnabled; }
        bool isGrayscaleEnabled() const { return m_grayscaleEnabled; }
        
        // Callbacks
        std::function<void()> onRewindStart;
        std::function<void()> onRewindEnd;
        std::function<void(float)> onRewindProgress;
    };

} // namespace TimeRewind
