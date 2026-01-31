#pragma once

#include <Geode/Geode.hpp>
#include "GroqAPI.hpp"
#include <map>
#include <deque>

using namespace geode::prelude;

/**
 * @brief Death location info for analysis
 */
struct DeathInfo {
    float xPosition;
    float percentage;
    int attempts;
    std::string gameMode;  // cube, ship, ball, etc.
    bool isPlatformer;
    std::string lastTip;
};

/**
 * @brief Conversation history entry
 */
struct ConversationEntry {
    std::string role;
    std::string content;
    double timestamp;
};

/**
 * @brief Main AI Assistant manager
 */
class AIAssistant {
private:
    static AIAssistant* s_instance;
    
    // Death tracking
    std::map<int, DeathInfo> m_deathLocations;  // Key: rounded X position
    int m_totalDeaths;
    int m_deathThreshold;
    bool m_autoTipsEnabled;
    
    // Current level info
    std::string m_currentLevelName;
    int m_currentLevelID;
    int m_currentDifficulty;
    float m_currentLevelLength;
    bool m_isInLevel;
    
    // Conversation
    std::deque<ConversationEntry> m_conversationHistory;
    size_t m_maxHistorySize;
    
    // System prompts
    std::string m_baseSystemPrompt;
    std::string m_tipSystemPrompt;
    std::string m_analysisSystemPrompt;
    
    AIAssistant();
    ~AIAssistant();
    
    std::string buildContextPrompt();
    std::string getGameModeString(PlayerObject* player);
    
public:
    static AIAssistant* get();
    static void destroy();
    
    // ==================== Level Tracking ====================
    
    void onLevelStart(GJGameLevel* level);
    void onLevelEnd();
    void onPlayerDeath(PlayerObject* player, float xPos, float percentage);
    void onLevelComplete();
    void onLevelReset();
    
    // ==================== AI Interactions ====================
    
    /**
     * @brief Get a tip for the current death location
     */
    void requestDeathTip(std::function<void(const std::string&)> callback);
    
    /**
     * @brief Analyze current gameplay and provide suggestions
     */
    void analyzeGameplay(std::function<void(const std::string&)> callback);
    
    /**
     * @brief General chat with the AI
     */
    void chat(const std::string& message, std::function<void(const std::string&)> callback);
    
    /**
     * @brief Get level difficulty prediction
     */
    void predictDifficulty(std::function<void(const std::string&)> callback);
    
    /**
     * @brief Get practice mode suggestions
     */
    void getPracticeSuggestions(std::function<void(const std::string&)> callback);
    
    // ==================== Configuration ====================
    
    void setAutoTips(bool enabled) { m_autoTipsEnabled = enabled; }
    bool getAutoTips() const { return m_autoTipsEnabled; }
    
    void setDeathThreshold(int threshold) { m_deathThreshold = threshold; }
    int getDeathThreshold() const { return m_deathThreshold; }
    
    void clearConversation();
    void clearDeathData();
    
    // ==================== Getters ====================
    
    int getTotalDeaths() const { return m_totalDeaths; }
    std::string getCurrentLevelName() const { return m_currentLevelName; }
    bool isInLevel() const { return m_isInLevel; }
    
    const std::deque<ConversationEntry>& getConversationHistory() const {
        return m_conversationHistory;
    }
    
    // ==================== Settings ====================
    
    void loadSettings();
};
