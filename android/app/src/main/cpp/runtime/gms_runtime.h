#pragma once

#include "gms_types.h"
#include "gms_ds.h"
#include "gms_math.h"
#include "gms_datawin_loader.h"
#include <unordered_map>
#include <vector>
#include <memory>
#include <string>

class GMS_Instance {
public:
    int id = 0;
    int objectIndex = 0;
    std::string objectName;
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
    double z_aim = 0.0;
    double xprevious = 0.0;
    double yprevious = 0.0;
    double xstart = 0.0;
    double ystart = 0.0;
    double hspeed = 0.0;
    double vspeed = 0.0;
    double speed = 0.0;
    double direction = 0.0;
    double gravity = 0.0;
    double gravity_direction = 270.0;
    double friction = 0.0;
    double image_index = 0.0;
    double image_speed = 1.0;
    double image_xscale = 1.0;
    double image_yscale = 1.0;
    double image_angle = 0.0;
    double image_alpha = 1.0;
    uint32_t image_blend = 0xFFFFFFFF;
    int sprite_index = -1;
    int mask_index = -1;
    double depth = 0.0;
    bool visible = true;
    bool solid = false;
    bool persistent = false;
    bool markedForDestroy = false;

    std::vector<double> alarms = std::vector<double>(12, -1.0);
    std::unordered_map<std::string, GMS_Value> variables;

    GMS_Instance(int inId, int inObjIndex, const std::string& inName)
        : id(inId), objectIndex(inObjIndex), objectName(inName) {}
    virtual ~GMS_Instance() {}

    virtual void onCreate() {}
    virtual void onDestroy() {}
    virtual void onCleanUp() {}
    virtual void onStep() {}
    virtual void onDraw() {}
    virtual void onDrawGUI() {}
    virtual void onCollision(GMS_Instance* other) {}

    GMS_Value getVar(const std::string& name) {
        auto it = variables.find(name);
        if (it != variables.end()) return it->second;
        return GMS_Value();
    }

    void setVar(const std::string& name, const GMS_Value& val) {
        variables[name] = val;
    }
};

class GMS_Runtime {
public:
    static GMS_Runtime& get() {
        static GMS_Runtime instance;
        return instance;
    }

    void initialize(const std::string& dataWinPath, const std::string& saveDir);
    void shutdown();
    void step();
    void render();
    void resize(int width, int height);

    // Global variable store
    GMS_Value getGlobal(const std::string& name);
    void setGlobal(const std::string& name, const GMS_Value& val);

    // Instance management
    std::shared_ptr<GMS_Instance> createInstance(double x, double y, double depth, int objectIndex, const std::string& name);
    void addInstance(std::shared_ptr<GMS_Instance> inst);
    void destroyInstance(int instanceId);
    std::shared_ptr<GMS_Instance> findInstance(int instanceId);
    std::vector<std::shared_ptr<GMS_Instance>> findInstancesByObject(const std::string& objectName);


    // Collision detection
    bool placeMeeting(GMS_Instance* inst, double testX, double testY, const std::string& targetObj);
    bool positionMeeting(double testX, double testY, const std::string& targetObj);

    // Room control
    void roomGoto(int roomId, const std::string& roomName);
    int getCurrentRoomId() const { return m_currentRoomId; }
    const std::string& getCurrentRoomName() const { return m_currentRoomName; }

    GMS_DataWinLoader& getDataLoader() { return m_dataLoader; }

    int getScreenWidth() const { return m_screenWidth; }
    int getScreenHeight() const { return m_screenHeight; }

private:
    GMS_Runtime();
    ~GMS_Runtime();

    void updateInstances();
    void sortInstancesByDepth();

    GMS_DataWinLoader m_dataLoader;
    std::string m_saveDirectory;
    int m_screenWidth = 1920;
    int m_screenHeight = 1080;

    int m_nextInstanceId = 100000;
    int m_currentRoomId = 0;
    std::string m_currentRoomName = "room_preload";

    std::unordered_map<std::string, GMS_Value> m_globals;
    std::vector<std::shared_ptr<GMS_Instance>> m_instances;
    std::vector<std::shared_ptr<GMS_Instance>> m_pendingInstances;
    std::vector<std::shared_ptr<GMS_Instance>> m_drawList;
};
