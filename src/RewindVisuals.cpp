#include "RewindVisuals.hpp"
#include "TimeRewindManager.hpp"

namespace TimeRewind {

    // ==================== RewindOverlay ====================

    RewindOverlay* RewindOverlay::create() {
        auto ret = new RewindOverlay();
        if (ret && ret->init()) {
            ret->autorelease();
            return ret;
        }
        CC_SAFE_DELETE(ret);
        return nullptr;
    }

    bool RewindOverlay::init() {
        if (!CCNode::init()) {
            return false;
        }
        
        auto winSize = CCDirector::sharedDirector()->getWinSize();
        
        // Create grayscale overlay (semi-transparent gray)
        m_grayscaleOverlay = CCLayerColor::create(ccc4(0, 0, 0, 0));
        m_grayscaleOverlay->setContentSize(winSize);
        m_grayscaleOverlay->setPosition(CCPointZero);
        m_grayscaleOverlay->setVisible(false);
        this->addChild(m_grayscaleOverlay, 0);
        
        // Create "REWINDING" label
        m_rewindingLabel = CCLabelBMFont::create("REWINDING", "bigFont.fnt");
        m_rewindingLabel->setPosition(ccp(winSize.width / 2, winSize.height / 2));
        m_rewindingLabel->setScale(0.8f);
        m_rewindingLabel->setOpacity(0);
        m_rewindingLabel->setVisible(false);
        this->addChild(m_rewindingLabel, 10);
        
        // Create charges label (top-right corner)
        m_chargesLabel = CCLabelBMFont::create("", "bigFont.fnt");
        m_chargesLabel->setAnchorPoint(ccp(1.0f, 1.0f));
        m_chargesLabel->setPosition(ccp(winSize.width - 10, winSize.height - 10));
        m_chargesLabel->setScale(0.4f);
        m_chargesLabel->setColor(ccc3(255, 200, 100));
        this->addChild(m_chargesLabel, 100);
        
        // Create glitch container
        m_glitchContainer = CCNode::create();
        this->addChild(m_glitchContainer, 5);
        
        // Create VHS effect elements
        createVHSLines();
        createScanlines();
        
        m_vhsTimer = 0.0f;
        m_glitchIntensity = 0.0f;
        m_isAnimating = false;
        m_animationProgress = 0.0f;
        
        // Schedule update
        this->scheduleUpdate();
        
        return true;
    }

    void RewindOverlay::createVHSLines() {
        auto winSize = CCDirector::sharedDirector()->getWinSize();
        
        // Create horizontal distortion lines
        m_vhsLines = CCSprite::create();
        
        // Create a simple texture for VHS tracking lines
        // Using a repeating pattern of semi-transparent lines
        auto texture = CCRenderTexture::create(winSize.width, 4);
        texture->begin();
        
        CCDrawNode* drawNode = CCDrawNode::create();
        drawNode->drawSegment(ccp(0, 2), ccp(winSize.width, 2), 2, ccc4f(1, 1, 1, 0.3f));
        drawNode->visit();
        
        texture->end();
        
        m_vhsLines->setVisible(false);
        m_glitchContainer->addChild(m_vhsLines);
    }

    void RewindOverlay::createScanlines() {
        auto winSize = CCDirector::sharedDirector()->getWinSize();
        
        // Create subtle scanline effect
        int lineCount = static_cast<int>(winSize.height / 3);
        
        for (int i = 0; i < lineCount; i++) {
            auto line = CCSprite::create("square.png");
            if (line) {
                line->setScaleX(winSize.width / line->getContentSize().width);
                line->setScaleY(0.5f);
                line->setOpacity(0);
                line->setColor(ccc3(0, 0, 0));
                line->setPosition(ccp(winSize.width / 2, i * 3));
                line->setVisible(false);
                m_glitchContainer->addChild(line);
                m_scanlines.push_back(line);
            }
        }
    }

    void RewindOverlay::showRewindUI() {
        m_isAnimating = true;
        m_animationProgress = 0.0f;
        
        auto manager = TimeRewindManager::get();
        
        // Show grayscale overlay
        if (manager->isGrayscaleEnabled()) {
            m_grayscaleOverlay->setVisible(true);
            m_grayscaleOverlay->setOpacity(0);
            m_grayscaleOverlay->runAction(CCFadeTo::create(0.2f, 80));
        }
        
        // Animate "REWINDING" label
        m_rewindingLabel->setVisible(true);
        m_rewindingLabel->setOpacity(0);
        m_rewindingLabel->setScale(1.2f);
        m_rewindingLabel->runAction(CCSpawn::create(
            CCFadeTo::create(0.15f, 255),
            CCScaleTo::create(0.15f, 0.8f),
            nullptr
        ));
        
        // Pulse animation for label
        auto pulse = CCSequence::create(
            CCScaleTo::create(0.3f, 0.85f),
            CCScaleTo::create(0.3f, 0.75f),
            nullptr
        );
        m_rewindingLabel->runAction(CCRepeatForever::create(pulse));
        
        // Show VHS effect
        if (manager->isVHSEffectEnabled()) {
            enableVHSEffect(true);
            triggerGlitch(1.0f);
        }
    }

    void RewindOverlay::hideRewindUI() {
        // Fade out grayscale
        if (m_grayscaleOverlay->isVisible()) {
            m_grayscaleOverlay->runAction(CCSequence::create(
                CCFadeTo::create(0.3f, 0),
                CCCallFunc::create(this, callfunc_selector(RewindOverlay::hideGrayscaleCallback)),
                nullptr
            ));
        }
        
        // Hide rewinding label
        m_rewindingLabel->stopAllActions();
        m_rewindingLabel->runAction(CCSequence::create(
            CCFadeTo::create(0.2f, 0),
            CCCallFunc::create(this, callfunc_selector(RewindOverlay::hideLabelCallback)),
            nullptr
        ));
        
        // Hide VHS effects
        enableVHSEffect(false);
        
        m_isAnimating = false;
    }

    void RewindOverlay::hideGrayscaleCallback() {
        m_grayscaleOverlay->setVisible(false);
    }

    void RewindOverlay::hideLabelCallback() {
        m_rewindingLabel->setVisible(false);
    }

    void RewindOverlay::updateChargesDisplay(int charges, int maxCharges) {
        std::string text = fmt::format("Rewind: {}/{}", charges, maxCharges);
        m_chargesLabel->setString(text.c_str());
        
        // Color based on charges remaining
        if (charges == 0) {
            m_chargesLabel->setColor(ccc3(255, 80, 80));
        } else if (charges == 1) {
            m_chargesLabel->setColor(ccc3(255, 200, 80));
        } else {
            m_chargesLabel->setColor(ccc3(100, 255, 100));
        }
        
        // Quick pulse animation on change
        m_chargesLabel->stopAllActions();
        m_chargesLabel->setScale(0.5f);
        m_chargesLabel->runAction(CCScaleTo::create(0.15f, 0.4f));
    }

    void RewindOverlay::setRewindProgress(float progress) {
        m_animationProgress = progress;
        
        // Update label with progress indicator
        int dots = static_cast<int>(progress * 3) % 4;
        std::string dotStr(dots, '.');
        std::string text = fmt::format("REWINDING{}", dotStr);
        m_rewindingLabel->setString(text.c_str());
        
        // Intensify glitch near the end
        if (progress > 0.8f) {
            float intensity = (progress - 0.8f) * 5.0f; // 0-1 over last 20%
            triggerGlitch(intensity);
        }
    }

    void RewindOverlay::enableGrayscale(bool enable) {
        if (enable) {
            m_grayscaleOverlay->setVisible(true);
            m_grayscaleOverlay->setOpacity(80);
        } else {
            m_grayscaleOverlay->setVisible(false);
        }
    }

    void RewindOverlay::enableVHSEffect(bool enable) {
        if (m_vhsLines) {
            m_vhsLines->setVisible(enable);
        }
        
        for (auto& scanline : m_scanlines) {
            scanline->setVisible(enable);
            if (enable) {
                scanline->setOpacity(15);
            }
        }
    }

    void RewindOverlay::triggerGlitch(float intensity) {
        m_glitchIntensity = intensity;
    }

    void RewindOverlay::updateGlitchEffect(float dt) {
        if (m_glitchIntensity <= 0.0f || !TimeRewindManager::get()->isVHSEffectEnabled()) {
            return;
        }
        
        auto winSize = CCDirector::sharedDirector()->getWinSize();
        
        // Random horizontal offset for glitch bars
        for (size_t i = 0; i < m_scanlines.size(); i++) {
            auto& line = m_scanlines[i];
            
            // Random chance to show glitch
            if (CCRANDOM_0_1() < 0.1f * m_glitchIntensity) {
                float offsetX = (CCRANDOM_0_1() - 0.5f) * 20.0f * m_glitchIntensity;
                line->setPositionX(winSize.width / 2 + offsetX);
                line->setOpacity(static_cast<GLubyte>(30 * m_glitchIntensity));
            } else {
                line->setPositionX(winSize.width / 2);
                line->setOpacity(10);
            }
        }
        
        // VHS tracking lines movement
        if (m_vhsLines && m_vhsLines->isVisible()) {
            m_vhsTimer += dt * 100.0f;
            float yOffset = fmodf(m_vhsTimer, winSize.height);
            m_vhsLines->setPositionY(yOffset);
        }
        
        // Decay glitch intensity
        m_glitchIntensity *= 0.95f;
        if (m_glitchIntensity < 0.01f) {
            m_glitchIntensity = 0.0f;
        }
    }

    void RewindOverlay::update(float dt) {
        updateGlitchEffect(dt);
        
        // Continuous random small glitches during rewind
        if (m_isAnimating && TimeRewindManager::get()->isVHSEffectEnabled()) {
            if (CCRANDOM_0_1() < 0.05f) {
                triggerGlitch(0.3f + CCRANDOM_0_1() * 0.3f);
            }
        }
    }

    // ==================== RewindVisuals ====================

    RewindVisuals* RewindVisuals::s_instance = nullptr;

    RewindVisuals::RewindVisuals()
        : m_overlay(nullptr)
        , m_playLayer(nullptr)
        , m_initialized(false)
    {}

    RewindVisuals* RewindVisuals::get() {
        if (!s_instance) {
            s_instance = new RewindVisuals();
        }
        return s_instance;
    }

    void RewindVisuals::destroy() {
        if (s_instance) {
            s_instance->cleanup();
            delete s_instance;
            s_instance = nullptr;
        }
    }

    void RewindVisuals::init(PlayLayer* playLayer) {
        m_playLayer = playLayer;
        
        if (!playLayer) {
            log::error("RewindVisuals::init - PlayLayer is null");
            return;
        }
        
        // Create overlay and add to UI layer (not affected by camera transforms)
        m_overlay = RewindOverlay::create();
        if (m_overlay) {
            // Add to m_uiLayer to avoid camera rotation/zoom interference
            if (playLayer->m_uiLayer) {
                playLayer->m_uiLayer->addChild(m_overlay, 1000);
            } else {
                // Fallback to adding directly to playLayer
                playLayer->addChild(m_overlay, 1000);
            }
            
            // Initial charges display
            auto manager = TimeRewindManager::get();
            m_overlay->updateChargesDisplay(manager->getCharges(), manager->getMaxCharges());
        }
        
        // Set up callbacks
        auto manager = TimeRewindManager::get();
        manager->onRewindStart = [this]() {
            onRewindStart();
        };
        manager->onRewindEnd = [this]() {
            onRewindEnd();
        };
        manager->onRewindProgress = [this](float progress) {
            onRewindProgress(progress);
        };
        
        m_initialized = true;
        log::info("RewindVisuals initialized");
    }

    void RewindVisuals::cleanup() {
        if (m_overlay) {
            m_overlay->removeFromParent();
            m_overlay = nullptr;
        }
        
        m_playLayer = nullptr;
        m_initialized = false;
        
        log::debug("RewindVisuals cleaned up");
    }

    void RewindVisuals::onRewindStart() {
        if (m_overlay) {
            m_overlay->showRewindUI();
        }
        log::debug("RewindVisuals: onRewindStart");
    }

    void RewindVisuals::onRewindEnd() {
        if (m_overlay) {
            m_overlay->hideRewindUI();
            
            // Update charges display
            auto manager = TimeRewindManager::get();
            m_overlay->updateChargesDisplay(manager->getCharges(), manager->getMaxCharges());
        }
        log::debug("RewindVisuals: onRewindEnd");
    }

    void RewindVisuals::onRewindProgress(float progress) {
        if (m_overlay) {
            m_overlay->setRewindProgress(progress);
        }
    }

    void RewindVisuals::updateCharges(int charges, int maxCharges) {
        if (m_overlay) {
            m_overlay->updateChargesDisplay(charges, maxCharges);
        }
    }

    void RewindVisuals::setGrayscaleEnabled(bool enabled) {
        if (m_overlay) {
            m_overlay->enableGrayscale(enabled);
        }
    }

    void RewindVisuals::setVHSEnabled(bool enabled) {
        if (m_overlay) {
            m_overlay->enableVHSEffect(enabled);
        }
    }

} // namespace TimeRewind
