#include "TimeRewindManager.hpp"
#include "RewindVisuals.hpp"

TimeRewindManager* TimeRewindManager::get() {
    static TimeRewindManager instance;
    return &instance;
}

void TimeRewindManager::loadSettings() {
    auto* mod = Mod::get();
    
    m_enabled = mod->getSettingValue<bool>("enabled");
    m_rewindDuration = static_cast<float>(mod->getSettingValue<double>("rewind-duration"));
    m_rewindSpeed = static_cast<float>(mod->getSettingValue<double>("rewind-speed"));
    m_maxRewindsPerAttempt = static_cast<int>(mod->getSettingValue<int64_t>("max-rewinds"));
    m_recordFPS = static_cast<float>(mod->getSettingValue<int64_t>("record-fps"));
    m_visualEffects = mod->getSettingValue<bool>("visual-effects");
    m_soundEffects = mod->getSettingValue<bool>("sound-effects");
    m_infiniteRewinds = mod->getSettingValue<bool>("infinite-rewinds");
    
    m_recordInterval = 1.0f / m_recordFPS;
    
    size_t bufferSize = static_cast<size_t>(m_recordFPS * m_rewindDuration * 1.5f);
    m_frameBuffer.resize(std::max(bufferSize, size_t(60)));
}

void TimeRewindManager::initialize(PlayLayer* playLayer) {
    m_playLayer = playLayer;
    
    if (!playLayer) {
        log::error("TimeRewind: PlayLayer is null");
        return;
    }
    
    m_player1 = playLayer->m_player1;
    m_player2 = playLayer->m_player2;
    
    loadSettings();
    reset();
    
    if (m_enabled) {
        m_state = RewindState::Recording;
        createVisuals();
        log::info("TimeRewind initialized: {}fps, {}s buffer, {} rewinds",
                  m_recordFPS, m_rewindDuration, m_maxRewindsPerAttempt);
    }
}

void TimeRewindManager::cleanup() {
    cleanupVisuals();
    m_frameBuffer.clear();
    m_rewindFrames.clear();
    m_state = RewindState::Inactive;
    m_playLayer = nullptr;
    m_player1 = nullptr;
    m_player2 = nullptr;
}

void TimeRewindManager::reset() {
    m_frameBuffer.clear();
    m_rewindFrames.clear();
    m_rewindFrameIndex = 0;
    m_rewindTimer = 0.0f;
    m_recordTimer = 0.0f;
    
    if (m_infiniteRewinds) {
        m_rewindsRemaining = 999;
    } else {
        m_rewindsRemaining = m_maxRewindsPerAttempt;
    }
    
    if (m_enabled) {
        m_state = RewindState::Recording;
    }
    
    if (m_rewindsLeftLabel) {
        std::string text = m_infiniteRewinds ? 
            "INF" : fmt::format("{}", m_rewindsRemaining);
        m_rewindsLeftLabel->setString(text.c_str());
    }
}

void TimeRewindManager::update(float dt) {
    if (!m_enabled || !m_playLayer) return;
    
    switch (m_state) {
        case RewindState::Recording:
            recordFrame(dt);
            break;
            
        case RewindState::Rewinding:
            updateRewind(dt);
            break;
            
        case RewindState::Resuming:
            finishRewind();
            break;
            
        default:
            break;
    }
}

void TimeRewindManager::recordFrame(float dt) {
    if (!m_player1) return;
    
    m_recordTimer += dt;
    
    if (m_recordTimer < m_recordInterval) return;
    m_recordTimer -= m_recordInterval;
    
    FrameState frame{};
    frame.timestamp = m_playLayer->m_gameState.m_levelTime;
    frame.deltaTime = dt;
    
    // Захватываем состояние игрока 1
    frame.capturePlayer(m_player1, frame.player1);
    
    // Захватываем игрока 2 если есть
    frame.hasDualPlayer = m_player2 != nullptr && m_player2->isVisible();
    if (frame.hasDualPlayer) {
        frame.capturePlayer(m_player2, frame.player2);
    }
    
    // Сохраняем
    m_frameBuffer.push(frame);
}

bool TimeRewindManager::canRewind() const {
    if (!m_enabled) return false;
    if (m_state == RewindState::Rewinding) return false;
    if (m_frameBuffer.empty()) return false;
    if (m_rewindsRemaining <= 0 && !m_infiniteRewinds) return false;
    
    return true;
}

void TimeRewindManager::startRewind() {
    if (!canRewind()) {
        log::info("TimeRewind: Cannot rewind (remaining: {}, frames: {})",
                  m_rewindsRemaining, m_frameBuffer.size());
        return;
    }
    
    log::info("TimeRewind: Starting rewind with {} frames", m_frameBuffer.size());
    
    if (!m_infiniteRewinds) {
        m_rewindsRemaining--;
    }
    
    size_t framesToRewind = static_cast<size_t>(m_rewindDuration * m_recordFPS);
    m_rewindFrames = m_frameBuffer.getRewindFrames(framesToRewind);
    
    if (m_rewindFrames.empty()) {
        log::warn("TimeRewind: No frames to rewind");
        return;
    }
    
    m_rewindFrameIndex = 0;
    m_rewindTimer = 0.0f;
    m_rewindFrameInterval = m_recordInterval / m_rewindSpeed;
    
    m_state = RewindState::Rewinding;
    
    if (m_player1) {
        m_player1->m_isDead = false;
        m_player1->setVisible(true);
    }
    
    auto* fmod = FMODAudioEngine::sharedEngine();
    if (fmod) {
        fmod->pauseAllMusic(true);
    }
    
    if (m_visualEffects) {
        RewindVisuals::get()->startRewindEffect(m_playLayer);
    }
    
    if (m_soundEffects) {
        playRewindSound();
    }
    
    if (m_rewindLabel) {
        m_rewindLabel->setVisible(true);
    }
    
    if (m_rewindsLeftLabel) {
        std::string text = m_infiniteRewinds ? 
            "INF" : fmt::format("{}", m_rewindsRemaining);
        m_rewindsLeftLabel->setString(text.c_str());
    }
}

void TimeRewindManager::updateRewind(float dt) {
    if (m_rewindFrames.empty()) {
        finishRewind();
        return;
    }
    
    m_rewindTimer += dt;
    
    while (m_rewindTimer >= m_rewindFrameInterval && 
           m_rewindFrameIndex < m_rewindFrames.size()) {
        
        m_rewindTimer -= m_rewindFrameInterval;
        
        const FrameState& frame = m_rewindFrames[m_rewindFrameIndex];
        
        if (m_player1) {
            frame.applyToPlayer(m_player1, frame.player1);
        }
        
        if (frame.hasDualPlayer && m_player2) {
            frame.applyToPlayer(m_player2, frame.player2);
        }
        
        m_rewindFrameIndex++;
    }
    
    float progress = static_cast<float>(m_rewindFrameIndex) / m_rewindFrames.size();
    
    if (m_visualEffects) {
        RewindVisuals::get()->updateEffect(progress);
    }
    
    if (m_rewindLabel) {
        float timeLeft = (m_rewindFrames.size() - m_rewindFrameIndex) * m_recordInterval;
        std::string text = fmt::format("-{:.1f}s", timeLeft);
        m_rewindLabel->setString(text.c_str());
    }
    
    if (m_rewindFrameIndex >= m_rewindFrames.size()) {
        m_state = RewindState::Resuming;
    }
}

void TimeRewindManager::finishRewind() {
    log::info("TimeRewind: Rewind finished");
    
    m_state = RewindState::Recording;
    m_rewindFrames.clear();
    m_frameBuffer.clear();
    
    auto* fmod = FMODAudioEngine::sharedEngine();
    if (fmod) {
        fmod->resumeAllMusic();
    }
    
    if (m_visualEffects) {
        RewindVisuals::get()->stopRewindEffect();
    }
    
    if (m_soundEffects) {
        stopRewindSound();
    }
    
    if (m_rewindLabel) {
        m_rewindLabel->setVisible(false);
    }
}

void TimeRewindManager::cancelRewind() {
    if (m_state != RewindState::Rewinding) return;
    
    m_state = RewindState::Recording;
    m_rewindFrames.clear();
    
    if (m_visualEffects) {
        RewindVisuals::get()->stopRewindEffect();
    }
    
    stopRewindSound();
    
    auto* fmod = FMODAudioEngine::sharedEngine();
    if (fmod) {
        fmod->resumeAllMusic();
    }
}

void TimeRewindManager::createVisuals() {
    if (!m_playLayer) return;
    
    auto winSize = CCDirector::sharedDirector()->getWinSize();
    
    m_overlayNode = CCNode::create();
    m_overlayNode->setPosition(CCPointZero);
    m_playLayer->addChild(m_overlayNode, 10000);
    
    std::string rewindsText = m_infiniteRewinds ? 
        "INF" : fmt::format("{}", m_rewindsRemaining);
    
    m_rewindsLeftLabel = CCLabelBMFont::create(rewindsText.c_str(), "bigFont.fnt");
    m_rewindsLeftLabel->setPosition(ccp(winSize.width - 30.0f, winSize.height - 20.0f));
    m_rewindsLeftLabel->setScale(0.4f);
    m_rewindsLeftLabel->setOpacity(180);
    m_rewindsLeftLabel->setColor(ccc3(100, 200, 255));
    m_overlayNode->addChild(m_rewindsLeftLabel);
    
    auto* rewindIcon = CCSprite::createWithSpriteFrameName("GJ_timeIcon_001.png");
    if (rewindIcon) {
        rewindIcon->setPosition(ccp(winSize.width - 55.0f, winSize.height - 20.0f));
        rewindIcon->setScale(0.5f);
        rewindIcon->setOpacity(180);
        m_overlayNode->addChild(rewindIcon);
    }
    
    m_rewindLabel = CCLabelBMFont::create("", "bigFont.fnt");
    m_rewindLabel->setPosition(ccp(winSize.width / 2, winSize.height - 30.0f));
    m_rewindLabel->setScale(0.6f);
    m_rewindLabel->setColor(ccc3(255, 100, 100));
    m_rewindLabel->setVisible(false);
    m_overlayNode->addChild(m_rewindLabel);
}

void TimeRewindManager::cleanupVisuals() {
    if (m_overlayNode) {
        m_overlayNode->removeFromParent();
        m_overlayNode = nullptr;
    }
    m_rewindLabel = nullptr;
    m_rewindsLeftLabel = nullptr;
    
    RewindVisuals::get()->cleanup();
}

void TimeRewindManager::playRewindSound() {
    FMODAudioEngine::sharedEngine()->playEffect("quitSound_01.ogg", 1.0f, 0.8f, 0.5f);
}

void TimeRewindManager::stopRewindSound() {
    // FMOD doesn't have simple way to stop specific effect
}
