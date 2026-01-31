#include "GroqAPI.hpp"

using namespace geode::prelude;

GroqAPI* GroqAPI::s_instance = nullptr;

GroqAPI::GroqAPI()
    : m_apiKey("")
    , m_model("llama-3.3-70b-versatile")
    , m_temperature(0.7f)
    , m_maxTokens(256)
    , m_isProcessing(false)
{
}

GroqAPI* GroqAPI::get() {
    if (!s_instance) {
        s_instance = new GroqAPI();
    }
    return s_instance;
}

void GroqAPI::loadSettings() {
    auto mod = Mod::get();
    if (!mod) return;
    
    m_apiKey = mod->getSettingValue<std::string>("api-key");
    m_model = mod->getSettingValue<std::string>("model");
    m_temperature = static_cast<float>(mod->getSettingValue<double>("temperature"));
    m_maxTokens = static_cast<int>(mod->getSettingValue<int64_t>("max-tokens"));
    
    log::info("[GroqAPI] Loaded settings, API key length: {}", m_apiKey.length());
}

void GroqAPI::sendMessage(
    const std::string& userMessage,
    const std::string& systemPrompt,
    GroqCallback callback
) {
    if (!hasApiKey()) {
        GroqResponse resp;
        resp.success = false;
        resp.error = "API key not configured. Go to mod settings.";
        if (callback) callback(resp);
        return;
    }
    
    if (m_isProcessing) {
        GroqResponse resp;
        resp.success = false;
        resp.error = "Please wait for the previous request to complete.";
        if (callback) callback(resp);
        return;
    }
    
    m_isProcessing = true;
    
    // Build request body
    matjson::Value body;
    body["model"] = m_model;
    body["temperature"] = m_temperature;
    body["max_tokens"] = m_maxTokens;
    body["stream"] = false;
    
    matjson::Value messages = matjson::Value::array();
    
    if (!systemPrompt.empty()) {
        matjson::Value sysMsg;
        sysMsg["role"] = "system";
        sysMsg["content"] = systemPrompt;
        messages.push(sysMsg);
    }
    
    matjson::Value userMsg;
    userMsg["role"] = "user";
    userMsg["content"] = userMessage;
    messages.push(userMsg);
    
    body["messages"] = messages;
    
    std::string bodyStr = body.dump();
    
    log::debug("[GroqAPI] Sending request...");
    
    auto req = web::WebRequest();
    req.header("Content-Type", "application/json");
    req.header("Authorization", fmt::format("Bearer {}", m_apiKey));
    req.bodyString(bodyStr);
    
    m_listener.bind([this, callback](web::WebTask::Event* e) {
        m_isProcessing = false;
        
        if (auto res = e->getValue()) {
            GroqResponse resp;
            
            if (res->ok()) {
                std::string responseStr = res->string().unwrapOr("");
                
                auto parseResult = matjson::parse(responseStr);
                if (parseResult.isOk()) {
                    auto json = parseResult.unwrap();
                    
                    if (json.contains("choices")) {
                        auto choicesResult = json["choices"].asArray();
                        if (choicesResult.isOk()) {
                            auto choices = choicesResult.unwrap();
                            if (!choices.empty()) {
                                auto& first = choices[0];
                                if (first.contains("message") && first["message"].contains("content")) {
                                    resp.content = first["message"]["content"].asString().unwrapOr("");
                                    resp.success = !resp.content.empty();
                                }
                            }
                        }
                    }
                    
                    if (!resp.success && json.contains("error")) {
                        if (json["error"].contains("message")) {
                            resp.error = json["error"]["message"].asString().unwrapOr("Unknown error");
                        }
                    }
                } else {
                    resp.error = "Failed to parse API response";
                }
            } else {
                resp.error = fmt::format("HTTP Error: {}", res->code());
            }
            
            if (!resp.success && resp.error.empty()) {
                resp.error = "Unknown error occurred";
            }
            
            Loader::get()->queueInMainThread([callback, resp]() {
                if (callback) callback(resp);
            });
        } else if (e->isCancelled()) {
            GroqResponse resp;
            resp.error = "Request cancelled";
            
            Loader::get()->queueInMainThread([callback, resp]() {
                if (callback) callback(resp);
            });
        }
    });
    
    m_listener.setFilter(req.post("https://api.groq.com/openai/v1/chat/completions"));
}
