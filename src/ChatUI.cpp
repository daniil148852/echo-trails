#include "ChatUI.hpp"
#include "AIAssistant.hpp"
#include "GroqAPI.hpp"

using namespace geode::prelude;

bool ChatUI::setup() {
    this->setTitle("AI Assistant");
    
    m_responseY = 180.0f;
    m_waiting = false;
    
    auto winSize = CCDirector::sharedDirector()->getWinSize();
    
    // Response area background
    auto bg = CCLayerColor::create({0, 0, 0, 150});
    bg->setContentSize({350.0f, 150.0f});
    bg->setPosition({25.0f, 80.0f});
    m_mainLayer->addChild(bg);
    
    // Clipping node for responses
    m_chatLayer = CCLayer::create();
    m_chatLayer->setPosition({30.0f, 85.0f});
    m_chatLayer->setContentSize({340.0f, 140.0f});
    m_mainLayer->addChild(m_chatLayer);
    
    // Response label (simple single response for now)
    m_responseLabel = CCLabelBMFont::create("Ask me anything about GD!", "chatFont.fnt");
    m_responseLabel->setScale(0.4f);
    m_responseLabel->setAnchorPoint({0.0f, 1.0f});
    m_responseLabel->setPosition({5.0f, 135.0f});
    m_responseLabel->setColor({200, 200, 200});
    m_responseLabel->setWidth(330.0f);
    m_chatLayer->addChild(m_responseLabel);
    
    // Status label
    m_statusLabel = CCLabelBMFont::create("", "bigFont.fnt");
    m_statusLabel->setScale(0.3f);
    m_statusLabel->setPosition({200.0f, 240.0f});
    m_statusLabel->setColor({255, 255, 100});
    m_mainLayer->addChild(m_statusLabel);
    
    // Input field
    m_input = TextInput::create(250.0f, "Type message...");
    m_input->setPosition({175.0f, 50.0f});
    m_input->setMaxCharCount(200);
    m_input->setCommonFilter(CommonFilter::Any);
    m_mainLayer->addChild(m_input);
    
    // Button menu
    m_buttonMenu = CCMenu::create();
    m_buttonMenu->setPosition({0.0f, 0.0f});
    m_mainLayer->addChild(m_buttonMenu);
    
    // Send button
    auto sendSpr = ButtonSprite::create("Send", "goldFont.fnt", "GJ_button_01.png", 0.8f);
    auto sendBtn = CCMenuItemSpriteExtra::create(sendSpr, this, menu_selector(ChatUI::onSend));
    sendBtn->setPosition({340.0f, 50.0f});
    sendBtn->setScale(0.6f);
    m_buttonMenu->addChild(sendBtn);
    
    // Quick action buttons
    auto tipSpr = ButtonSprite::create("Tip", "goldFont.fnt", "GJ_button_02.png", 0.7f);
    auto tipBtn = CCMenuItemSpriteExtra::create(tipSpr, this, menu_selector(ChatUI::onTip));
    tipBtn->setPosition({80.0f, 15.0f});
    tipBtn->setScale(0.55f);
    m_buttonMenu->addChild(tipBtn);
    
    auto analyzeSpr = ButtonSprite::create("Analyze", "goldFont.fnt", "GJ_button_02.png", 0.7f);
    auto analyzeBtn = CCMenuItemSpriteExtra::create(analyzeSpr, this, menu_selector(ChatUI::onAnalyze));
    analyzeBtn->setPosition({170.0f, 15.0f});
    analyzeBtn->setScale(0.55f);
    m_buttonMenu->addChild(analyzeBtn);
    
    auto clearSpr = ButtonSprite::create("Clear", "goldFont.fnt", "GJ_button_06.png", 0.7f);
    auto clearBtn = CCMenuItemSpriteExtra::create(clearSpr, this, menu_selector(ChatUI::onClear));
    clearBtn->setPosition({260.0f, 15.0f});
    clearBtn->setScale(0.55f);
    m_buttonMenu->addChild(clearBtn);
    
    // Check API key
    if (!GroqAPI::get()->hasApiKey()) {
        m_responseLabel->setString("Please set your Groq API key in mod settings first!");
        m_responseLabel->setColor({255, 100, 100});
    }
    
    return true;
}

void ChatUI::onSend(CCObject*) {
    if (m_waiting) return;
    
    std::string msg = m_input->getString();
    if (msg.empty()) return;
    
    m_input->setString("");
    setWaiting(true);
    
    addResponse("You: " + msg, true);
    
    AIAssistant::get()->chat(msg, [this](const std::string& response) {
        Loader::get()->queueInMainThread([this, response]() {
            setWaiting(false);
            addResponse("AI: " + response, false);
        });
    });
}

void ChatUI::onTip(CCObject*) {
    if (m_waiting) return;
    
    setWaiting(true);
    addResponse("[Getting tip...]", true);
    
    AIAssistant::get()->getTip([this](const std::string& response) {
        Loader::get()->queueInMainThread([this, response]() {
            setWaiting(false);
            addResponse("Tip: " + response, false);
        });
    });
}

void ChatUI::onAnalyze(CCObject*) {
    if (m_waiting) return;
    
    setWaiting(true);
    addResponse("[Analyzing...]", true);
    
    AIAssistant::get()->analyze([this](const std::string& response) {
        Loader::get()->queueInMainThread([this, response]() {
            setWaiting(false);
            addResponse("Analysis:\n" + response, false);
        });
    });
}

void ChatUI::onClear(CCObject*) {
    AIAssistant::get()->clearHistory();
    m_responseLabel->setString("Cleared! Ask me anything.");
    m_responseLabel->setColor({200, 200, 200});
}

void ChatUI::addResponse(const std::string& text, bool isUser) {
    m_responseLabel->setString(text.c_str());
    m_responseLabel->setColor(isUser ? ccColor3B{150, 200, 255} : ccColor3B{200, 255, 200});
}

void ChatUI::setWaiting(bool waiting) {
    m_waiting = waiting;
    m_statusLabel->setString(waiting ? "Thinking..." : "");
}

ChatUI* ChatUI::create() {
    auto ret = new ChatUI();
    if (ret && ret->initAnchored(400.0f, 270.0f)) {
        ret->autorelease();
        return ret;
    }
    CC_SAFE_DELETE(ret);
    return nullptr;
}

void ChatUI::open() {
    auto popup = ChatUI::create();
    if (popup) {
        popup->setZOrder(1000);
        auto scene = CCDirector::sharedDirector()->getRunningScene();
        if (scene) {
            scene->addChild(popup);
        }
    }
}
