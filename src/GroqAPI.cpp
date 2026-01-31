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

std::string GroqAPI::fixApiKey(const std::string& key) {
    std::string fixed = key;
    
    // Убираем пробелы
    while (!fixed.empty() && (fixed.front() == ' ' || fixed.front() == '\n' || fixed.front() == '\r')) {
        fixed.erase(0, 1);
    }
    while (!fixed.empty() && (fixed.back() == ' ' || fixed.back() == '\n' || fixed.back() == '\r')) {
        fixed.pop_back();
    }
    
    // GD убирает подчёркивание, поэтому "gskXXX" -> "gsk_XXX"
    if (fixed.length() > 3 && fixed.substr(0, 3) == "gsk" && fixed[3] != '_') {
        fixed = "gsk_" + fixed.substr(3);
    }
    
    return fixed;
}

void GroqAPI::loadSettings() {
    auto mod = Mod::get();
    if (!mod) return;
    
    std::string rawKey = mod->getSettingValue<std::string>("api-key");
    m_apiKey = fixApiKey(rawKey);
    
    m_model = mod->getSettingValue<std::string>("model");
    m_temperature = static_cast<float>(mod->getSettingValue<double>("temperature"));
    m_maxTokens = static_cast<int>(mod->getSettingValue<int64_t>("max-tokens"));
    
    log::info("[GroqAPI] API key fixed: {} -> {}", 
        rawKey.substr(0, 7) + "...", 
        m_apiKey.substr(0, 7) + "...");
}

void GroqAPI::setApiKey(const std::string& key) {
    m_apiKey = fixApiKey(key);
}

void GroqAPI::sendMessage(
    const std::string& userMessage,
    const std::string& systemPrompt,
    GroqCallback callback
) {
    if (m_apiKey.empty()) {
        GroqResponse resp;
        resp.error = "API key is empty. Set it in mod settings.";
        if (callback) callback(resp);
        return;
    }
    
    if (m_apiKey.length() < 20) {
        GroqResponse resp;
        resp.error = "API key too short. Check mod settings.";
        if (callback) callback(resp);
        return;
    }
    
    if (m_isProcessing) {
        GroqResponse resp;
        resp.error = "Please wait for previous request.";
        if (callback) callback(resp);
        return;
    }
    
    if (userMessage.empty()) {
        GroqResponse resp;
        resp.error = "Message cannot be empty.";
        if (callback) callback(resp);
        return;
    }
    
    m_isProcessing = true;
    
    // Escape JSON strings
    auto escapeJson = [](const std::string& s) -> std::string {
        std::string result;
        for (char c : s) {
            switch (c) {
                case '"': result += "\\\""; break;
                case '\\': result += "\\\\"; break;
                case '\n': result += "\\n"; break;
                case '\r': result += "\\r"; break;
                case '\t': result += "\\t"; break;
                default: result += c; break;
            }
        }
        return result;
    };
    
    std::string messagesJson = "[";
    
    if (!systemPrompt.empty()) {
        messagesJson += "{\"role\":\"system\",\"content\":\"" + escapeJson(systemPrompt) + "\"},";
    }
    
    messagesJson += "{\"role\":\"user\",\"content\":\"" + escapeJson(userMessage) + "\"}]";
    
    std::string bodyStr = fmt::format(
        R"({{"model":"{}","messages":{},"temperature":{},"max_tokens":{}}})",
        m_model,
        messagesJson,
        m_temperature,
        m_maxTokens
    );
    
    log::debug("[GroqAPI] Sending to Groq...");
    
    auto req = web::WebRequest();
    req.header("Content-Type", "application/json");
    req.header("Authorization", "Bearer " + m_apiKey);
    req.bodyString(bodyStr);
    
    m_listener.bind([this, callback](web::WebTask::Event* e) {
        m_isProcessing = false;
        
        if (auto res = e->getValue()) {
            GroqResponse resp;
            int code = res->code();
            std::string body = res->string().unwrapOr("");
            
            log::debug("[GroqAPI] Code: {}", code);
            
            if (code == 200) {
                auto parsed = matjson::parse(body);
                if (parsed.isOk()) {
                    auto json = parsed.unwrap();
                    if (json.contains("choices")) {
                        auto arr = json["choices"].asArray();
                        if (arr.isOk() && !arr.unwrap().empty()) {
                            auto& first = arr.unwrap()[0];
                            if (first.contains("message") && first["message"].contains("content")) {
                                resp.content = first["message"]["content"].asString().unwrapOr("");
                                resp.success = !resp.content.empty();
                            }
                        }
                    }
                }
                if (!resp.success) resp.error = "Failed to parse response";
            } else if (code == 401) {
                resp.error = "Invalid API key (401). Check your key.";
            } else if (code == 400) {
                resp.error = "Bad request (400).";
                auto parsed = matjson::parse(body);
                if (parsed.isOk()) {
                    auto json = parsed.unwrap();
                    if (json.contains("error") && json["error"].contains("message")) {
                        resp.error += " " + json["error"]["message"].asString().unwrapOr("");
                    }
                }
            } else if (code == 429) {
                resp.error = "Rate limit. Wait a bit.";
            } else {
                resp.error = fmt::format("Error: {}", code);
            }
            
            Loader::get()->queueInMainThread([callback, resp]() {
                if (callback) callback(resp);
            });
        } else if (e->isCancelled()) {
            GroqResponse resp;
            resp.error = "Cancelled";
            Loader::get()->queueInMainThread([callback, resp]() {
                if (callback) callback(resp);
            });
        }
    });
    
    m_listener.setFilter(req.post("https://api.groq.com/openai/v1/chat/completions"));
}
