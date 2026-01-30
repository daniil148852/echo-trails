#pragma once

#include "GhostData.hpp"
#include <Geode/modify/PlayLayer.hpp>

namespace EchoTrails {

class GhostRecorder {
public:
    static GhostRecorder& get();
    
    void startRecording(GJGameLevel* level);
    void recordFrame(PlayerObject* player, bool isPlayer2 = false);
    void stopRecording();
    void reset();
    
    bool isRecording() const { return m_isRecording; }
    const GhostRecording& getCurrentRecording() const { return m_currentRecording; }
    
    int getCurrentPercent() const { return m_currentPercent; }
    void setCurrentPercent(int percent) { m_currentPercent = percent; }
    
    // Сохранение и загрузка
    bool saveCurrentRecording();
    bool shouldSaveRecording() const;
    
    // Сделаем публичным для доступа извне
    std::filesystem::path getRecordingPath(const std::string& levelID) const;
    
private:
    GhostRecorder() = default;
    
    GhostFrame capturePlayerState(PlayerObject* player, bool isPlayer2);
    GameMode getPlayerGameMode(PlayerObject* player);
    
    bool m_isRecording = false;
    GhostRecording m_currentRecording;
    int m_currentPercent = 0;
    float m_recordingStartTime = 0.0f;
    
    // Для оптимизации - не записывать каждый кадр если ничего не изменилось
    GhostFrame m_lastFrame;
    bool m_hasLastFrame = false;
};

} // namespace EchoTrails
