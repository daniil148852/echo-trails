#pragma once

#include <Geode/Geode.hpp>
#include "FrameState.hpp"

using namespace geode::prelude;

enum class RewindState {
    Inactive,       // Обычная игра
    Recording,      // Запись кадров
    Rewinding,      // Воспроизведение отмотки
    Resuming        // Переход обратно к игре
};

class TimeRewindManager {
private:
    TimeRewindManager() = default;
    
public:
    static TimeRewindManager* get();
    
    // === Состояние ===
    RewindState m_state = RewindState::Inactive;
    FrameBuffer m_frameBuffer;
    
    // === Настройки (кэш) ===
    bool m_enabled = true;
    float m_rewindDuration = 2.0f;          // Секунд отмотки
    float m_rewindSpeed = 2.0f;             // Скорость воспроизведения
    int m_maxRewindsPerAttempt = 3;         // Лимит отмоток
    float m_recordFPS = 60.0f;              // FPS записи
    bool m_visualEffects = true;
    bool m_soundEffects = true;
    bool m_infiniteRewinds = false;
    
    // === Состояние отмотки ===
    int m_rewindsRemaining = 3;
    size_t m_rewindFrameIndex = 0;
    float m_rewindTimer = 0.0f;
    float m_rewindFrameInterval = 0.0f;
    std::vector<FrameState> m_rewindFrames;
    
    // === Таймеры ===
    float m_recordTimer = 0.0f;
    float m_recordInterval = 0.0f;
    
    // === Визуальные элементы ===
    CCNode* m_overlayNode = nullptr;
    CCSprite* m_vhsOverlay = nullptr;
    CCLabelBMFont* m_rewindLabel = nullptr;
    CCLabelBMFont* m_rewindsLeftLabel = nullptr;
    
    // === Ссылки ===
    PlayLayer* m_playLayer = nullptr;
    PlayerObject* m_player1 = nullptr;
    PlayerObject* m_player2 = nullptr;
    
    // === Методы ===
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
    void updateVisuals(float progress);
    void cleanupVisuals();
    
    void playRewindSound();
    void stopRewindSound();
    
    bool isRewinding() const { return m_state == RewindState::Rewinding; }
    bool isActive() const { return m_state != RewindState::Inactive; }
};
