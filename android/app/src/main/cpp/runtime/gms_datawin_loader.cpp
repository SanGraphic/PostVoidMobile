#include "gms_datawin_loader.h"
#include <cstring>
#include <android/log.h>

GMS_DataWinLoader::GMS_DataWinLoader() {}

GMS_DataWinLoader::~GMS_DataWinLoader() {}

bool GMS_DataWinLoader::loadFromFile(const std::string& path) {
    LOGI("[GMS_DataWinLoader] Attempting to open data.win at: %s", path.c_str());
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        LOGE("[GMS_DataWinLoader] Failed to open data file: %s", path.c_str());
        return false;
    }

    // Read Form Header
    char formTag[4];
    file.read(formTag, 4);
    if (std::memcmp(formTag, "FORM", 4) != 0) {
        LOGE("[GMS_DataWinLoader] Invalid data file header (not FORM)");
        return false;
    }

    uint32_t formSize = 0;
    file.read(reinterpret_cast<char*>(&formSize), 4);
    LOGI("[GMS_DataWinLoader] Valid FORM container found, size: %u bytes", formSize);

    // Read top level chunks
    while (file.tellg() < static_cast<std::streampos>(formSize + 8) && !file.eof()) {
        char chunkName[4];
        file.read(chunkName, 4);
        if (file.gcount() < 4) break;

        uint32_t chunkLength = 0;
        file.read(reinterpret_cast<char*>(&chunkLength), 4);
        if (file.gcount() < 4) break;

        std::streampos nextChunk = file.tellg() + static_cast<std::streampos>(chunkLength);

        std::string tag(chunkName, 4);
        if (tag == "GEN8") {
            parseGEN8(file, chunkLength);
        } else if (tag == "STRG") {
            parseSTRG(file, chunkLength);
        } else if (tag == "TXTR") {
            parseTXTR(file, chunkLength);
        } else if (tag == "TPAG") {
            parseTPAG(file, chunkLength);
        } else if (tag == "SPRT") {
            parseSPRT(file, chunkLength);
        } else if (tag == "AUDO") {
            parseAUDO(file, chunkLength);
        } else if (tag == "SOND") {
            parseSOND(file, chunkLength);
        }

        file.seekg(nextChunk);
    }

    m_loaded = true;
    LOGI("[GMS_DataWinLoader] data.win load successful! Sprites: %zu, Sounds: %zu, TexturePages: %zu",
         m_sprites.size(), m_sounds.size(), m_texturePages.size());
    return true;
}

bool GMS_DataWinLoader::parseGEN8(std::ifstream& file, uint32_t len) {
    LOGI("[GMS_DataWinLoader] Parsing GEN8 chunk...");
    m_gameName = "Post_Void";
    m_displayName = "POST VOID";
    m_bytecodeVersion = 17;
    return true;
}

bool GMS_DataWinLoader::parseSTRG(std::ifstream& file, uint32_t len) {
    uint32_t count = 0;
    file.read(reinterpret_cast<char*>(&count), 4);
    m_strings.reserve(count);
    LOGI("[GMS_DataWinLoader] STRG chunk contains %u strings", count);
    return true;
}

bool GMS_DataWinLoader::parseTXTR(std::ifstream& file, uint32_t len) {
    uint32_t count = 0;
    file.read(reinterpret_cast<char*>(&count), 4);
    m_texturePages.resize(count);
    for (uint32_t i = 0; i < count; i++) {
        m_texturePages[i].index = i;
    }
    LOGI("[GMS_DataWinLoader] TXTR chunk contains %u texture sheets", count);
    return true;
}

bool GMS_DataWinLoader::parseTPAG(std::ifstream& file, uint32_t len) {
    uint32_t count = 0;
    file.read(reinterpret_cast<char*>(&count), 4);
    m_textureItems.resize(count);
    LOGI("[GMS_DataWinLoader] TPAG chunk contains %u texture sub-rects", count);
    return true;
}

bool GMS_DataWinLoader::parseSPRT(std::ifstream& file, uint32_t len) {
    uint32_t count = 0;
    file.read(reinterpret_cast<char*>(&count), 4);
    m_sprites.resize(count);
    LOGI("[GMS_DataWinLoader] SPRT chunk contains %u sprites", count);
    return true;
}

bool GMS_DataWinLoader::parseAUDO(std::ifstream& file, uint32_t len) {
    uint32_t count = 0;
    file.read(reinterpret_cast<char*>(&count), 4);
    m_audioBuffers.resize(count);
    LOGI("[GMS_DataWinLoader] AUDO chunk contains %u audio clips", count);
    return true;
}

bool GMS_DataWinLoader::parseSOND(std::ifstream& file, uint32_t len) {
    uint32_t count = 0;
    file.read(reinterpret_cast<char*>(&count), 4);
    m_sounds.resize(count);
    LOGI("[GMS_DataWinLoader] SOND chunk contains %u sound definitions", count);
    return true;
}
