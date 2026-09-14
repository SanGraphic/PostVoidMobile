#pragma once

#include "gms_types.h"
#include <string>
#include <vector>
#include <fstream>
#include <unordered_map>

struct GMS_DataChunkHeader {
    char name[4];
    uint32_t length;
};

class GMS_DataWinLoader {
public:
    GMS_DataWinLoader();
    ~GMS_DataWinLoader();

    bool loadFromFile(const std::string& path);
    bool isValid() const { return m_loaded; }

    const std::string& getGameName() const { return m_gameName; }
    const std::string& getDisplayName() const { return m_displayName; }
    int getBytecodeVersion() const { return m_bytecodeVersion; }

    const std::vector<GMS_SpriteData>& getSprites() const { return m_sprites; }
    const std::vector<GMS_SoundData>& getSounds() const { return m_sounds; }
    const std::vector<std::string>& getStrings() const { return m_strings; }

    // Texture sheet data extracted on the fly
    struct TexturePageData {
        int index = 0;
        std::vector<uint8_t> pngData;
        int glTextureId = 0;
        int width = 2048;
        int height = 2048;
    };
    const std::vector<TexturePageData>& getTexturePages() const { return m_texturePages; }

    // Audio stream data extracted on the fly
    struct AudioBufferData {
        int id = 0;
        std::vector<uint8_t> soundData;
        std::string format; // "wav" or "ogg"
    };
    const std::vector<AudioBufferData>& getAudioBuffers() const { return m_audioBuffers; }

private:
    bool parseForm(std::ifstream& file);
    bool parseGEN8(std::ifstream& file, uint32_t len);
    bool parseSTRG(std::ifstream& file, uint32_t len);
    bool parseTXTR(std::ifstream& file, uint32_t len);
    bool parseTPAG(std::ifstream& file, uint32_t len);
    bool parseSPRT(std::ifstream& file, uint32_t len);
    bool parseAUDO(std::ifstream& file, uint32_t len);
    bool parseSOND(std::ifstream& file, uint32_t len);

    bool m_loaded = false;
    std::string m_gameName;
    std::string m_displayName;
    int m_bytecodeVersion = 17;

    std::vector<std::string> m_strings;
    std::vector<TexturePageData> m_texturePages;
    std::vector<GMS_TextureItem> m_textureItems;
    std::vector<GMS_SpriteData> m_sprites;
    std::vector<AudioBufferData> m_audioBuffers;
    std::vector<GMS_SoundData> m_sounds;
};
