#pragma once

#include <Geode/Geode.hpp>
#include <Geode/utils/web.hpp>
#include <functional>
#include <vector>
#include <string>
#include <queue>
#include <mutex>

using namespace geode::prelude;

struct ChatMessage {
    std::string role;
    std::string content;
    
    ChatMessage() = default;
    ChatMessage(const std::string& r, const std::string& c) : role(r), content(c) {}
};

struct GroqResponse {
    bool success = false;
    std::string content;
    std::string error;
    int promptTokens = 0;
    int completionTokens = 0;
};

using GroqCallback = std::function<void(const GroqResponse&)>;

class GroqAPI {
private:
    static GroqAPI* s_instance;
    
    std::string m_apiKey;
    std::string m_model;
    float m_temperature;
    int m_maxTokens;
    bool m_isProcessing;
    
    EventListener<web::WebTask> m_listener;
    
    GroqAPI();
    
public:
    static GroqAPI* get();
    
    void setApiKey(const std::string& key) { m_apiKey = key; }
    std::string getApiKey() const { return m_apiKey; }
    bool hasApiKey() const { return !m_apiKey.empty() && m_apiKey.length() > 10; }
    
    void setModel(const std::string& model) { m_model = model; }
    void setTemperature(float temp) { m_temperature = temp; }
    void setMaxTokens(int tokens) { m_maxTokens = tokens; }
    
    bool isProcessing() const { return m_isProcessing; }
    
    void sendMessage(
        const std::string& userMessage,
        const std::string& systemPrompt,
        GroqCallback callback
    );
    
    void loadSettings();
};
