#pragma once

#include <Geode/Geode.hpp>
#include "FrameState.hpp"

using namespace geode::prelude;

enum class RewindState {
    Inactive,
    Recording,
    Rewinding,
    Resuming
};

class RewindVisuals;

class TimeRewindManager {
private:
    TimeRewindManager() = default;
    
public:
    static TimeRewindManager* get();
    
    RewindState m_state = RewindState::Inactive;
    FrameBuffer m_frameBuffer;
    
    bool m_enabled = true;
    float m_rewindDuration = 2.0f;
    float m_rewindSpeed = 2.0f;
    int m_maxRewindsPerAttempt = 3;
    float m_recordFPS = 60.0f;
    bool m_visualEffects = true;
    bool m_soundEffects = true;
    bool m_infiniteRewinds = false;
    
    int m_rewindsRemaining = 3;
    size_t m_rewindFrameIndex = 0;
    float m_rewindTimer = 0.0f;
    float m_rewindFrameInterval = 0.0f;
    std::vector<FrameState> m_rewindFrames;
    
    float m_recordTimer = 0.0f;
    float m_recordInterval = 0.0f;
    
    CCNode* m_overlayNode = nullptr;
    CCLabelBMFont* m_rewindLabel = nullptr;
    CCLabelBMFont* m_rewindsLeftLabel = nullptr;
    
    PlayLayer* m_playLayer = nullptr;
    PlayerObject* m_player1 = nullptr;
    PlayerObject* m_player2 = nullptr;
    
    void loadSettings();
    void initialize(PlayLayer* playLayer);
    void cleanup();
    void reset();
    void update(float dt);
    void recordFrame(float dt);
    bool canRewind() const;
    void startRewind();
    void updateRewind(float dt);
    void finishRewind();
    void cancelRewind();
    void createVisuals();
    void cleanupVisuals();
    void playRewindSound();
    void stopRewindSound();
    
    bool isRewinding() const { return m_state == RewindState::Rewinding; }
    bool isActive() const { return m_state != RewindState::Inactive; }
};
