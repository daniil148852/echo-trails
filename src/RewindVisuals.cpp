#include "RewindVisuals.hpp"
#include "TimeRewindManager.hpp"

using namespace geode::prelude;

RewindVisuals* RewindVisuals::s_instance = nullptr;

// ============================================================
// Initialization
// ============================================================

bool RewindVisuals::init() {
    if (!CCNode::init()) {
        return false;
    }
    
    m_overlayContainer = nullptr;
    m_dimLayer = nullptr;
    m_rewindLabel = nullptr;
    m_chargesLabel = nullptr;
    m_rewindIcon = nullptr;
    m_progressBar = nullptr;
    m_vhsEffectContainer = nullptr;
    m_scanlines = nullptr;
    m_staticNoise = nullptr;
    
    m_glitchTimer = 0.0f;
    m_scanlineOffset = 0.0f;
    m_labelPulseTimer = 0.0f;
    m_vhsIntensity = 0.0f;
    m_effectsEnabled = true;
    m_isShowingOverlay = false;
    
    // Enable updates
    scheduleUpdate();
    
    return true;
}

RewindVisuals* RewindVisuals::get() {
    if (!s_instance) {
        s_instance = new RewindVisuals();
        if (s_instance && s_instance->init()) {
            s_instance->autorelease();
            s_instance->retain(); // Keep alive
        } else {
            CC_SAFE_DELETE(s_instance);
        }
    }
    return s_instance;
}

void RewindVisuals::destroy() {
    if (s_instance) {
        s_instance->cleanupVisuals();
        s_instance->release();
        s_instance = nullptr;
    }
}

// ============================================================
// Charges Display
// ============================================================

void RewindVisuals::createChargesDisplay(PlayLayer* playLayer) {
    if (!playLayer) return;
    
    // Remove existing label if any
    if (m_chargesLabel) {
        m_chargesLabel->removeFromParent();
        m_chargesLabel = nullptr;
    }
    
    auto winSize = CCDirector::sharedDirector()->getWinSize();
    
    // Create charges label
    m_chargesLabel = CCLabelBMFont::create("", "bigFont.fnt");
    m_chargesLabel->setScale(0.35f);
    m_chargesLabel->setAnchorPoint({1.0f, 1.0f});
    m_chargesLabel->setPosition({winSize.width - 10.0f, winSize.height - 10.0f});
    m_chargesLabel->setZOrder(1000);
    m_chargesLabel->setID("time-rewind-charges"_spr);
    
    // Add outline effect
    m_chargesLabel->setColor({255, 255, 255});
    m_chargesLabel->setOpacity(220);
    
    // Add to UI layer (to avoid camera transform issues in GD 2.2)
    if (playLayer->m_uiLayer) {
        playLayer->m_uiLayer->addChild(m_chargesLabel);
    } else {
        playLayer->addChild(m_chargesLabel);
    }
    
    // Initial update
    auto manager = TimeRewindManager::get();
    updateChargesDisplay(manager->getCharges(), manager->getMaxCharges());
}

void RewindVisuals::updateChargesDisplay(int charges, int maxCharges) {
    if (!m_chargesLabel) return;
    
    // Format: "REWIND: 3/3"
    std::string text = fmt::format("REWIND: {}/{}", charges, maxCharges);
    m_chargesLabel->setString(text.c_str());
    
    // Color based on remaining charges
    ccColor3B color;
    if (charges == 0) {
        color = {255, 80, 80};      // Red - no charges
    } else if (charges == 1) {
        color = {255, 200, 80};     // Yellow - low charges
    } else {
        color = {120, 255, 120};    // Green - charges available
    }
    m_chargesLabel->setColor(color);
}

// ============================================================
// Rewind Overlay
// ============================================================

void RewindVisuals::showRewindOverlay(PlayLayer* playLayer) {
    if (!playLayer || m_isShowingOverlay) return;
    
    m_isShowingOverlay = true;
    m_vhsIntensity = 0.5f;
    
    auto winSize = CCDirector::sharedDirector()->getWinSize();
    
    // Create overlay container
    m_overlayContainer = CCNode::create();
    m_overlayContainer->setZOrder(999);
    m_overlayContainer->setID("time-rewind-overlay"_spr);
    
    // === Dim layer (semi-transparent background) ===
    m_dimLayer = CCLayerColor::create({0, 0, 0, 100});
    m_overlayContainer->addChild(m_dimLayer, 0);
    
    // === REWINDING label ===
    createRewindLabel();
    
    // === Progress bar ===
    createProgressBar();
    
    // === VHS effects ===
    if (m_effectsEnabled) {
        createVHSEffect();
    }
    
    // Add overlay to UI layer
    if (playLayer->m_uiLayer) {
        playLayer->m_uiLayer->addChild(m_overlayContainer);
    } else {
        playLayer->addChild(m_overlayContainer);
    }
    
    // Animate entrance
    m_overlayContainer->setScale(1.1f);
    m_dimLayer->setOpacity(0);
    
    auto scaleAction = CCEaseOut::create(CCScaleTo::create(0.15f, 1.0f), 2.0f);
    auto fadeAction = CCFadeTo::create(0.15f, 100);
    
    m_overlayContainer->runAction(scaleAction);
    m_dimLayer->runAction(fadeAction);
    
    log::debug("[RewindVisuals] Showing overlay");
}

void RewindVisuals::hideRewindOverlay(PlayLayer* playLayer) {
    if (!m_overlayContainer || !m_isShowingOverlay) return;
    
    m_isShowingOverlay = false;
    
    // Animate exit and remove
    auto fadeAction = CCFadeTo::create(0.2f, 0);
    auto scaleAction = CCScaleTo::create(0.2f, 0.9f);
    auto removeAction = CCRemoveSelf::create();
    
    if (m_dimLayer) {
        m_dimLayer->runAction(fadeAction);
    }
    
    m_overlayContainer->runAction(CCSequence::create(
        scaleAction,
        CCDelayTime::create(0.1f),
        removeAction,
        nullptr
    ));
    
    // Clear references
    m_overlayContainer = nullptr;
    m_dimLayer = nullptr;
    m_rewindLabel = nullptr;
    m_progressBar = nullptr;
    m_vhsEffectContainer = nullptr;
    m_scanlines = nullptr;
    m_staticNoise = nullptr;
    
    log::debug("[RewindVisuals] Hiding overlay");
}

void RewindVisuals::updateRewindProgress(float progress) {
    // Update progress bar
    if (m_progressBar) {
        m_progressBar->setPercentage(progress * 100.0f);
    }
    
    // Update label text with animation
    if (m_rewindLabel) {
        // Cycle through dots
        int dotCount = static_cast<int>(progress * 12.0f) % 4;
        std::string text = "REWINDING";
        for (int i = 0; i < dotCount; i++) {
            text += ".";
        }
        m_rewindLabel->setString(text.c_str());
    }
    
    // Intensify VHS effect as rewind progresses
    m_vhsIntensity = 0.5f + progress * 0.5f;
    
    if (m_staticNoise && m_effectsEnabled) {
        m_staticNoise->setOpacity(static_cast<GLubyte>(m_vhsIntensity * 30));
    }
}

// ============================================================
// Effect Creation
// ============================================================

void RewindVisuals::createRewindLabel() {
    if (!m_overlayContainer) return;
    
    auto winSize = CCDirector::sharedDirector()->getWinSize();
    
    m_rewindLabel = CCLabelBMFont::create("REWINDING", "goldFont.fnt");
    m_rewindLabel->setPosition({winSize.width / 2.0f, winSize.height / 2.0f + 60.0f});
    m_rewindLabel->setScale(0.8f);
    m_rewindLabel->setColor({255, 120, 120});
    m_rewindLabel->setZOrder(10);
    
    m_overlayContainer->addChild(m_rewindLabel);
    
    // Add pulsing animation
    auto pulseUp = CCScaleTo::create(0.4f, 0.85f);
    auto pulseDown = CCScaleTo::create(0.4f, 0.75f);
    auto pulseSeq = CCSequence::create(pulseUp, pulseDown, nullptr);
    m_rewindLabel->runAction(CCRepeatForever::create(pulseSeq));
}

void RewindVisuals::createProgressBar() {
    if (!m_overlayContainer) return;
    
    auto winSize = CCDirector::sharedDirector()->getWinSize();
    
    // Background bar
    auto bgBar = CCSprite::create("GJ_progressBar_001.png");
    bgBar->setPosition({winSize.width / 2.0f, winSize.height / 2.0f + 20.0f});
    bgBar->setScaleX(0.8f);
    bgBar->setScaleY(0.6f);
    bgBar->setColor({50, 50, 50});
    bgBar->setOpacity(180);
    m_overlayContainer->addChild(bgBar, 5);
    
    // Progress bar
    auto barSprite = CCSprite::create("GJ_progressBar_001.png");
    barSprite->setColor({255, 100, 100});
    
    m_progressBar = CCProgressTimer::create(barSprite);
    m_progressBar->setType(CCProgressTimerType::kCCProgressTimerTypeBar);
    m_progressBar->setMidpoint({0, 0.5f});
    m_progressBar->setBarChangeRate({1, 0});
    m_progressBar->setPosition(bgBar->getPosition());
    m_progressBar->setScaleX(0.78f);
    m_progressBar->setScaleY(0.55f);
    m_progressBar->setPercentage(0);
    m_progressBar->setZOrder(6);
    
    m_overlayContainer->addChild(m_progressBar);
}

void RewindVisuals::createVHSEffect() {
    if (!m_overlayContainer) return;
    
    auto winSize = CCDirector::sharedDirector()->getWinSize();
    
    m_vhsEffectContainer = CCNode::create();
    m_vhsEffectContainer->setZOrder(2);
    m_overlayContainer->addChild(m_vhsEffectContainer);
    
    // Create scanlines
    createScanlines();
    
    // Create static noise layer
    m_staticNoise = CCLayerColor::create({255, 255, 255, 0});
    m_staticNoise->setContentSize(winSize);
    m_staticNoise->setOpacity(0);
    m_vhsEffectContainer->addChild(m_staticNoise, 1);
}

void RewindVisuals::createScanlines() {
    if (!m_vhsEffectContainer) return;
    
    auto winSize = CCDirector::sharedDirector()->getWinSize();
    
    m_scanlines = CCDrawNode::create();
    m_scanlines->setZOrder(2);
    
    // Draw horizontal scanlines
    ccColor4F lineColor = {0, 0, 0, 0.15f};
    
    for (float y = 0; y < winSize.height; y += 3.0f) {
        CCPoint verts[4] = {
            {0, y},
            {winSize.width, y},
            {winSize.width, y + 1.0f},
            {0, y + 1.0f}
        };
        m_scanlines->drawPolygon(verts, 4, lineColor, 0, lineColor);
    }
    
    m_vhsEffectContainer->addChild(m_scanlines);
}

// ============================================================
// Animation Updates
// ============================================================

void RewindVisuals::update(float dt) {
    if (!m_isShowingOverlay) return;
    
    m_glitchTimer += dt;
    
    if (m_effectsEnabled) {
        updateVHSEffect(dt);
        updateScanlines(dt);
    }
    
    updateLabelPulse(dt);
}

void RewindVisuals::updateVHSEffect(float dt) {
    if (!m_vhsEffectContainer) return;
    
    // Random glitch effect
    if (m_glitchTimer > 0.1f && (rand() % 100) < 10) {
        m_glitchTimer = 0.0f;
        
        // Random offset
        float offsetX = (rand() % 10 - 5) * 0.5f;
        float offsetY = (rand() % 4 - 2) * 0.5f;
        
        auto originalPos = m_vhsEffectContainer->getPosition();
        m_vhsEffectContainer->setPosition(originalPos + CCPoint{offsetX, offsetY});
        
        // Quick shake back
        auto moveBack = CCMoveTo::create(0.05f, originalPos);
        m_vhsEffectContainer->runAction(moveBack);
        
        // Static noise flash
        if (m_staticNoise) {
            m_staticNoise->setOpacity(rand() % 30);
            auto fadeOut = CCFadeTo::create(0.08f, 0);
            m_staticNoise->runAction(fadeOut);
        }
    }
}

void RewindVisuals::updateScanlines(float dt) {
    if (!m_scanlines) return;
    
    m_scanlineOffset += dt * 60.0f; // Scroll speed
    
    if (m_scanlineOffset > 3.0f) {
        m_scanlineOffset = 0.0f;
    }
    
    // Move scanlines up slightly for scrolling effect
    m_scanlines->setPositionY(m_scanlineOffset);
}

void RewindVisuals::updateLabelPulse(float dt) {
    // Label pulsing is handled by CCAction, no need for manual update
}

// ============================================================
// Cleanup
// ============================================================

void RewindVisuals::cleanupVisuals() {
    // Remove overlay if showing
    if (m_overlayContainer) {
        m_overlayContainer->removeFromParent();
        m_overlayContainer = nullptr;
    }
    
    // Remove charges label
    if (m_chargesLabel) {
        m_chargesLabel->removeFromParent();
        m_chargesLabel = nullptr;
    }
    
    // Clear all references
    m_dimLayer = nullptr;
    m_rewindLabel = nullptr;
    m_progressBar = nullptr;
    m_vhsEffectContainer = nullptr;
    m_scanlines = nullptr;
    m_staticNoise = nullptr;
    m_rewindIcon = nullptr;
    
    m_isShowingOverlay = false;
    
    log::debug("[RewindVisuals] Cleaned up");
}
