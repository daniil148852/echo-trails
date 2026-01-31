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
    
    // Trim whitespace from API key
    while (!m_apiKey.empty() && (m_apiKey.front() == ' ' || m_apiKey.front() == '\n' || m_apiKey.front() == '\r')) {
        m_apiKey.erase(0, 1);
    }
    while (!m_apiKey.empty() && (m_apiKey.back() == ' ' || m_apiKey.back() == '\n' || m_apiKey.back() == '\r')) {
        m_apiKey.pop_back();
    }
    
    log::info("[GroqAPI] Settings loaded");
    log::info("[GroqAPI] API key length: {}, starts with gsk_: {}", 
        m_apiKey.length(), 
        m_apiKey.substr(0, 4) == "gsk_" ? "yes" : "no");
    log::info("[GroqAPI] Model: {}", m_model);
}

void GroqAPI::sendMessage(
    const std::string& userMessage,
    const std::string& systemPrompt,
    GroqCallback callback
) {
    // Validate API key
    if (m_apiKey.empty()) {
        GroqResponse resp;
        resp.success = false;
        resp.error = "API key is empty. Please set it in mod settings.";
        if (callback) callback(resp);
        return;
    }
    
    if (m_apiKey.length() < 20) {
        GroqResponse resp;
        resp.success = false;
        resp.error = "API key seems too short. Check mod settings.";
        if (callback) callback(resp);
        return;
    }
    
    if (m_apiKey.substr(0, 4) != "gsk_") {
        GroqResponse resp;
        resp.success = false;
        resp.error = "API key should start with 'gsk_'. Check mod settings.";
        if (callback) callback(resp);
        return;
    }
    
    if (m_isProcessing) {
        GroqResponse resp;
        resp.success = false;
        resp.error = "Please wait for the previous request.";
        if (callback) callback(resp);
        return;
    }
    
    if (userMessage.empty()) {
        GroqResponse resp;
        resp.success = false;
        resp.error = "Message cannot be empty.";
        if (callback) callback(resp);
        return;
    }
    
    m_isProcessing = true;
    
    // Build JSON manually for reliability
    std::string messagesJson = "[";
    
    if (!systemPrompt.empty()) {
        messagesJson += "{\"role\":\"system\",\"content\":\"";
        // Escape special characters
        for (char c : systemPrompt) {
            if (c == '"') messagesJson += "\\\"";
            else if (c == '\\') messagesJson += "\\\\";
            else if (c == '\n') messagesJson += "\\n";
            else if (c == '\r') messagesJson += "\\r";
            else if (c == '\t') messagesJson += "\\t";
            else messagesJson += c;
        }
        messagesJson += "\"},";
    }
    
    messagesJson += "{\"role\":\"user\",\"content\":\"";
    for (char c : userMessage) {
        if (c == '"') messagesJson += "\\\"";
        else if (c == '\\') messagesJson += "\\\\";
        else if (c == '\n') messagesJson += "\\n";
        else if (c == '\r') messagesJson += "\\r";
        else if (c == '\t') messagesJson += "\\t";
        else messagesJson += c;
    }
    messagesJson += "\"}]";
    
    std::string bodyStr = fmt::format(
        R"({{"model":"{}","messages":{},"temperature":{},"max_tokens":{},"stream":false}})",
        m_model,
        messagesJson,
        m_temperature,
        m_maxTokens
    );
    
    log::debug("[GroqAPI] Request body: {}", bodyStr);
    
    auto req = web::WebRequest();
    req.header("Content-Type", "application/json");
    req.header("Authorization", "Bearer " + m_apiKey);
    req.bodyString(bodyStr);
    
    m_listener.bind([this, callback](web::WebTask::Event* e) {
        m_isProcessing = false;
        
        if (auto res = e->getValue()) {
            GroqResponse resp;
            int code = res->code();
            std::string responseStr = res->string().unwrapOr("");
            
            log::debug("[GroqAPI] Response code: {}", code);
            log::debug("[GroqAPI] Response body: {}", responseStr);
            
            if (code == 200) {
                auto parseResult = matjson::parse(responseStr);
                if (parseResult.isOk()) {
                    auto json = parseResult.unwrap();
                    
                    if (json.contains("choices")) {
                        auto& choices = json["choices"];
                        if (choices.isArray()) {
                            auto arr = choices.asArray();
                            if (arr.isOk() && !arr.unwrap().empty()) {
                                auto& first = arr.unwrap()[0];
                                if (first.contains("message")) {
                                    auto& msg = first["message"];
                                    if (msg.contains("content")) {
                                        resp.content = msg["content"].asString().unwrapOr("");
                                        resp.success = !resp.content.empty();
                                    }
                                }
                            }
                        }
                    }
                    
                    if (!resp.success) {
                        resp.error = "Could not parse AI response";
                    }
                } else {
                    resp.error = "JSON parse error";
                }
            } else if (code == 401) {
                resp.error = "Invalid API key (401). Please check your Groq API key in mod settings.";
                
                // Try to get more details
                auto parseResult = matjson::parse(responseStr);
                if (parseResult.isOk()) {
                    auto json = parseResult.unwrap();
                    if (json.contains("error") && json["error"].contains("message")) {
                        resp.error = json["error"]["message"].asString().unwrapOr(resp.error);
                    }
                }
            } else if (code == 400) {
                resp.error = "Bad request (400). ";
                
                auto parseResult = matjson::parse(responseStr);
                if (parseResult.isOk()) {
                    auto json = parseResult.unwrap();
                    if (json.contains("error") && json["error"].contains("message")) {
                        resp.error += json["error"]["message"].asString().unwrapOr("Unknown error");
                    }
                }
            } else if (code == 429) {
                resp.error = "Rate limited (429). Please wait a moment.";
            } else if (code == 500 || code == 502 || code == 503) {
                resp.error = fmt::format("Server error ({}). Try again later.", code);
            } else {
                resp.error = fmt::format("HTTP Error: {}", code);
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
        } else if (e->getProgress()) {
            // Still loading, do nothing
        } else {
            GroqResponse resp;
            resp.error = "Network error. Check your internet connection.";
            
            Loader::get()->queueInMainThread([callback, resp]() {
                if (callback) callback(resp);
            });
        }
    });
    
    m_listener.setFilter(req.post("https://api.groq.com/openai/v1/chat/completions"));
}
