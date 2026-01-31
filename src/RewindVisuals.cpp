#include "RewindVisuals.hpp"
#include "TimeRewindManager.hpp"

namespace TimeRewind {

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
        
        m_grayscaleOverlay = CCLayerColor::create(ccc4(0, 0, 0, 0));
        m_grayscaleOverlay->setContentSize(winSize);
        m_grayscaleOverlay->setPosition(CCPointZero);
        m_grayscaleOverlay->setVisible(false);
        this->addChild(m_grayscaleOverlay, 0);
        
        m_rewindingLabel = CCLabelBMFont::create("REWINDING", "bigFont.fnt");
        m_rewindingLabel->setPosition(ccp(winSize.width / 2, winSize.height / 2));
        m_rewindingLabel->setScale(0.8f);
        m_rewindingLabel->setOpacity(0);
        m_rewindingLabel->setVisible(false);
        this->addChild(m_rewindingLabel, 10);
        
        m_chargesLabel = CCLabelBMFont::create("", "bigFont.fnt");
        m_chargesLabel->setAnchorPoint(ccp(1.0f, 1.0f));
        m_chargesLabel->setPosition(ccp(winSize.width - 10, winSize.height - 10));
        m_chargesLabel->setScale(0.4f);
        m_chargesLabel->setColor(ccc3(255, 200, 100));
        this->addChild(m_chargesLabel, 100);
        
        m_glitchContainer = CCNode::create();
        this->addChild(m_glitchContainer, 5);
        
        createVHSLines();
        createScanlines();
        
        m_vhsTimer = 0.0f;
        m_glitchIntensity = 0.0f;
        m_isAnimating = false;
        m_animationProgress = 0.0f;
        
        this->scheduleUpdate();
        
        return true;
    }

    void RewindOverlay::createVHSLines() {
        m_vhsLines = CCSprite::create();
        m_vhsLines->setVisible(false);
        m_glitchContainer->addChild(m_vhsLines);
    }

    void RewindOverlay::createScanlines() {
        auto winSize = CCDirector::sharedDirector()->getWinSize();
        
        int lineCount = static_cast<int>(winSize.height / 4);
        
        for (int i = 0; i < lineCount; i++) {
            auto line = CCLayerColor::create(ccc4(0, 0, 0, 10), winSize.width, 1);
            if (line) {
                line->setPosition(ccp(0, i * 4));
                line->setVisible(false);
                m_glitchContainer->addChild(line);
            }
        }
    }

    void RewindOverlay::onHideGrayscale() {
        m_grayscaleOverlay->setVisible(false);
    }

    void RewindOverlay::onHideLabel() {
        m_rewindingLabel->setVisible(false);
    }

    void RewindOverlay::showRewindUI() {
        m_isAnimating = true;
        m_animationProgress = 0.0f;
        
        auto manager = TimeRewindManager::get();
        
        if (manager->isGrayscaleEnabled()) {
            m_grayscaleOverlay->setVisible(true);
            m_grayscaleOverlay->setOpacity(0);
            m_grayscaleOverlay->runAction(CCFadeTo::create(0.2f, 80));
        }
        
        m_rewindingLabel->setVisible(true);
        m_rewindingLabel->setOpacity(0);
        m_rewindingLabel->setScale(1.2f);
        m_rewindingLabel->runAction(CCSpawn::create(
            CCFadeTo::create(0.15f, 255),
            CCScaleTo::create(0.15f, 0.8f),
            nullptr
        ));
        
        auto pulse = CCSequence::create(
            CCScaleTo::create(0.3f, 0.85f),
            CCScaleTo::create(0.3f, 0.75f),
            nullptr
        );
        m_rewindingLabel->runAction(CCRepeatForever::create(pulse));
        
        if (manager->isVHSEffectEnabled()) {
            enableVHSEffect(true);
            triggerGlitch(1.0f);
        }
    }

    void RewindOverlay::hideRewindUI() {
        if (m_grayscaleOverlay->isVisible()) {
            m_grayscaleOverlay->runAction(CCSequence::create(
                CCFadeTo::create(0.3f, 0),
                CCCallFunc::create(this, callfunc_selector(RewindOverlay::onHideGrayscale)),
                nullptr
            ));
        }
        
        m_rewindingLabel->stopAllActions();
        m_rewindingLabel->runAction(CCSequence::create(
            CCFadeTo::create(0.2f, 0),
            CCCallFunc::create(this, callfunc_selector(RewindOverlay::onHideLabel)),
            nullptr
        ));
        
        enableVHSEffect(false);
        
        m_isAnimating = false;
    }

    void RewindOverlay::updateChargesDisplay(int charges, int maxCharges) {
        std::string text = fmt::format("Rewind: {}/{}", charges, maxCharges);
        m_chargesLabel->setString(text.c_str());
        
        if (charges == 0) {
            m_chargesLabel->setColor(ccc3(255, 80, 80));
        } else if (charges == 1) {
            m_chargesLabel->setColor(ccc3(255, 200, 80));
        } else {
            m_chargesLabel->setColor(ccc3(100, 255, 100));
        }
        
        m_chargesLabel->stopAllActions();
        m_chargesLabel->setScale(0.5f);
        m_chargesLabel->runAction(CCScaleTo::create(0.15f, 0.4f));
    }

    void RewindOverlay::setRewindProgress(float progress) {
        m_animationProgress = progress;
        
        int dots = static_cast<int>(progress * 3) % 4;
        std::string dotStr(dots, '.');
        std::string text = fmt::format("REWINDING{}", dotStr);
        m_rewindingLabel->setString(text.c_str());
        
        if (progress > 0.8f) {
            float intensity = (progress - 0.8f) * 5.0f;
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
        
        for (auto child : CCArrayExt<CCNode*>(m_glitchContainer->getChildren())) {
            child->setVisible(enable);
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
        
        if (m_vhsLines && m_vhsLines->isVisible()) {
            m_vhsTimer += dt * 100.0f;
            float yOffset = std::fmod(m_vhsTimer, winSize.height);
            m_vhsLines->setPositionY(yOffset);
        }
        
        m_glitchIntensity *= 0.95f;
        if (m_glitchIntensity < 0.01f) {
            m_glitchIntensity = 0.0f;
        }
    }

    void RewindOverlay::update(float dt) {
        updateGlitchEffect(dt);
        
        if (m_isAnimating && TimeRewindManager::get()->isVHSEffectEnabled()) {
            float randVal = static_cast<float>(rand()) / static_cast<float>(RAND_MAX);
            if (randVal < 0.05f) {
                float randIntensity = static_cast<float>(rand()) / static_cast<float>(RAND_MAX);
                triggerGlitch(0.3f + randIntensity * 0.3f);
            }
        }
    }

    // RewindVisuals implementation

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
        
        m_overlay = RewindOverlay::create();
        if (m_overlay) {
            if (playLayer->m_uiLayer) {
                playLayer->m_uiLayer->addChild(m_overlay, 1000);
            } else {
                playLayer->addChild(m_overlay, 1000);
            }
            
            auto manager = TimeRewindManager::get();
            m_overlay->updateChargesDisplay(manager->getCharges(), manager->getMaxCharges());
        }
        
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

}
