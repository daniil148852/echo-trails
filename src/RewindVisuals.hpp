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
        
        float m_vhsTimer;
        float m_glitchIntensity;
        std::vector<CCSprite*> m_scanlines;
        
        bool m_isAnimating;
        float m_animationProgress;
        
        void createVHSLines();
        void createScanlines();
        void updateGlitchEffect(float dt);
        void onHideGrayscale();
        void onHideLabel();
        
    public:
        static RewindOverlay* create();
        bool init() override;
        
        void showRewindUI();
        void hideRewindUI();
        void updateChargesDisplay(int charges, int maxCharges);
        void setRewindProgress(float progress);
        
        void enableGrayscale(bool enable);
        void enableVHSEffect(bool enable);
        void triggerGlitch(float intensity = 1.0f);
        
        void update(float dt) override;
        
        bool isAnimating() const { return m_isAnimating; }
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
        
        void onRewindStart();
        void onRewindEnd();
        void onRewindProgress(float progress);
        void updateCharges(int charges, int maxCharges);
        
        void setGrayscaleEnabled(bool enabled);
        void setVHSEnabled(bool enabled);
        
        RewindOverlay* getOverlay() { return m_overlay; }
    };

}
