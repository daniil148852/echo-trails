#pragma once
#include <Geode/Geode.hpp>
#include <vector>

using namespace geode::prelude;

// Один кадр записи
struct GhostFrame {
    float xPos;           // Позиция X
    float yPos;           // Позиция Y
    float rotation;       // Поворот иконки
    int gameMode;         // 0=куб, 1=корабль, 2=шар, 3=UFO, 4=волна, 5=робот, 6=паук
    bool isUpsideDown;    // Перевёрнут ли
    bool isMini;          // Мини-режим
    bool isDashing;       // Дэш (2.2)
    bool isHolding;       // Зажата ли кнопка
    
    // Для интерполяции - время кадра
    float timestamp;
};

// Полная запись попытки
struct GhostRecording {
    std::string levelID;           // ID уровня
    int bestPercent;               // Процент этой попытки
    float totalTime;               // Длительность записи
    std::vector<GhostFrame> frames;
    
    bool isValid() const {
        return !frames.empty() && !levelID.empty();
    }
    
    void clear() {
        frames.clear();
        bestPercent = 0;
        totalTime = 0.f;
    }
};

// Настройки мода
struct GhostSettings {
    bool enabled = true;
    float ghostOpacity = 0.4f;      // Прозрачность (0.0 - 1.0)
    bool showOnlyIfBetter = false;  // Показывать только если призрак впереди
    bool recordPractice = false;    // Записывать в практике
    ccColor3B ghostColor = {100, 200, 255}; // Цвет призрака (голубой)
};
