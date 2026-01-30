#pragma once
#include "GhostData.hpp"

class GhostRecorder {
private:
    GhostRecording m_currentRecording;
    GhostRecording m_bestRecording;
    bool m_isRecording = false;
    float m_recordTimer = 0.f;
    
    // Записываем каждые N секунд (для оптимизации)
    const float RECORD_INTERVAL = 1.f / 60.f; // 60 FPS
    float m_timeSinceLastFrame = 0.f;

    // Singleton
    static GhostRecorder* s_instance;
    
public:
    static GhostRecorder* get() {
        if (!s_instance) s_instance = new GhostRecorder();
        return s_instance;
    }
    
    // Начать запись новой попытки
    void startRecording(const std::string& levelID);
    
    // Записать кадр
    void recordFrame(PlayerObject* player, float dt);
    
    // Остановить запись (смерть или завершение)
    void stopRecording(int percent);
    
    // Получить лучшую запись для воспроизведения
    GhostRecording& getBestRecording() { return m_bestRecording; }
    
    // Проверить есть ли запись для уровня
    bool hasRecording() const { return m_bestRecording.isValid(); }
    
    // Сохранить/загрузить с диска
    void saveToFile(const std::string& levelID);
    void loadFromFile(const std::string& levelID);
    
    // Очистить записи
    void clearRecordings();
};
