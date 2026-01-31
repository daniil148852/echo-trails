#pragma once

#include <Geode/Geode.hpp>

using namespace geode::prelude;

/**
 * @brief Handles visual effects during time rewind
 */
class RewindVisuals : public cocos2d::CCNode {
private:
    static RewindVisuals* s_instance;
    
    // UI Elements
    cocos2d::CCNode* m_overlayContainer;
    cocos2d::CCLayerColor* m_dimLayer;
    cocos2d::CCLabelBMFont* m_rewindLabel;
    cocos2d::CCLabelBMFont* m_chargesLabel;
    cocos2d::CCSprite* m_rewindIcon;
    cocos2d::CCProgressTimer* m_progressBar;
    
    // VHS Effect nodes
    cocos2d::CCNode* m_vhsEffectContainer;
    cocos2d::CCDrawNode* m_scanlines;
    cocos2d::CCLayerColor* m_staticNoise;
    
    // Animation state
    float m_glitchTimer;
    float m_scanlineOffset;
    float m_labelPulseTimer;
    float m_vhsIntensity;
    bool m_effectsEnabled;
    bool m_isShowingOverlay;
    
    // Private constructor (singleton pattern via create)
    bool init() override;
    
public:
    static RewindVisuals* get();
    static void destroy();
    
    /**
     * @brief Create the charges display (always visible in gameplay)
     */
    void createChargesDisplay(PlayLayer* playLayer);
    
    /**
     * @brief Update the charges display
     */
    void updateChargesDisplay(int charges, int maxCharges);
    
    /**
     * @brief Show the rewind overlay effect
     */
    void showRewindOverlay(PlayLayer* playLayer);
    
    /**
     * @brief Hide the rewind overlay effect
     */
    void hideRewindOverlay(PlayLayer* playLayer);
    
    /**
     * @brief Update the rewind progress (0.0 to 1.0)
     */
    void updateRewindProgress(float progress);
    
    /**
     * @brief Clean up all visual elements
     */
    void cleanupVisuals();
    
    /**
     * @brief Set whether effects are enabled
     */
    void setEffectsEnabled(bool enabled) { m_effectsEnabled = enabled; }
    
    /**
     * @brief Check if overlay is currently showing
     */
    bool isShowingOverlay() const { return m_isShowingOverlay; }
    
private:
    // Effect creation helpers
    void createVHSEffect();
    void createScanlines();
    void createProgressBar();
    void createRewindLabel();
    
    // Animation updates
    void updateVHSEffect(float dt);
    void updateLabelPulse(float dt);
    void updateScanlines(float dt);
    
    // Scheduled update
    void update(float dt) override;
};
