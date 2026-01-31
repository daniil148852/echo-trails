#pragma once

#include <Geode/Geode.hpp>
#include <vector>

using namespace geode::prelude;

// Состояние игрока в один момент времени
struct PlayerState {
    // Позиция и трансформация
    CCPoint position;
    float rotation;
    float scale;
    double yVelocity;
    
    // Флаги состояния
    bool isUpsideDown;
    bool isOnGround;
    bool isDashing;
    bool isHidden;
    bool isLocked;
    
    // Игровой режим
    bool isShip;
    bool isBall;
    bool isBird;    // UFO
    bool isDart;    // Wave  
    bool isRobot;
    bool isSpider;
    bool isSwing;
    
    // Визуальные
    float iconRotation;
    bool flipX;
    bool flipY;
};

// Полное состояние кадра
struct FrameState {
    float timestamp;
    float deltaTime;
    
    // Игроки
    PlayerState player1;
    PlayerState player2;
    bool hasDualPlayer;
    
    // Камера
    CCPoint cameraOffset;
    float cameraZoom;
    
    // Игровое состояние
    float levelTime;
    float musicTime;
    
    // Захват состояния игрока
    void capturePlayer(PlayerObject* player, PlayerState& state) const {
        if (!player) return;
        
        state.position = player->getPosition();
        state.rotation = player->getRotation();
        state.scale = player->getScale();
        state.yVelocity = player->m_yVelocity;
        
        state.isUpsideDown = player->m_isUpsideDown;
        state.isOnGround = player->m_isOnGround;
        state.isDashing = player->m_isDashing;
        state.isHidden = player->m_isHidden;
        state.isLocked = player->m_isLocked;
        
        state.isShip = player->m_isShip;
        state.isBall = player->m_isBall;
        state.isBird = player->m_isBird;
        state.isDart = player->m_isDart;
        state.isRobot = player->m_isRobot;
        state.isSpider = player->m_isSpider;
        state.isSwing = player->m_isSwing;
        
        if (player->m_iconSprite) {
            state.iconRotation = player->m_iconSprite->getRotation();
            state.flipX = player->m_iconSprite->isFlipX();
            state.flipY = player->m_iconSprite->isFlipY();
        }
    }
    
    // Применение состояния к игроку
    void applyToPlayer(PlayerObject* player, const PlayerState& state) const {
        if (!player) return;
        
        player->setPosition(state.position);
        player->setRotation(state.rotation);
        
        player->m_yVelocity = state.yVelocity;
        player->m_isUpsideDown = state.isUpsideDown;
        player->m_isOnGround = state.isOnGround;
        
        if (player->m_iconSprite) {
            player->m_iconSprite->setRotation(state.iconRotation);
            player->m_iconSprite->setFlipX(state.flipX);
            player->m_iconSprite->setFlipY(state.flipY);
        }
    }
};

// Кольцевой буфер для хранения кадров
class FrameBuffer {
private:
    std::vector<FrameState> m_frames;
    size_t m_capacity;
    size_t m_head = 0;      // Позиция записи
    size_t m_size = 0;      // Текущий размер
    
public:
    FrameBuffer(size_t capacity = 300) : m_capacity(capacity) {
        m_frames.resize(capacity);
    }
    
    void push(const FrameState& frame) {
        m_frames[m_head] = frame;
        m_head = (m_head + 1) % m_capacity;
        if (m_size < m_capacity) {
            m_size++;
        }
    }
    
    void clear() {
        m_head = 0;
        m_size = 0;
    }
    
    size_t size() const { return m_size; }
    bool empty() const { return m_size == 0; }
    size_t capacity() const { return m_capacity; }
    
    void resize(size_t newCapacity) {
        m_frames.resize(newCapacity);
        m_capacity = newCapacity;
        clear();
    }
    
    // Получить кадр по индексу (0 = самый старый, size-1 = самый новый)
    const FrameState& at(size_t index) const {
        size_t actualIndex = (m_head - m_size + index + m_capacity) % m_capacity;
        return m_frames[actualIndex];
    }
    
    // Получить кадры в обратном порядке для воспроизведения
    const FrameState& fromEnd(size_t index) const {
        return at(m_size - 1 - index);
    }
    
    // Итератор с конца (для rewind)
    std::vector<FrameState> getRewindFrames(size_t count) const {
        std::vector<FrameState> result;
        size_t actualCount = std::min(count, m_size);
        result.reserve(actualCount);
        
        for (size_t i = 0; i < actualCount; i++) {
            result.push_back(fromEnd(i));
        }
        
        return result;
    }
};
