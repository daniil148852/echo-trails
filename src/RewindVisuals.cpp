#include "RewindVisuals.hpp"

RewindVisuals* RewindVisuals::get() {
    static RewindVisuals instance;
    return &instance;
}

void RewindVisuals::startRewindEffect(PlayLayer* playLayer) {
    if (!playLayer) return;
    
    m_playLayer = playLayer;
    m_active = true;
    m_effectTime = 0.0f;
    
    cleanup();
    
    auto winSize = CCDirector::sharedDirector()->getWinSize();
    
    // Контейнер эффектов
    m_effectContainer = CCNode::create();
    m_effectContainer->setPosition(CCPointZero);
    playLayer->addChild(m_effectContainer, 9999);
    
    createTintOverlay();
    createVHSEffect();
    createGhostTrail();
}

void RewindVisuals::createTintOverlay() {
    if (!m_playLayer) return;
    
    auto winSize = CCDirector::sharedDirector()->getWinSize();
    
    // Синеватый оттенок экрана
    m_tintLayer = CCLayerColor::create(ccc4(50, 100, 200, 60));
    m_tintLayer->setContentSize(winSize);
    m_tintLayer->setBlendFunc({GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA});
    m_effectContainer->addChild(m_tintLayer, 1);
    
    // Анимация пульсации
    auto* fadeIn = CCFadeTo::create(0.1f, 80);
    auto* fadeOut = CCFadeTo::create(0.1f, 40);
    auto* pulse = CCSequence::create(fadeIn, fadeOut, nullptr);
    m_tintLayer->runAction(CCRepeatForever::create(pulse));
}

void RewindVisuals::createVHSEffect() {
    if (!m_playLayer) return;
    
    auto winSize = CCDirector::sharedDirector()->getWinSize();
    
    // Создаём полосы VHS-эффекта
    for (int i = 0; i < 5; i++) {
        auto* line = CCLayerColor::create(ccc4(255, 255, 255, 20));
        line->setContentSize(CCSizeMake(winSize.width, 2.0f + (rand() % 4)));
        
        float startY = static_cast<float>(rand() % static_cast<int>(winSize.height));
        line->setPosition(ccp(0, startY));
        
        m_effectContainer->addChild(line, 2);
        
        // Анимация движения полосы
        float speed = 50.0f + (rand() % 100);
        float direction = (rand() % 2 == 0) ? 1.0f : -1.0f;
        
        auto* moveBy = CCMoveBy::create(2.0f, ccp(0, speed * direction));
        auto* reset = CCCallFunc::create(line, [line, winSize]() {
            float newY = static_cast<float>(rand() % static_cast<int>(winSize.height));
            line->setPositionY(newY);
        });
        auto* seq = CCSequence::create(moveBy, reset, nullptr);
        line->runAction(CCRepeatForever::create(seq));
    }
    
    // Создаём "глитч" полосы
    for (int i = 0; i < 3; i++) {
        auto* glitch = CCLayerColor::create(ccc4(255, 50, 50, 30));
        glitch->setContentSize(CCSizeMake(winSize.width * 0.3f, 10.0f + (rand() % 20)));
        
        float startX = static_cast<float>(rand() % static_cast<int>(winSize.width));
        float startY = static_cast<float>(rand() % static_cast<int>(winSize.height));
        glitch->setPosition(ccp(startX, startY));
        
        m_effectContainer->addChild(glitch, 3);
        
        // Хаотичное движение
        auto* randomMove = CCCallFunc::create(glitch, [glitch, winSize]() {
            float newX = static_cast<float>(rand() % static_cast<int>(winSize.width));
            float newY = static_cast<float>(rand() % static_cast<int>(winSize.height));
            glitch->setPosition(ccp(newX, newY));
            glitch->setOpacity(static_cast<GLubyte>(10 + rand() % 40));
            glitch->setScaleX(0.2f + (rand() % 100) / 100.0f);
        });
        
        auto* delay = CCDelayTime::create(0.05f + (rand() % 10) / 100.0f);
        auto* seq = CCSequence::create(randomMove, delay, nullptr);
        glitch->runAction(CCRepeatForever::create(seq));
    }
}

void RewindVisuals::createGhostTrail() {
    // Призрачный след игрока (опционально)
    m_ghostTrail.clear();
}

void RewindVisuals::updateEffect(float progress) {
    if (!m_active) return;
    
    m_effectTime += 0.016f;
    
    // Интенсивность эффектов уменьшается к концу
    float intensity = 1.0f - (progress * 0.5f);
    
    if (m_tintLayer) {
        GLubyte alpha = static_cast<GLubyte>(60 * intensity);
        // Анимация сама обновляет альфу
    }
    
    // Хроматическая аберрация (сдвиг RGB) — требует шейдера
    // В базовой версии просто меняем оттенок
    float colorShift = std::sin(m_effectTime * 10.0f) * 0.3f;
    if (m_tintLayer) {
        GLubyte r = static_cast<GLubyte>(50 + colorShift * 50);
        GLubyte g = static_cast<GLubyte>(100 - colorShift * 30);
        GLubyte b = static_cast<GLubyte>(200 + colorShift * 20);
        // m_tintLayer->setColor(ccc3(r, g, b)); // Если нужно
    }
}

void RewindVisuals::stopRewindEffect() {
    if (!m_active) return;
    
    m_active = false;
    
    // Плавное затухание
    if (m_effectContainer) {
        auto* fadeOut = CCFadeOut::create(0.3f);
        auto* remove = CCRemoveSelf::create();
        
        // Применяем к каждому дочернему элементу
        CCArray* children = m_effectContainer->getChildren();
        if (children) {
            CCObject* obj;
            CCARRAY_FOREACH(children, obj) {
                auto* node = dynamic_cast<CCNode*>(obj);
                if (node) {
                    node->runAction(CCFadeOut::create(0.3f));
                }
            }
        }
        
        m_effectContainer->runAction(CCSequence::create(
            CCDelayTime::create(0.3f),
            CCRemoveSelf::create(),
            nullptr
        ));
        
        m_effectContainer = nullptr;
    }
    
    m_tintLayer = nullptr;
    m_ghostTrail.clear();
}

void RewindVisuals::cleanup() {
    if (m_effectContainer) {
        m_effectContainer->removeFromParent();
        m_effectContainer = nullptr;
    }
    
    m_tintLayer = nullptr;
    m_ghostTrail.clear();
    m_active = false;
    m_playLayer = nullptr;
}
