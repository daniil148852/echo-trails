#pragma once

#include <Geode/Geode.hpp>
#include <Geode/ui/Popup.hpp>
#include <Geode/ui/TextInput.hpp>
#include <Geode/ui/ScrollLayer.hpp>

using namespace geode::prelude;

/**
 * @brief Chat UI popup for interacting with the AI
 */
class ChatPopup : public geode::Popup<> {
protected:
    ScrollLayer* m_scrollLayer;
    CCNode* m_chatContainer;
    TextInput* m_inputField;
    CCMenuItemSpriteExtra* m_sendButton;
    CCLabelBMFont* m_statusLabel;
    CCNode* m_loadingIndicator;
    
    float m_chatHeight;
    bool m_isWaitingForResponse;
    
    bool setup() override;
    void onSend(CCObject* sender);
    void onClose(CCObject* sender) override;
    void onClear(CCObject* sender);
    void onAnalyze(CCObject* sender);
    void onTip(CCObject* sender);
    
    void addMessage(const std::string& content, bool isUser);
    void addSystemMessage(const std::string& content);
    void scrollToBottom();
    void setLoading(bool loading);
    void updateInputState();
    
    void keyDown(cocos2d::enumKeyCodes key) override;
    
public:
    static ChatPopup* create();
    void show() override;
    
    void refreshHistory();
};
