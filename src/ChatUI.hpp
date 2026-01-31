#pragma once

#include <Geode/Geode.hpp>
#include <Geode/ui/Popup.hpp>
#include <Geode/ui/TextInput.hpp>

using namespace geode::prelude;

class ChatUI : public Popup<> {
protected:
    CCLayer* m_chatLayer;
    TextInput* m_input;
    CCMenu* m_buttonMenu;
    CCLabelBMFont* m_statusLabel;
    CCLabelBMFont* m_responseLabel;
    
    float m_responseY;
    bool m_waiting;
    
    bool setup() override;
    
    void onSend(CCObject*);
    void onTip(CCObject*);
    void onAnalyze(CCObject*);
    void onClear(CCObject*);
    
    void addResponse(const std::string& text, bool isUser);
    void setWaiting(bool waiting);
    
public:
    static ChatUI* create();
    static void open();
};
