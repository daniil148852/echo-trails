#pragma once

#include "GhostData.hpp"
#include "GhostSprite.hpp"

namespace EchoTrails {

class GhostPlayer {
public:
    static GhostPlayer& get();
    
    bool loadGhostForLevel(const std::string& levelID);
    void unloadGhost();
    
    void createSprite(cocos2d::CCNode* parent);
    void removeSprite();
    
    void update(float time);
    void reset();
    
    bool hasGhost() const { return m_hasGhost; }
    bool isPlaying() const { return m_isPlaying; }
    
    void start();
    void stop();
    void pause();
    void resume();
    
    const GhostRecording* getRecording() const { 
        return m_hasGhost ? &m_recording : nullptr; 
    }
    
private:
    GhostPlayer() = default;
    
    GhostFrame getInterpolatedFrame(float time);
    std::filesystem::path getRecordingPath(const std::string& levelID);
    
    bool m_hasGhost = false;
    bool m_isPlaying = false;
    bool m_isPaused = false;
    
    GhostRecording m_recording;
    GhostSprite* m_sprite = nullptr;
    GhostSprite* m_player2Sprite = nullptr;
    
    float m_currentTime = 0.0f;
    size_t m_currentFrameIndex = 0;
};

} // namespace EchoTrails
