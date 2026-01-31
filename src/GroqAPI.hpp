#pragma once

#include <Geode/Geode.hpp>
#include <Geode/utils/web.hpp>
#include <functional>
#include <string>

using namespace geode::prelude;

struct GroqResponse {
    bool success = false;
    std::string content;
    std::string error;
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
    
    // Исправляет ключ (добавляет _ после gsk если GD его убрал)
    static std::string fixApiKey(const std::string& key);
    
public:
    static GroqAPI* get();
    
    void setApiKey(const std::string& key);
    std::string getApiKey() const { return m_apiKey; }
    bool hasApiKey() const { return m_apiKey.length() > 20; }
    
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
