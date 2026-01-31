#include "AIAssistant.hpp"
#include "GroqAPI.hpp"
#include <chrono>

using namespace geode::prelude;

AIAssistant* AIAssistant::s_instance = nullptr;

AIAssistant::AIAssistant()
    : m_totalDeaths(0)
    , m_deathThreshold(3)
    , m_autoTipsEnabled(true)
    , m_currentLevelName("")
    , m_currentLevelID(0)
    , m_currentDifficulty(0)
    , m_currentLevelLength(0.0f)
    , m_isInLevel(false)
    , m_maxHistorySize(20)
{
    // Initialize system prompts
    m_baseSystemPrompt = R"(You are an expert Geometry Dash assistant. You have deep knowledge of:
- All game mechanics (cube, ship, ball, UFO, wave, robot, spider, swing copter)
- Level design patterns and common obstacles
- Speedrunning techniques and optimization
- Practice strategies and muscle memory development
- Common player mistakes and how to fix them

Keep responses concise but helpful. Use GD terminology correctly.
When giving tips, be specific and actionable.)";

    m_tipSystemPrompt = R"(You are helping a Geometry Dash player who keeps dying at a specific point.
Analyze the death pattern and provide a specific, actionable tip.
Consider:
- The game mode they're in (cube, ship, etc.)
- The percentage where they're dying
- How many times they've died there
- Common obstacles at that point in levels

Give ONE clear, practical tip. Keep it under 2 sentences.)";

    m_analysisSystemPrompt = R"(Analyze the player's gameplay data and provide insights.
Look for patterns in their deaths and suggest improvements.
Be encouraging but honest about areas that need work.
Provide 2-3 specific, actionable suggestions.)";
    
    loadSettings();
}

AIAssistant::~AIAssistant() {}

AIAssistant* AIAssistant::get() {
    if (!s_instance) {
        s_instance = new AIAssistant();
    }
    return s_instance;
}

void AIAssistant::destroy() {
    if (s_instance) {
        delete s_instance;
        s_instance = nullptr;
    }
}

// ============================================================
// Level Tracking
// ============================================================

void AIAssistant::onLevelStart(GJGameLevel* level) {
    if (!level) return;
    
    m_isInLevel = true;
    m_currentLevelName = level->m_levelName;
    m_currentLevelID = level->m_levelID;
    m_currentDifficulty = static_cast<int>(level->m_difficulty);
    
    // Clear death data for new level
    clearDeathData();
    
    log::info("[AIAssistant] Level started: {} (ID: {})", m_currentLevelName, m_currentLevelID);
}

void AIAssistant::onLevelEnd() {
    m_isInLevel = false;
    log::info("[AIAssistant] Level ended");
}

void AIAssistant::onPlayerDeath(PlayerObject* player, float xPos, float percentage) {
    m_totalDeaths++;
    
    // Round position to group nearby deaths
    int roundedPos = static_cast<int>(xPos / 50.0f) * 50;
    
    if (m_deathLocations.find(roundedPos) == m_deathLocations.end()) {
        DeathInfo info;
        info.xPosition = xPos;
        info.percentage = percentage;
        info.attempts = 1;
        info.gameMode = getGameModeString(player);
        info.isPlatformer = false;  // Set from PlayLayer
        m_deathLocations[roundedPos] = info;
    } else {
        m_deathLocations[roundedPos].attempts++;
        m_deathLocations[roundedPos].percentage = percentage;  // Update to latest
        m_deathLocations[roundedPos].gameMode = getGameModeString(player);
    }
    
    log::debug("[AIAssistant] Death at {:.1f}% (x: {:.0f}), attempts: {}", 
               percentage, xPos, m_deathLocations[roundedPos].attempts);
}

void AIAssistant::onLevelComplete() {
    log::info("[AIAssistant] Level completed! Total deaths: {}", m_totalDeaths);
}

void AIAssistant::onLevelReset() {
    // Don't clear death data on reset - we want to track across attempts
    log::debug("[AIAssistant] Level reset, maintaining death data");
}

std::string AIAssistant::getGameModeString(PlayerObject* player) {
    if (!player) return "cube";
    
    if (player->m_isShip) return "ship";
    if (player->m_isBall) return "ball";
    if (player->m_isBird) return "ufo";
    if (player->m_isDart) return "wave";
    if (player->m_isRobot) return "robot";
    if (player->m_isSpider) return "spider";
    if (player->m_isSwing) return "swing copter";
    
    return "cube";
}

// ============================================================
// AI Interactions
// ============================================================

std::string AIAssistant::buildContextPrompt() {
    std::string context = fmt::format(
        "Current level: {}\n"
        "Total deaths this session: {}\n",
        m_currentLevelName.empty() ? "Unknown" : m_currentLevelName,
        m_totalDeaths
    );
    
    // Add death hotspots
    if (!m_deathLocations.empty()) {
        context += "\nDeath hotspots:\n";
        
        std::vector<std::pair<int, DeathInfo>> sorted(
            m_deathLocations.begin(), m_deathLocations.end()
        );
        std::sort(sorted.begin(), sorted.end(), 
            [](const auto& a, const auto& b) { return a.second.attempts > b.second.attempts; });
        
        int count = 0;
        for (const auto& [pos, info] : sorted) {
            if (count++ >= 5) break;  // Top 5 death spots
            context += fmt::format("- {:.1f}%: {} deaths in {} mode\n",
                info.percentage, info.attempts, info.gameMode);
        }
    }
    
    return context;
}

void AIAssistant::requestDeathTip(std::function<void(const std::string&)> callback) {
    // Find the worst death spot
    int worstPos = -1;
    int maxDeaths = 0;
    
    for (const auto& [pos, info] : m_deathLocations) {
        if (info.attempts > maxDeaths) {
            maxDeaths = info.attempts;
            worstPos = pos;
        }
    }
    
    if (worstPos < 0 || maxDeaths < m_deathThreshold) {
        callback("Keep practicing! You haven't died enough at one spot for me to give specific advice yet.");
        return;
    }
    
    const DeathInfo& deathInfo = m_deathLocations[worstPos];
    
    std::string prompt = fmt::format(
        "Player keeps dying at {:.1f}% of the level.\n"
        "Game mode: {}\n"
        "Deaths at this spot: {}\n"
        "Level: {}\n\n"
        "Give a specific tip to help them pass this part.",
        deathInfo.percentage,
        deathInfo.gameMode,
        deathInfo.attempts,
        m_currentLevelName
    );
    
    GroqAPI::get()->query(prompt, [callback](const GroqResponse& response) {
        if (response.success) {
            callback(response.content);
        } else {
            callback(fmt::format("Sorry, I couldn't get a tip: {}", response.error));
        }
    }, m_tipSystemPrompt);
}

void AIAssistant::analyzeGameplay(std::function<void(const std::string&)> callback) {
    std::string context = buildContextPrompt();
    
    std::string prompt = fmt::format(
        "{}\n\nAnalyze this gameplay session and provide helpful suggestions for improvement.",
        context
    );
    
    GroqAPI::get()->query(prompt, [callback](const GroqResponse& response) {
        if (response.success) {
            callback(response.content);
        } else {
            callback(fmt::format("Analysis failed: {}", response.error));
        }
    }, m_analysisSystemPrompt);
}

void AIAssistant::chat(const std::string& message, std::function<void(const std::string&)> callback) {
    // Add user message to history
    ConversationEntry userEntry;
    userEntry.role = "user";
    userEntry.content = message;
    userEntry.timestamp = std::chrono::duration<double>(
        std::chrono::steady_clock::now().time_since_epoch()
    ).count();
    m_conversationHistory.push_back(userEntry);
    
    // Build messages with context
    std::vector<ChatMessage> messages;
    
    // System prompt with context
    std::string systemWithContext = m_baseSystemPrompt + "\n\n" + buildContextPrompt();
    messages.emplace_back("system", systemWithContext);
    
    // Add conversation history
    for (const auto& entry : m_conversationHistory) {
        messages.emplace_back(entry.role, entry.content);
    }
    
    GroqAPI::get()->chat(messages, [this, callback](const GroqResponse& response) {
        if (response.success) {
            // Add assistant response to history
            ConversationEntry assistantEntry;
            assistantEntry.role = "assistant";
            assistantEntry.content = response.content;
            assistantEntry.timestamp = std::chrono::duration<double>(
                std::chrono::steady_clock::now().time_since_epoch()
            ).count();
            m_conversationHistory.push_back(assistantEntry);
            
            // Trim history if too long
            while (m_conversationHistory.size() > m_maxHistorySize) {
                m_conversationHistory.pop_front();
            }
            
            callback(response.content);
        } else {
            callback(fmt::format("Error: {}", response.error));
        }
    });
}

void AIAssistant::predictDifficulty(std::function<void(const std::string&)> callback) {
    std::string context = buildContextPrompt();
    
    std::string prompt = fmt::format(
        "{}\n\n"
        "Based on the player's death pattern and performance, "
        "what would you estimate the effective difficulty of this level is for them? "
        "Use Geometry Dash difficulty ratings (Easy, Normal, Hard, Harder, Insane, Easy Demon, Medium Demon, Hard Demon, Insane Demon, Extreme Demon).",
        context
    );
    
    GroqAPI::get()->query(prompt, [callback](const GroqResponse& response) {
        if (response.success) {
            callback(response.content);
        } else {
            callback(fmt::format("Couldn't predict difficulty: {}", response.error));
        }
    }, m_baseSystemPrompt);
}

void AIAssistant::getPracticeSuggestions(std::function<void(const std::string&)> callback) {
    std::string context = buildContextPrompt();
    
    std::string prompt = fmt::format(
        "{}\n\n"
        "Based on this player's death data, suggest which sections they should practice most. "
        "Give specific percentage ranges and tips for each section.",
        context
    );
    
    GroqAPI::get()->query(prompt, [callback](const GroqResponse& response) {
        if (response.success) {
            callback(response.content);
        } else {
            callback(fmt::format("Couldn't get suggestions: {}", response.error));
        }
    }, m_analysisSystemPrompt);
}

// ============================================================
// Utility
// ============================================================

void AIAssistant::clearConversation() {
    m_conversationHistory.clear();
    log::info("[AIAssistant] Conversation cleared");
}

void AIAssistant::clearDeathData() {
    m_deathLocations.clear();
    m_totalDeaths = 0;
    log::info("[AIAssistant] Death data cleared");
}

void AIAssistant::loadSettings() {
    auto mod = Mod::get();
    
    m_autoTipsEnabled = mod->getSettingValue<bool>("auto-tips");
    m_deathThreshold = static_cast<int>(mod->getSettingValue<int64_t>("death-threshold"));
    
    log::info("[AIAssistant] Settings loaded - AutoTips: {}, Threshold: {}", 
              m_autoTipsEnabled, m_deathThreshold);
}
