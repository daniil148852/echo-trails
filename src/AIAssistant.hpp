#pragma once

#include <Geode/Geode.hpp>
#include <map>
#include <deque>
#include <string>

using namespace geode::prelude;

struct DeathData {
    float percentage = 0.0f;
    int count = 0;
    std::string gameMode;
};

struct ConvoMessage {
    std::string role;
    std::string content;
};

class AIAssistant {
private:
    static AIAssistant* s_instance;
    
    std::map<int, DeathData> m_deathSpots;
    std::deque<ConvoMessage> m_history;
    std::string m_levelName;
    int m_totalDeaths;
    bool m_inLevel;
    int m_deathThreshold;
    bool m_autoTips;
    
    AIAssistant();
    
public:
    static AIAssistant* get();
    
    void onLevelStart(const std::string& levelName);
    void onLevelEnd();
    void onDeath(float percentage, const std::string& gameMode);
    void resetDeaths();
    
    void chat(const std::string& message, std::function<void(const std::string&)> callback);
    void getTip(std::function<void(const std::string&)> callback);
    void analyze(std::function<void(const std::string&)> callback);
    
    void clearHistory();
    const std::deque<ConvoMessage>& getHistory() const { return m_history; }
    
    int getTotalDeaths() const { return m_totalDeaths; }
    std::string getLevelName() const { return m_levelName; }
    bool isInLevel() const { return m_inLevel; }
    
    void loadSettings();
};
