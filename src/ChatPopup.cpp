#include "ChatPopup.hpp"
#include "AIAssistant.hpp"
#include "GroqAPI.hpp"

using namespace geode::prelude;

bool ChatPopup::setup() {
    auto winSize = CCDirector::sharedDirector()->getWinSize();
    
    this->setTitle("AI Assistant");
    
    m_chatHeight = 0.0f;
    m_isWaitingForResponse = false;
    
    // Main container
    auto mainContainer = CCNode::create();
    mainContainer->setContentSize({380.0f, 240.0f});
    mainContainer->setAnchorPoint({0.5f, 0.5f});
    mainContainer->setPosition({winSize.width / 2, winSize.height / 2 + 10.0f});
    m_mainLayer->addChild(mainContainer);
    
    // Chat scroll area background
    auto chatBg = CCScale9Sprite::create("square02_small.png");
    chatBg->setContentSize({360.0f, 180.0f});
    chatBg->setPosition({190.0f, 140.0f});
    chatBg->setColor({30, 30, 30});
    chatBg->setOpacity(200);
    mainContainer->addChild(chatBg);
    
    // Scroll layer for chat
    m_scrollLayer = ScrollLayer::create({360.0f, 180.0f});
    m_scrollLayer->setPosition({10.0f, 50.0f});
    mainContainer->addChild(m_scrollLayer);
    
    // Chat content container
    m_chatContainer = CCNode::create();
    m_chatContainer->setContentSize({340.0f, 0.0f});
    m_chatContainer->setAnchorPoint({0.0f, 1.0f});
    m_chatContainer->setPosition({10.0f, 0.0f});
    m_scrollLayer->m_contentLayer->addChild(m_chatContainer);
    
    // Input field background
    auto inputBg = CCScale9Sprite::create("square02_small.png");
    inputBg->setContentSize({300.0f, 30.0f});
    inputBg->setPosition({160.0f, 25.0f});
    inputBg->setColor({50, 50, 50});
    mainContainer->addChild(inputBg);
    
    // Input field
    m_inputField = TextInput::create(280.0f, "Type a message...", "chatFont.fnt");
    m_inputField->setPosition({160.0f, 25.0f});
    m_inputField->setMaxCharCount(500);
    m_inputField->setCommonFilter(CommonFilter::Any);
    mainContainer->addChild(m_inputField);
    
    // Send button
    auto sendSprite = CCSprite::createWithSpriteFrameName("GJ_chatBtn_001.png");
    if (!sendSprite) {
        sendSprite = CCSprite::create("GJ_button_01.png");
    }
    sendSprite->setScale(0.7f);
    m_sendButton = CCMenuItemSpriteExtra::create(
        sendSprite,
        this,
        menu_selector(ChatPopup::onSend)
    );
    m_sendButton->setPosition({340.0f, 25.0f});
    
    auto menu = CCMenu::create();
    menu->setPosition({0, 0});
    menu->addChild(m_sendButton);
    mainContainer->addChild(menu);
    
    // Quick action buttons
    auto quickMenu = CCMenu::create();
    quickMenu->setPosition({0, 0});
    mainContainer->addChild(quickMenu);
    
    // Analyze button
    auto analyzeBtn = CCMenuItemSpriteExtra::create(
        ButtonSprite::create("Analyze", "goldFont.fnt", "GJ_button_01.png", 0.6f),
        this,
        menu_selector(ChatPopup::onAnalyze)
    );
    analyzeBtn->setPosition({60.0f, -10.0f});
    analyzeBtn->setScale(0.7f);
    quickMenu->addChild(analyzeBtn);
    
    // Get Tip button
    auto tipBtn = CCMenuItemSpriteExtra::create(
        ButtonSprite::create("Get Tip", "goldFont.fnt", "GJ_button_01.png", 0.6f),
        this,
        menu_selector(ChatPopup::onTip)
    );
    tipBtn->setPosition({150.0f, -10.0f});
    tipBtn->setScale(0.7f);
    quickMenu->addChild(tipBtn);
    
    // Clear button
    auto clearBtn = CCMenuItemSpriteExtra::create(
        ButtonSprite::create("Clear", "goldFont.fnt", "GJ_button_06.png", 0.6f),
        this,
        menu_selector(ChatPopup::onClear)
    );
    clearBtn->setPosition({240.0f, -10.0f});
    clearBtn->setScale(0.7f);
    quickMenu->addChild(clearBtn);
    
    // Status label
    m_statusLabel = CCLabelBMFont::create("", "chatFont.fnt");
    m_statusLabel->setPosition({190.0f, 245.0f});
    m_statusLabel->setScale(0.5f);
    m_statusLabel->setColor({150, 150, 150});
    mainContainer->addChild(m_statusLabel);
    
    // Loading indicator
    m_loadingIndicator = CCNode::create();
    auto loadingSprite = CCSprite::createWithSpriteFrameName("loadingCircle_001.png");
    if (loadingSprite) {
        loadingSprite->setScale(0.5f);
        loadingSprite->runAction(CCRepeatForever::create(CCRotateBy::create(1.0f, 360.0f)));
        m_loadingIndicator->addChild(loadingSprite);
    }
    m_loadingIndicator->setPosition({190.0f, 140.0f});
    m_loadingIndicator->setVisible(false);
    mainContainer->addChild(m_loadingIndicator);
    
    // Check API key
    if (!GroqAPI::get()->hasApiKey()) {
        addSystemMessage("API key not set! Please configure your Groq API key in mod settings.");
    } else {
        addSystemMessage("Hi! I'm your GD AI assistant. Ask me anything or use the quick actions below!");
    }
    
    // Refresh with existing conversation
    refreshHistory();
    
    return true;
}

void ChatPopup::onSend(CCObject* sender) {
    std::string message = m_inputField->getString();
    
    if (message.empty() || m_isWaitingForResponse) {
        return;
    }
    
    // Clear input
    m_inputField->setString("");
    
    // Add user message to UI
    addMessage(message, true);
    
    // Set loading state
    setLoading(true);
    m_statusLabel->setString("AI is thinking...");
    
    // Send to AI
    AIAssistant::get()->chat(message, [this](const std::string& response) {
        Loader::get()->queueInMainThread([this, response]() {
            setLoading(false);
            m_statusLabel->setString("");
            addMessage(response, false);
        });
    });
}

void ChatPopup::onClose(CCObject* sender) {
    Popup::onClose(sender);
}

void ChatPopup::onClear(CCObject* sender) {
    AIAssistant::get()->clearConversation();
    
    // Clear chat UI
    m_chatContainer->removeAllChildren();
    m_chatHeight = 0.0f;
    m_scrollLayer->m_contentLayer->setContentSize({340.0f, 0.0f});
    
    addSystemMessage("Conversation cleared!");
}

void ChatPopup::onAnalyze(CCObject* sender) {
    if (m_isWaitingForResponse) return;
    
    setLoading(true);
    m_statusLabel->setString("Analyzing gameplay...");
    addMessage("[Analyzing your gameplay...]", true);
    
    AIAssistant::get()->analyzeGameplay([this](const std::string& response) {
        Loader::get()->queueInMainThread([this, response]() {
            setLoading(false);
            m_statusLabel->setString("");
            addMessage(response, false);
        });
    });
}

void ChatPopup::onTip(CCObject* sender) {
    if (m_isWaitingForResponse) return;
    
    setLoading(true);
    m_statusLabel->setString("Getting tip...");
    addMessage("[Requesting tip for your trouble spot...]", true);
    
    AIAssistant::get()->requestDeathTip([this](const std::string& response) {
        Loader::get()->queueInMainThread([this, response]() {
            setLoading(false);
            m_statusLabel->setString("");
            addMessage(response, false);
        });
    });
}

void ChatPopup::addMessage(const std::string& content, bool isUser) {
    float maxWidth = 280.0f;
    float padding = 10.0f;
    
    // Create message bubble
    auto bubble = CCScale9Sprite::create("square02_small.png");
    bubble->setColor(isUser ? ccColor3B{60, 100, 180} : ccColor3B{60, 60, 60});
    
    // Create label
    auto label = CCLabelBMFont::create(content.c_str(), "chatFont.fnt");
    label->setScale(0.4f);
    label->setAnchorPoint({0, 1});
    
    // Word wrap manually by measuring
    float labelWidth = label->getContentSize().width * 0.4f;
    if (labelWidth > maxWidth - 20.0f) {
        // Need to wrap - recreate with line breaks
        std::string wrapped;
        std::string currentLine;
        std::istringstream stream(content);
        std::string word;
        
        while (stream >> word) {
            std::string testLine = currentLine.empty() ? word : currentLine + " " + word;
            auto testLabel = CCLabelBMFont::create(testLine.c_str(), "chatFont.fnt");
            testLabel->setScale(0.4f);
            
            if (testLabel->getContentSize().width * 0.4f > maxWidth - 20.0f) {
                if (!wrapped.empty()) wrapped += "\n";
                wrapped += currentLine;
                currentLine = word;
            } else {
                currentLine = testLine;
            }
        }
        if (!currentLine.empty()) {
            if (!wrapped.empty()) wrapped += "\n";
            wrapped += currentLine;
        }
        
        label->setString(wrapped.c_str());
    }
    
    float bubbleWidth = std::min(maxWidth, label->getContentSize().width * 0.4f + 20.0f);
    float bubbleHeight = label->getContentSize().height * 0.4f + 15.0f;
    
    bubble->setContentSize({bubbleWidth, bubbleHeight});
    bubble->setAnchorPoint(isUser ? CCPoint{1, 1} : CCPoint{0, 1});
    
    label->setPosition({10.0f, bubbleHeight - 8.0f});
    bubble->addChild(label);
    
    // Position the bubble
    float xPos = isUser ? 330.0f : 10.0f;
    float yPos = -m_chatHeight - padding;
    bubble->setPosition({xPos, yPos});
    
    m_chatContainer->addChild(bubble);
    
    m_chatHeight += bubbleHeight + padding;
    
    // Update scroll layer content size
    m_chatContainer->setContentSize({340.0f, m_chatHeight});
    m_scrollLayer->m_contentLayer->setContentSize({340.0f, std::max(180.0f, m_chatHeight)});
    m_chatContainer->setPosition({10.0f, std::max(180.0f, m_chatHeight)});
    
    scrollToBottom();
}

void ChatPopup::addSystemMessage(const std::string& content) {
    float padding = 8.0f;
    
    auto label = CCLabelBMFont::create(content.c_str(), "chatFont.fnt");
    label->setScale(0.35f);
    label->setColor({200, 200, 200});
    label->setAnchorPoint({0.5f, 1});
    label->setPosition({170.0f, -m_chatHeight - padding});
    
    m_chatContainer->addChild(label);
    
    m_chatHeight += label->getContentSize().height * 0.35f + padding * 2;
    
    m_chatContainer->setContentSize({340.0f, m_chatHeight});
    m_scrollLayer->m_contentLayer->setContentSize({340.0f, std::max(180.0f, m_chatHeight)});
    m_chatContainer->setPosition({10.0f, std::max(180.0f, m_chatHeight)});
}

void ChatPopup::scrollToBottom() {
    float contentHeight = m_chatHeight;
    float viewHeight = 180.0f;
    
    if (contentHeight > viewHeight) {
        m_scrollLayer->scrollToTop();
    }
}

void ChatPopup::setLoading(bool loading) {
    m_isWaitingForResponse = loading;
    m_loadingIndicator->setVisible(loading);
    m_sendButton->setEnabled(!loading);
    m_sendButton->setOpacity(loading ? 128 : 255);
}

void ChatPopup::updateInputState() {
    bool hasKey = GroqAPI::get()->hasApiKey();
    m_inputField->setEnabled(hasKey);
    m_sendButton->setEnabled(hasKey && !m_isWaitingForResponse);
}

void ChatPopup::refreshHistory() {
    const auto& history = AIAssistant::get()->getConversationHistory();
    
    for (const auto& entry : history) {
        addMessage(entry.content, entry.role == "user");
    }
}

void ChatPopup::keyDown(cocos2d::enumKeyCodes key) {
    if (key == enumKeyCodes::KEY_Enter) {
        onSend(nullptr);
    } else {
        Popup::keyDown(key);
    }
}

ChatPopup* ChatPopup::create() {
    auto ret = new ChatPopup();
    if (ret && ret->initAnchored(400.0f, 280.0f)) {
        ret->autorelease();
        return ret;
    }
    CC_SAFE_DELETE(ret);
    return nullptr;
}

void ChatPopup::show() {
    this->setZOrder(1000);
    CCScene::get()->addChild(this);
}
