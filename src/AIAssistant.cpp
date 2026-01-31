#include "AIAssistant.hpp"
#include "GroqAPI.hpp"

using namespace geode::prelude;

AIAssistant* AIAssistant::s_instance = nullptr;

AIAssistant::AIAssistant()
    : m_totalDeaths(0)
    , m_inLevel(false)
    , m_deathThreshold(3)
    , m_autoTips(true)
{
}

AIAssistant* AIAssistant::get() {
    if (!s_instance) {
        s_instance = new AIAssistant();
    }
    return s_instance;
}

void AIAssistant::loadSettings() {
    auto mod = Mod::get();
    if (!mod) return;
    
    m_autoTips = mod->getSettingValue<bool>("auto-tips");
    m_deathThreshold = static_cast<int>(mod->getSettingValue<int64_t>("death-threshold"));
}

void AIAssistant::onLevelStart(const std::string& levelName) {
    m_levelName = levelName;
    m_inLevel = true;
    resetDeaths();
    log::info("[AIAssistant] Level started: {}", levelName);
}

void AIAssistant::onLevelEnd() {
    m_inLevel = false;
}

void AIAssistant::onDeath(float percentage, const std::string& gameMode) {
    m_totalDeaths++;
    
    int key = static_cast<int>(percentage * 10);  // Group by 0.1%
    
    if (m_deathSpots.find(key) == m_deathSpots.end()) {
        m_deathSpots[key] = DeathData{percentage, 1, gameMode};
    } else {
        m_deathSpots[key].count++;
        m_deathSpots[key].gameMode = gameMode;
    }
    
    log::debug("[AIAssistant] Death at {:.1f}%, total: {}", percentage, m_totalDeaths);
}

void AIAssistant::resetDeaths() {
    m_deathSpots.clear();
    m_totalDeaths = 0;
}

void AIAssistant::chat(const std::string& message, std::function<void(const std::string&)> callback) {
    // Add to history
    m_history.push_back({"user", message});
    if (m_history.size() > 20) {
        m_history.pop_front();
    }
    
    std::string systemPrompt = 
        "You are a helpful Geometry Dash assistant. "
        "You know about all game modes, mechanics, and can give gameplay tips. "
        "Keep responses concise (2-3 sentences max).";
    
    if (!m_levelName.empty()) {
        systemPrompt += fmt::format(" The player is playing level: {}.", m_levelName);
    }
    
    GroqAPI::get()->sendMessage(message, systemPrompt, [this, callback](const GroqResponse& resp) {
        std::string result;
        if (resp.success) {
            result = resp.content;
            m_history.push_back({"assistant", result});
            if (m_history.size() > 20) {
                m_history.pop_front();
            }
        } else {
            result = "Error: " + resp.error;
        }
        
        if (callback) callback(result);
    });
}

void AIAssistant::getTip(std::function<void(const std::string&)> callback) {
    // Find worst death spot
    int worstKey = -1;
    int maxDeaths = 0;
    
    for (const auto& [key, data] : m_deathSpots) {
        if (data.count > maxDeaths) {
            maxDeaths = data.count;
            worstKey = key;
        }
    }
    
    if (worstKey < 0 || maxDeaths < 2) {
        if (callback) callback("Keep playing! I need more data to give you a tip.");
        return;
    }
    
    const DeathData& data = m_deathSpots[worstKey];
    
    std::string prompt = fmt::format(
        "I keep dying at {:.1f}% in {} mode ({} deaths). Give me ONE short tip.",
        data.percentage, data.gameMode, data.count
    );
    
    std::string systemPrompt = 
        "You are a Geometry Dash coach. Give ONE specific, actionable tip. "
        "Be concise (1-2 sentences). Consider the game mode and percentage.";
    
    GroqAPI::get()->sendMessage(prompt, systemPrompt, [callback](const GroqResponse& resp) {
        std::string result = resp.success ? resp.content : ("Error: " + resp.error);
        if (callback) callback(result);
    });
}

void AIAssistant::analyze(std::function<void(const std::string&)> callback) {
    if (m_deathSpots.empty()) {
        if (callback) callback("No death data to analyze. Play the level first!");
        return;
    }
    
    std::string deathInfo = fmt::format("Level: {}, Total deaths: {}\nDeath spots:\n", 
        m_levelName.empty() ? "Unknown" : m_levelName, m_totalDeaths);
    
    std::vector<std::pair<int, DeathData>> sorted(m_deathSpots.begin(), m_deathSpots.end());
    std::sort(sorted.begin(), sorted.end(), 
        [](const auto& a, const auto& b) { return a.second.count > b.second.count; });
    
    int count = 0;
    for (const auto& [key, data] : sorted) {
        if (count++ >= 5) break;
        deathInfo += fmt::format("- {:.1f}%: {} deaths ({})\n", 
            data.percentage, data.count, data.gameMode);
    }
    
    std::string prompt = deathInfo + "\nAnalyze my performance and give 2-3 tips.";
    
    std::string systemPrompt = 
        "You are a Geometry Dash coach analyzing player performance. "
        "Give specific, actionable advice based on the death pattern. "
        "Be encouraging but direct. Keep it short (3-4 sentences).";
    
    GroqAPI::get()->sendMessage(prompt, systemPrompt, [callback](const GroqResponse& resp) {
        std::string result = resp.success ? resp.content : ("Error: " + resp.error);
        if (callback) callback(result);
    });
}

void AIAssistant::clearHistory() {
    m_history.clear();
}
