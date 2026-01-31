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
}

void RewindVisuals::createTintOverlay() {
    if (!m_playLayer) return;
    
    auto winSize = CCDirector::sharedDirector()->getWinSize();
    
    // Синеватый оттенок экрана
    m_tintLayer = CCLayerColor::create(ccc4(50, 100, 200, 60));
    m_tintLayer->setContentSize(winSize);
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
    
    // Создаём горизонтальные полосы VHS-эффекта
    for (int i = 0; i < 5; i++) {
        auto* line = CCLayerColor::create(ccc4(255, 255, 255, 20));
        float height = 2.0f + static_cast<float>(rand() % 4);
        line->setContentSize(CCSizeMake(winSize.width, height));
        
        float startY = static_cast<float>(rand() % static_cast<int>(winSize.height));
        line->setPosition(ccp(0, startY));
        
        m_effectContainer->addChild(line, 2);
        m_vhsLines.push_back(line);
        
        // Анимация движения полосы вниз
        float duration = 1.0f + static_cast<float>(rand() % 200) / 100.0f;
        auto* moveDown = CCMoveBy::create(duration, ccp(0, -winSize.height - 50));
        auto* resetPos = CCMoveTo::create(0.0f, ccp(0, winSize.height + 10));
        auto* seq = CCSequence::create(moveDown, resetPos, nullptr);
        line->runAction(CCRepeatForever::create(seq));
    }
    
    // Создаём "глитч" полосы
    for (int i = 0; i < 3; i++) {
        auto* glitch = CCLayerColor::create(ccc4(255, 50, 50, 30));
        float width = winSize.width * (0.2f + static_cast<float>(rand() % 30) / 100.0f);
        float height = 10.0f + static_cast<float>(rand() % 20);
        glitch->setContentSize(CCSizeMake(width, height));
        
        float startX = static_cast<float>(rand() % static_cast<int>(winSize.width));
        float startY = static_cast<float>(rand() % static_cast<int>(winSize.height));
        glitch->setPosition(ccp(startX, startY));
        
        m_effectContainer->addChild(glitch, 3);
        m_glitchBars.push_back(glitch);
        
        // Мигание
        auto* fadeOut = CCFadeTo::create(0.05f, 0);
        auto* fadeIn = CCFadeTo::create(0.05f, 40);
        auto* delay = CCDelayTime::create(static_cast<float>(rand() % 100) / 100.0f);
        auto* seq = CCSequence::create(fadeIn, delay, fadeOut, delay->copy(), nullptr);
        glitch->runAction(CCRepeatForever::create(seq));
    }
}

void RewindVisuals::updateEffect(float progress) {
    if (!m_active) return;
    
    m_effectTime += 0.016f;
    
    // Перемещаем глитч-полосы случайно
    auto winSize = CCDirector::sharedDirector()->getWinSize();
    
    for (auto* glitch : m_glitchBars) {
        if (rand() % 10 == 0) {
            float newX = static_cast<float>(rand() % static_cast<int>(winSize.width));
            float newY = static_cast<float>(rand() % static_cast<int>(winSize.height));
            glitch->setPosition(ccp(newX, newY));
        }
    }
}

void RewindVisuals::stopRewindEffect() {
    if (!m_active) return;
    
    m_active = false;
    
    // Плавное затухание
    if (m_effectContainer) {
        // Останавливаем все экшены и затухаем
        for (auto* line : m_vhsLines) {
            line->stopAllActions();
            line->runAction(CCFadeOut::create(0.2f));
        }
        for (auto* glitch : m_glitchBars) {
            glitch->stopAllActions();
            glitch->runAction(CCFadeOut::create(0.2f));
        }
        if (m_tintLayer) {
            m_tintLayer->stopAllActions();
            m_tintLayer->runAction(CCFadeOut::create(0.2f));
        }
        
        // Удаляем контейнер после затухания
        auto* delay = CCDelayTime::create(0.3f);
        auto* remove = CCRemoveSelf::create();
        m_effectContainer->runAction(CCSequence::create(delay, remove, nullptr));
        m_effectContainer = nullptr;
    }
    
    m_tintLayer = nullptr;
    m_vhsLines.clear();
    m_glitchBars.clear();
}

void RewindVisuals::cleanup() {
    if (m_effectContainer) {
        m_effectContainer->stopAllActions();
        m_effectContainer->removeFromParent();
        m_effectContainer = nullptr;
    }
    
    m_tintLayer = nullptr;
    m_vhsLines.clear();
    m_glitchBars.clear();
    m_active = false;
    m_playLayer = nullptr;
}
