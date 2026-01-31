#pragma once

#include <Geode/Geode.hpp>
#include <Geode/utils/web.hpp>
#include <functional>
#include <vector>
#include <string>
#include <queue>

using namespace geode::prelude;

/**
 * @brief Represents a single message in a conversation
 */
struct ChatMessage {
    std::string role;     // "system", "user", or "assistant"
    std::string content;
    
    ChatMessage(const std::string& r, const std::string& c) 
        : role(r), content(c) {}
};

/**
 * @brief Response from Groq API
 */
struct GroqResponse {
    bool success = false;
    std::string content;
    std::string error;
    int promptTokens = 0;
    int completionTokens = 0;
    double responseTimeMs = 0.0;
};

/**
 * @brief Callback type for API responses
 */
using GroqCallback = std::function<void(const GroqResponse&)>;

/**
 * @brief Request in the queue
 */
struct QueuedRequest {
    std::vector<ChatMessage> messages;
    GroqCallback callback;
    std::string model;
    float temperature;
    int maxTokens;
};

/**
 * @brief Handles all communication with Groq API
 */
class GroqAPI {
private:
    static GroqAPI* s_instance;
    
    std::string m_apiKey;
    std::string m_defaultModel;
    float m_defaultTemperature;
    int m_defaultMaxTokens;
    
    std::string m_baseUrl;
    
    bool m_isProcessing;
    std::queue<QueuedRequest> m_requestQueue;
    
    EventListener<web::WebTask> m_webListener;
    
    // Rate limiting
    double m_lastRequestTime;
    double m_minRequestInterval;  // Minimum time between requests
    
    GroqAPI();
    ~GroqAPI();
    
    void processQueue();
    void executeRequest(const QueuedRequest& request);
    matjson::Value buildRequestBody(const QueuedRequest& request);
    GroqResponse parseResponse(const std::string& jsonStr);
    
public:
    static GroqAPI* get();
    static void destroy();
    
    // ==================== Configuration ====================
    
    void setApiKey(const std::string& key);
    std::string getApiKey() const { return m_apiKey; }
    bool hasApiKey() const { return !m_apiKey.empty(); }
    
    void setDefaultModel(const std::string& model) { m_defaultModel = model; }
    std::string getDefaultModel() const { return m_defaultModel; }
    
    void setDefaultTemperature(float temp) { m_defaultTemperature = temp; }
    float getDefaultTemperature() const { return m_defaultTemperature; }
    
    void setDefaultMaxTokens(int tokens) { m_defaultMaxTokens = tokens; }
    int getDefaultMaxTokens() const { return m_defaultMaxTokens; }
    
    // ==================== API Calls ====================
    
    /**
     * @brief Send a chat completion request
     * @param messages The conversation messages
     * @param callback Function to call with the response
     * @param model Override default model (optional)
     * @param temperature Override default temperature (optional)
     * @param maxTokens Override default max tokens (optional)
     */
    void chat(
        const std::vector<ChatMessage>& messages,
        GroqCallback callback,
        const std::string& model = "",
        float temperature = -1.0f,
        int maxTokens = -1
    );
    
    /**
     * @brief Simple single-message query
     * @param prompt The user's message
     * @param callback Function to call with the response
     * @param systemPrompt Optional system prompt
     */
    void query(
        const std::string& prompt,
        GroqCallback callback,
        const std::string& systemPrompt = ""
    );
    
    /**
     * @brief Cancel all pending requests
     */
    void cancelAll();
    
    /**
     * @brief Check if currently processing a request
     */
    bool isProcessing() const { return m_isProcessing; }
    
    /**
     * @brief Get number of queued requests
     */
    size_t getQueueSize() const { return m_requestQueue.size(); }
    
    // ==================== Settings ====================
    
    void loadSettings();
    void saveSettings();
};
