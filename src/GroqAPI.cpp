#include "GroqAPI.hpp"
#include <chrono>

using namespace geode::prelude;

GroqAPI* GroqAPI::s_instance = nullptr;

GroqAPI::GroqAPI()
    : m_apiKey("")
    , m_defaultModel("llama-3.3-70b-versatile")
    , m_defaultTemperature(0.7f)
    , m_defaultMaxTokens(256)
    , m_baseUrl("https://api.groq.com/openai/v1/chat/completions")
    , m_isProcessing(false)
    , m_lastRequestTime(0.0)
    , m_minRequestInterval(0.1)  // 100ms between requests
{
    loadSettings();
}

GroqAPI::~GroqAPI() {
    cancelAll();
}

GroqAPI* GroqAPI::get() {
    if (!s_instance) {
        s_instance = new GroqAPI();
    }
    return s_instance;
}

void GroqAPI::destroy() {
    if (s_instance) {
        delete s_instance;
        s_instance = nullptr;
    }
}

// ============================================================
// Configuration
// ============================================================

void GroqAPI::setApiKey(const std::string& key) {
    m_apiKey = key;
    log::info("[GroqAPI] API key set (length: {})", key.length());
}

void GroqAPI::loadSettings() {
    auto mod = Mod::get();
    
    m_apiKey = mod->getSettingValue<std::string>("api-key");
    m_defaultModel = mod->getSettingValue<std::string>("model");
    m_defaultTemperature = static_cast<float>(mod->getSettingValue<double>("temperature"));
    m_defaultMaxTokens = static_cast<int>(mod->getSettingValue<int64_t>("max-tokens"));
    
    log::info("[GroqAPI] Settings loaded - Model: {}, Temp: {:.2f}", 
              m_defaultModel, m_defaultTemperature);
}

void GroqAPI::saveSettings() {
    auto mod = Mod::get();
    
    mod->setSettingValue<std::string>("api-key", m_apiKey);
    mod->setSettingValue<std::string>("model", m_defaultModel);
    mod->setSettingValue<double>("temperature", m_defaultTemperature);
    mod->setSettingValue<int64_t>("max-tokens", m_defaultMaxTokens);
}

// ============================================================
// API Calls
// ============================================================

void GroqAPI::chat(
    const std::vector<ChatMessage>& messages,
    GroqCallback callback,
    const std::string& model,
    float temperature,
    int maxTokens
) {
    if (!hasApiKey()) {
        GroqResponse response;
        response.success = false;
        response.error = "API key not set. Please configure your Groq API key in mod settings.";
        callback(response);
        return;
    }
    
    QueuedRequest request;
    request.messages = messages;
    request.callback = callback;
    request.model = model.empty() ? m_defaultModel : model;
    request.temperature = temperature < 0 ? m_defaultTemperature : temperature;
    request.maxTokens = maxTokens < 0 ? m_defaultMaxTokens : maxTokens;
    
    m_requestQueue.push(request);
    
    if (!m_isProcessing) {
        processQueue();
    }
}

void GroqAPI::query(
    const std::string& prompt,
    GroqCallback callback,
    const std::string& systemPrompt
) {
    std::vector<ChatMessage> messages;
    
    if (!systemPrompt.empty()) {
        messages.emplace_back("system", systemPrompt);
    }
    
    messages.emplace_back("user", prompt);
    
    chat(messages, callback);
}

void GroqAPI::cancelAll() {
    while (!m_requestQueue.empty()) {
        m_requestQueue.pop();
    }
    m_isProcessing = false;
}

// ============================================================
// Internal Processing
// ============================================================

void GroqAPI::processQueue() {
    if (m_requestQueue.empty()) {
        m_isProcessing = false;
        return;
    }
    
    m_isProcessing = true;
    
    // Rate limiting
    auto now = std::chrono::duration<double>(
        std::chrono::steady_clock::now().time_since_epoch()
    ).count();
    
    double timeSinceLastRequest = now - m_lastRequestTime;
    
    if (timeSinceLastRequest < m_minRequestInterval) {
        // Schedule delayed execution
        double delay = m_minRequestInterval - timeSinceLastRequest;
        
        Loader::get()->queueInMainThread([this]() {
            if (!m_requestQueue.empty()) {
                QueuedRequest request = m_requestQueue.front();
                m_requestQueue.pop();
                executeRequest(request);
            }
        });
        return;
    }
    
    QueuedRequest request = m_requestQueue.front();
    m_requestQueue.pop();
    executeRequest(request);
}

void GroqAPI::executeRequest(const QueuedRequest& request) {
    auto startTime = std::chrono::steady_clock::now();
    
    m_lastRequestTime = std::chrono::duration<double>(
        startTime.time_since_epoch()
    ).count();
    
    matjson::Value body = buildRequestBody(request);
    std::string bodyStr = body.dump();
    
    log::debug("[GroqAPI] Sending request to Groq API...");
    log::debug("[GroqAPI] Model: {}, Messages: {}", request.model, request.messages.size());
    
    auto webRequest = web::WebRequest();
    webRequest.header("Content-Type", "application/json");
    webRequest.header("Authorization", fmt::format("Bearer {}", m_apiKey));
    webRequest.bodyString(bodyStr);
    
    auto callback = request.callback;
    
    m_webListener.bind([this, callback, startTime](web::WebTask::Event* e) {
        if (auto res = e->getValue()) {
            auto endTime = std::chrono::steady_clock::now();
            double elapsed = std::chrono::duration<double, std::milli>(endTime - startTime).count();
            
            if (res->ok()) {
                std::string responseStr = res->string().unwrapOr("");
                log::debug("[GroqAPI] Response received in {:.0f}ms", elapsed);
                
                GroqResponse response = parseResponse(responseStr);
                response.responseTimeMs = elapsed;
                
                Loader::get()->queueInMainThread([callback, response]() {
                    callback(response);
                });
            } else {
                log::error("[GroqAPI] Request failed with code: {}", res->code());
                
                GroqResponse response;
                response.success = false;
                response.error = fmt::format("HTTP Error {}: {}", 
                    res->code(), 
                    res->string().unwrapOr("Unknown error"));
                response.responseTimeMs = elapsed;
                
                // Try to parse error from response
                std::string responseStr = res->string().unwrapOr("");
                if (!responseStr.empty()) {
                    try {
                        auto json = matjson::parse(responseStr);
                        if (json.contains("error") && json["error"].contains("message")) {
                            response.error = json["error"]["message"].asString().unwrapOr(response.error);
                        }
                    } catch (...) {}
                }
                
                Loader::get()->queueInMainThread([callback, response]() {
                    callback(response);
                });
            }
            
            // Process next request in queue
            Loader::get()->queueInMainThread([this]() {
                processQueue();
            });
        } else if (e->isCancelled()) {
            log::warn("[GroqAPI] Request was cancelled");
            
            GroqResponse response;
            response.success = false;
            response.error = "Request was cancelled";
            
            Loader::get()->queueInMainThread([callback, response]() {
                callback(response);
            });
            
            Loader::get()->queueInMainThread([this]() {
                processQueue();
            });
        }
    });
    
    m_webListener.setFilter(webRequest.post(m_baseUrl));
}

matjson::Value GroqAPI::buildRequestBody(const QueuedRequest& request) {
    matjson::Value body;
    
    body["model"] = request.model;
    body["temperature"] = request.temperature;
    body["max_tokens"] = request.maxTokens;
    body["stream"] = false;
    
    matjson::Value messages = matjson::Value::array();
    
    for (const auto& msg : request.messages) {
        matjson::Value msgObj;
        msgObj["role"] = msg.role;
        msgObj["content"] = msg.content;
        messages.push(msgObj);
    }
    
    body["messages"] = messages;
    
    return body;
}

GroqResponse GroqAPI::parseResponse(const std::string& jsonStr) {
    GroqResponse response;
    
    try {
        auto json = matjson::parse(jsonStr);
        
        if (json.contains("choices") && json["choices"].isArray()) {
            auto choices = json["choices"].asArray().unwrap();
            if (!choices.empty()) {
                auto firstChoice = choices[0];
                if (firstChoice.contains("message") && 
                    firstChoice["message"].contains("content")) {
                    response.content = firstChoice["message"]["content"].asString().unwrapOr("");
                    response.success = true;
                }
            }
        }
        
        if (json.contains("usage")) {
            auto usage = json["usage"];
            response.promptTokens = usage["prompt_tokens"].asInt().unwrapOr(0);
            response.completionTokens = usage["completion_tokens"].asInt().unwrapOr(0);
        }
        
        if (!response.success && json.contains("error")) {
            response.error = json["error"]["message"].asString().unwrapOr("Unknown API error");
        }
        
    } catch (const std::exception& e) {
        response.success = false;
        response.error = fmt::format("Failed to parse response: {}", e.what());
        log::error("[GroqAPI] Parse error: {}", e.what());
    }
    
    return response;
}
