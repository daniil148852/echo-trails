#pragma once

#include <Geode/Geode.hpp>
#include <Geode/cocos/include/cocos2d.h>

using namespace geode::prelude;

namespace TimeRewind {

    class RewindOverlay : public CCNode {
    private:
        CCLayerColor* m_grayscaleOverlay;
        CCLabelBMFont* m_rewindingLabel;
        CCLabelBMFont* m_chargesLabel;
        CCSprite* m_vhsLines;
        CCNode* m_glitchContainer;
        
        // VHS effect state
        float m_vhsTimer;
        float m_glitchIntensity;
        std::vector<CCSprite*> m_scanlines;
        
        // Animation state
        bool m_isAnimating;
        float m_animationProgress;
        
        bool init();
        void createVHSLines();
        void createScanlines();
        void updateGlitchEffect(float dt);
        
    public:
        static RewindOverlay* create();
        
        // Lifecycle
        void showRewindUI();
        void hideRewindUI();
        void updateChargesDisplay(int charges, int maxCharges);
        void setRewindProgress(float progress);
        
        // Effects
        void enableGrayscale(bool enable);
        void enableVHSEffect(bool enable);
        void triggerGlitch(float intensity = 1.0f);
        
        // Update loop
        void update(float dt) override;
        
        // Accessors
        bool isVisible() const { return m_isAnimating; }
    };

    class RewindVisuals {
    private:
        static RewindVisuals* s_instance;
        
        RewindOverlay* m_overlay;
        PlayLayer* m_playLayer;
        bool m_initialized;
        
        RewindVisuals();
        
    public:
        static RewindVisuals* get();
        static void destroy();
        
        void init(PlayLayer* playLayer);
        void cleanup();
        
        // UI Control
        void onRewindStart();
        void onRewindEnd();
        void onRewindProgress(float progress);
        void updateCharges(int charges, int maxCharges);
        
        // Effect settings
        void setGrayscaleEnabled(bool enabled);
        void setVHSEnabled(bool enabled);
        
        // Access to overlay
        RewindOverlay* getOverlay() { return m_overlay; }
    };

} // namespace TimeRewind
