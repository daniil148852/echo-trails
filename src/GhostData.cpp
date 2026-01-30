#include "GhostData.hpp"
#include <fstream>
#include <cstring>

namespace EchoTrails {

void GhostFrame::serialize(std::vector<uint8_t>& buffer) const {
    auto writeFloat = [&buffer](float val) {
        uint8_t bytes[4];
        std::memcpy(bytes, &val, 4);
        buffer.insert(buffer.end(), bytes, bytes + 4);
    };
    
    auto writeByte = [&buffer](uint8_t val) {
        buffer.push_back(val);
    };
    
    writeFloat(posX);
    writeFloat(posY);
    writeFloat(rotation);
    writeFloat(scaleX);
    writeFloat(scaleY);
    
    // Упаковываем флаги в один байт
    uint8_t flags = 0;
    flags |= (isUpsideDown ? 1 : 0) << 0;
    flags |= (isVisible ? 1 : 0) << 1;
    flags |= (isDashing ? 1 : 0) << 2;
    flags |= (isMini ? 1 : 0) << 3;
    flags |= (isDual ? 1 : 0) << 4;
    flags |= (isPlayer2 ? 1 : 0) << 5;
    
    writeByte(static_cast<uint8_t>(gameMode));
    writeByte(flags);
}

GhostFrame GhostFrame::deserialize(const uint8_t* data, size_t& offset) {
    GhostFrame frame;
    
    auto readFloat = [&data, &offset]() -> float {
        float val;
        std::memcpy(&val, data + offset, 4);
        offset += 4;
        return val;
    };
    
    auto readByte = [&data, &offset]() -> uint8_t {
        return data[offset++];
    };
    
    frame.posX = readFloat();
    frame.posY = readFloat();
    frame.rotation = readFloat();
    frame.scaleX = readFloat();
    frame.scaleY = readFloat();
    
    frame.gameMode = static_cast<GameMode>(readByte());
    
    uint8_t flags = readByte();
    frame.isUpsideDown = (flags >> 0) & 1;
    frame.isVisible = (flags >> 1) & 1;
    frame.isDashing = (flags >> 2) & 1;
    frame.isMini = (flags >> 3) & 1;
    frame.isDual = (flags >> 4) & 1;
    frame.isPlayer2 = (flags >> 5) & 1;
    
    return frame;
}

GhostFrame interpolateFrames(const GhostFrame& a, const GhostFrame& b, float t) {
    GhostFrame result;
    
    // Линейная интерполяция позиции
    result.posX = a.posX + (b.posX - a.posX) * t;
    result.posY = a.posY + (b.posY - a.posY) * t;
    
    // Интерполяция поворота (учитываем переход через 360)
    float rotDiff = b.rotation - a.rotation;
    if (rotDiff > 180.0f) rotDiff -= 360.0f;
    if (rotDiff < -180.0f) rotDiff += 360.0f;
    result.rotation = a.rotation + rotDiff * t;
    
    result.scaleX = a.scaleX + (b.scaleX - a.scaleX) * t;
    result.scaleY = a.scaleY + (b.scaleY - a.scaleY) * t;
    
    // Дискретные значения берем из ближайшего кадра
    if (t < 0.5f) {
        result.gameMode = a.gameMode;
        result.isUpsideDown = a.isUpsideDown;
        result.isVisible = a.isVisible;
        result.isDashing = a.isDashing;
        result.isMini = a.isMini;
        result.isDual = a.isDual;
        result.isPlayer2 = a.isPlayer2;
    } else {
        result.gameMode = b.gameMode;
        result.isUpsideDown = b.isUpsideDown;
        result.isVisible = b.isVisible;
        result.isDashing = b.isDashing;
        result.isMini = b.isMini;
        result.isDual = b.isDual;
        result.isPlayer2 = b.isPlayer2;
    }
    
    return result;
}

// Магическое число для проверки формата файла
constexpr uint32_t GHOST_FILE_MAGIC = 0x47485354; // "GHST"
constexpr uint16_t GHOST_FILE_VERSION = 1;

bool GhostRecording::saveToFile(const std::filesystem::path& path) const {
    try {
        std::ofstream file(path, std::ios::binary);
        if (!file.is_open()) return false;
        
        // Заголовок
        file.write(reinterpret_cast<const char*>(&GHOST_FILE_MAGIC), 4);
        file.write(reinterpret_cast<const char*>(&GHOST_FILE_VERSION), 2);
        
        // Метаданные
        uint32_t levelIDLen = levelID.size();
        file.write(reinterpret_cast<const char*>(&levelIDLen), 4);
        file.write(levelID.c_str(), levelIDLen);
        
        uint32_t levelNameLen = levelName.size();
        file.write(reinterpret_cast<const char*>(&levelNameLen), 4);
        file.write(levelName.c_str(), levelNameLen);
        
        file.write(reinterpret_cast<const char*>(&bestPercent), 4);
        file.write(reinterpret_cast<const char*>(&totalTime), 4);
        file.write(reinterpret_cast<const char*>(&fps), 4);
        file.write(reinterpret_cast<const char*>(&timestamp), 8);
        
        // Визуал игрока
        file.write(reinterpret_cast<const char*>(&visuals), sizeof(PlayerVisuals));
        
        // Кадры
        uint32_t frameCount = frames.size();
        file.write(reinterpret_cast<const char*>(&frameCount), 4);
        
        std::vector<uint8_t> frameBuffer;
        for (const auto& frame : frames) {
            frame.serialize(frameBuffer);
        }
        file.write(reinterpret_cast<const char*>(frameBuffer.data()), frameBuffer.size());
        
        // Player 2 кадры (для dual)
        uint32_t p2FrameCount = player2Frames.size();
        file.write(reinterpret_cast<const char*>(&p2FrameCount), 4);
        
        if (p2FrameCount > 0) {
            frameBuffer.clear();
            for (const auto& frame : player2Frames) {
                frame.serialize(frameBuffer);
            }
            file.write(reinterpret_cast<const char*>(frameBuffer.data()), frameBuffer.size());
        }
        
        return true;
    } catch (...) {
        return false;
    }
}

std::optional<GhostRecording> GhostRecording::loadFromFile(const std::filesystem::path& path) {
    try {
        std::ifstream file(path, std::ios::binary);
        if (!file.is_open()) return std::nullopt;
        
        // Проверка заголовка
        uint32_t magic;
        file.read(reinterpret_cast<char*>(&magic), 4);
        if (magic != GHOST_FILE_MAGIC) return std::nullopt;
        
        uint16_t version;
        file.read(reinterpret_cast<char*>(&version), 2);
        if (version > GHOST_FILE_VERSION) return std::nullopt;
        
        GhostRecording recording;
        
        // Метаданные
        uint32_t levelIDLen;
        file.read(reinterpret_cast<char*>(&levelIDLen), 4);
        recording.levelID.resize(levelIDLen);
        file.read(recording.levelID.data(), levelIDLen);
        
        uint32_t levelNameLen;
        file.read(reinterpret_cast<char*>(&levelNameLen), 4);
        recording.levelName.resize(levelNameLen);
        file.read(recording.levelName.data(), levelNameLen);
        
        file.read(reinterpret_cast<char*>(&recording.bestPercent), 4);
        file.read(reinterpret_cast<char*>(&recording.totalTime), 4);
        file.read(reinterpret_cast<char*>(&recording.fps), 4);
        file.read(reinterpret_cast<char*>(&recording.timestamp), 8);
        
        // Визуал
        file.read(reinterpret_cast<char*>(&recording.visuals), sizeof(PlayerVisuals));
        
        // Кадры
        uint32_t frameCount;
        file.read(reinterpret_cast<char*>(&frameCount), 4);
        recording.frameCount = frameCount;
        
        constexpr size_t FRAME_SIZE = 22; // Размер сериализованного кадра
        std::vector<uint8_t> frameBuffer(frameCount * FRAME_SIZE);
        file.read(reinterpret_cast<char*>(frameBuffer.data()), frameBuffer.size());
        
        recording.frames.reserve(frameCount);
        size_t offset = 0;
        for (uint32_t i = 0; i < frameCount; ++i) {
            recording.frames.push_back(GhostFrame::deserialize(frameBuffer.data(), offset));
        }
        
        // Player 2 кадры
        uint32_t p2FrameCount;
        file.read(reinterpret_cast<char*>(&p2FrameCount), 4);
        
        if (p2FrameCount > 0) {
            frameBuffer.resize(p2FrameCount * FRAME_SIZE);
            file.read(reinterpret_cast<char*>(frameBuffer.data()), frameBuffer.size());
            
            recording.player2Frames.reserve(p2FrameCount);
            offset = 0;
            for (uint32_t i = 0; i < p2FrameCount; ++i) {
                recording.player2Frames.push_back(GhostFrame::deserialize(frameBuffer.data(), offset));
            }
        }
        
        return recording;
    } catch (...) {
        return std::nullopt;
    }
}

} // namespace EchoTrails
