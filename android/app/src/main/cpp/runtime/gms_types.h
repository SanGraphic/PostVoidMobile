#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <cstdint>
#include <cmath>
#include <algorithm>
#include <iostream>
#include <sstream>
#include <android/log.h>

#define LOG_TAG "PostVoidRunner"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)

enum class GMS_Type {
    UNDEFINED,
    REAL,
    STRING,
    ARRAY,
    POINTER,
    BOOLEAN
};

struct GMS_Value;
class GMS_Instance;

struct GMS_Value {
    GMS_Type type = GMS_Type::UNDEFINED;
    double rVal = 0.0;
    std::string sVal = "";
    std::shared_ptr<std::vector<GMS_Value>> aVal = nullptr;
    void* pVal = nullptr;

    GMS_Value() : type(GMS_Type::UNDEFINED), rVal(0.0), aVal(nullptr), pVal(nullptr) {}
    GMS_Value(double v) : type(GMS_Type::REAL), rVal(v), aVal(nullptr), pVal(nullptr) {}
    GMS_Value(int v) : type(GMS_Type::REAL), rVal(static_cast<double>(v)), aVal(nullptr), pVal(nullptr) {}
    GMS_Value(bool v) : type(GMS_Type::BOOLEAN), rVal(v ? 1.0 : 0.0), aVal(nullptr), pVal(nullptr) {}
    GMS_Value(const char* s) : type(GMS_Type::STRING), rVal(0.0), sVal(s ? s : ""), aVal(nullptr), pVal(nullptr) {}
    GMS_Value(const std::string& s) : type(GMS_Type::STRING), rVal(0.0), sVal(s), aVal(nullptr), pVal(nullptr) {}
    GMS_Value(std::shared_ptr<std::vector<GMS_Value>> arr) : type(GMS_Type::ARRAY), rVal(0.0), aVal(arr), pVal(nullptr) {}
    GMS_Value(void* ptr) : type(GMS_Type::POINTER), rVal(0.0), aVal(nullptr), pVal(ptr) {}

    static GMS_Value CreateArray(size_t initialSize = 0) {
        GMS_Value val;
        val.type = GMS_Type::ARRAY;
        val.aVal = std::make_shared<std::vector<GMS_Value>>(initialSize);
        return val;
    }

    double asReal() const {
        if (type == GMS_Type::REAL || type == GMS_Type::BOOLEAN) return rVal;
        if (type == GMS_Type::STRING) {
            try { return std::stod(sVal); } catch (...) { return 0.0; }
        }
        return 0.0;
    }

    int asInt() const {
        return static_cast<int>(std::round(asReal()));
    }

    bool asBool() const {
        if (type == GMS_Type::BOOLEAN || type == GMS_Type::REAL) return rVal > 0.5;
        if (type == GMS_Type::STRING) return !sVal.empty() && sVal != "0";
        if (type == GMS_Type::ARRAY) return aVal != nullptr && !aVal->empty();
        return false;
    }

    std::string asString() const {
        if (type == GMS_Type::STRING) return sVal;
        if (type == GMS_Type::REAL) {
            if (std::floor(rVal) == rVal) return std::to_string(static_cast<int64_t>(rVal));
            return std::to_string(rVal);
        }
        if (type == GMS_Type::BOOLEAN) return rVal > 0.5 ? "true" : "false";
        if (type == GMS_Type::UNDEFINED) return "undefined";
        return "";
    }

    bool isDefined() const {
        return type != GMS_Type::UNDEFINED;
    }

    GMS_Value& operator[](size_t index) {
        if (type != GMS_Type::ARRAY || !aVal) {
            type = GMS_Type::ARRAY;
            aVal = std::make_shared<std::vector<GMS_Value>>();
        }
        if (index >= aVal->size()) {
            aVal->resize(index + 1);
        }
        return (*aVal)[index];
    }

    const GMS_Value& operator[](size_t index) const {
        if (type == GMS_Type::ARRAY && aVal && index < aVal->size()) {
            return (*aVal)[index];
        }
        static GMS_Value s_undef;
        return s_undef;
    }
};

enum GMS_EventType {
    EVENT_CREATE = 0,
    EVENT_DESTROY = 1,
    EVENT_ALARM = 2,
    EVENT_STEP = 3,
    EVENT_COLLISION = 4,
    EVENT_KEYBOARD = 5,
    EVENT_MOUSE = 6,
    EVENT_OTHER = 7,
    EVENT_DRAW = 8,
    EVENT_KEYPRESS = 9,
    EVENT_KEYRELEASE = 10,
    EVENT_TRIGGER = 11,
    EVENT_CLEANUP = 12,
    EVENT_GESTURE = 13
};

struct GMS_TextureItem {
    int id = -1;
    int pageIndex = 0;
    int srcX = 0;
    int srcY = 0;
    int srcW = 0;
    int srcH = 0;
    int targetX = 0;
    int targetY = 0;
    int targetW = 0;
    int targetH = 0;
    int boundW = 0;
    int boundH = 0;
};

struct GMS_SpriteData {
    int id = -1;
    std::string name;
    int width = 0;
    int height = 0;
    int originX = 0;
    int originY = 0;
    std::vector<GMS_TextureItem> frames;
    bool isSpecial = false;
};

struct GMS_SoundData {
    int id = -1;
    std::string name;
    std::string type;
    std::string file;
    std::string audioGroup;
    float volume = 1.0f;
    float pitch = 1.0f;
    uint32_t audioOffset = 0;
    uint32_t audioLength = 0;
};
