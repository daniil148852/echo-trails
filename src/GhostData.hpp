#pragma once

#include <Geode/Geode.hpp>
#include <vector>
#include <string>

using namespace geode::prelude;

namespace EchoTrails {

// Игровые режимы
enum class GameMode : uint8_t {
    Cube = 0,
    Ship = 1,
    Ball = 2,
    UFO = 3,
    Wave = 4,
    Robot = 5,
    Spider = 6,
    Swing = 7
};

// Данные одного кадра
struct GhostFrame {
    float posX;
    float posY;
    float rotation;
    float scaleX;
    float scaleY;
    GameMode gameMode;
    bool isUpsideDown;
    bool isVisible;
    bool isDashing;      // Для свинг-копоптера
    bool isMini;
    bool isDual;
    bool isPlayer2;      // Для dual mode
    
    // Сериализация
    void serialize(std::vector<uint8_t>& buffer) const;
    static GhostFrame deserialize(const uint8_t* data, size_t& offset);
};

// Данные игрока для визуала
struct PlayerVisuals {
    int iconID;
    int shipID;
    int ballID;
    int ufoID;
    int waveID;
    int robotID;
    int spiderID;
    int swingID;
    int color1;
    int color2;
    int glowColor;
    bool hasGlow;
};

// Полная запись прохождения
struct GhostRecording {
    std::string levelID;
    std::string levelName;
    int bestPercent;
    float totalTime;
    uint32_t frameCount;
    float fps;  // Для интерполяции
    PlayerVisuals visuals;
    std::vector<GhostFrame> frames;
    std::vector<GhostFrame> player2Frames;  // Для dual mode
    
    // Время записи
    int64_t timestamp;
    
    bool isComplete() const { return bestPercent >= 100; }
    
    // Файловые операции
    bool saveToFile(const std::filesystem::path& path) const;
    static std::optional<GhostRecording> loadFromFile(const std::filesystem::path& path);
};

// Интерполяция между кадрами
GhostFrame interpolateFrames(const GhostFrame& a, const GhostFrame& b, float t);

} // namespace EchoTrails
