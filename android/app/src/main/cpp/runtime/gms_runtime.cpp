#include "gms_runtime.h"
#include <algorithm>

GMS_Runtime::GMS_Runtime() {}
GMS_Runtime::~GMS_Runtime() {}

void GMS_Runtime::initialize(const std::string& dataWinPath, const std::string& saveDir) {
    LOGI("[GMS_Runtime] Initializing GMS2 Runner Runtime...");
    m_saveDirectory = saveDir;

    // Load data.win
    if (m_dataLoader.loadFromFile(dataWinPath)) {
        LOGI("[GMS_Runtime] Loaded game: %s (Bytecode v%d)",
             m_dataLoader.getGameName().c_str(), m_dataLoader.getBytecodeVersion());
    } else {
        LOGE("[GMS_Runtime] CRITICAL: Failed to load data.win!");
    }

    // Initialize global built-in state
    setGlobal("__texturegroup_names", GMS_Value::CreateArray());
    setGlobal("score", GMS_Value(0.0));
    setGlobal("lives", GMS_Value(3.0));
    setGlobal("health", GMS_Value(100.0));

    // Boot into preload room
    roomGoto(0, "room_preload");
}

void GMS_Runtime::shutdown() {
    LOGI("[GMS_Runtime] Shutting down runtime...");
    for (auto& inst : m_instances) {
        if (inst) {
            inst->onDestroy();
            inst->onCleanUp();
        }
    }
    m_instances.clear();
    m_pendingInstances.clear();
    m_drawList.clear();
}

void GMS_Runtime::step() {
    // Add pending instances
    if (!m_pendingInstances.empty()) {
        for (auto& inst : m_pendingInstances) {
            m_instances.push_back(inst);
            inst->onCreate();
        }
        m_pendingInstances.clear();
    }

    // Update instance step logic
    for (auto& inst : m_instances) {
        if (inst && !inst->markedForDestroy) {
            // Update alarms
            for (size_t a = 0; a < inst->alarms.size(); a++) {
                if (inst->alarms[a] > 0.0) {
                    inst->alarms[a] -= 1.0;
                    if (inst->alarms[a] == 0.0) {
                        // Trigger alarm
                        inst->alarms[a] = -1.0;
                    }
                }
            }

            // Update movement kinematics
            if (inst->gravity != 0.0) {
                inst->hspeed += GMSMath::lengthdir_x(inst->gravity, inst->gravity_direction);
                inst->vspeed += GMSMath::lengthdir_y(inst->gravity, inst->gravity_direction);
            }
            if (inst->friction != 0.0 && inst->speed != 0.0) {
                if (inst->speed > inst->friction) {
                    inst->speed -= inst->friction;
                } else if (inst->speed < -inst->friction) {
                    inst->speed += inst->friction;
                } else {
                    inst->speed = 0.0;
                }
                inst->hspeed = GMSMath::lengthdir_x(inst->speed, inst->direction);
                inst->vspeed = GMSMath::lengthdir_y(inst->speed, inst->direction);
            }

            inst->xprevious = inst->x;
            inst->yprevious = inst->y;
            inst->x += inst->hspeed;
            inst->y += inst->vspeed;

            // Step event
            inst->onStep();
        }
    }

    // Remove destroyed instances
    m_instances.erase(
        std::remove_if(m_instances.begin(), m_instances.end(), [](const std::shared_ptr<GMS_Instance>& inst) {
            if (!inst || inst->markedForDestroy) {
                if (inst) {
                    inst->onDestroy();
                    inst->onCleanUp();
                }
                return true;
            }
            return false;
        }),
        m_instances.end()
    );
}

void GMS_Runtime::render() {
    sortInstancesByDepth();

    // Standard Draw Event
    for (auto& inst : m_drawList) {
        if (inst && inst->visible && !inst->markedForDestroy) {
            inst->onDraw();
        }
    }

    // Draw GUI Event
    for (auto& inst : m_drawList) {
        if (inst && inst->visible && !inst->markedForDestroy) {
            inst->onDrawGUI();
        }
    }
}

void GMS_Runtime::resize(int width, int height) {
    m_screenWidth = width;
    m_screenHeight = height;
    LOGI("[GMS_Runtime] Screen resized to: %dx%d", width, height);
}

GMS_Value GMS_Runtime::getGlobal(const std::string& name) {
    auto it = m_globals.find(name);
    if (it != m_globals.end()) return it->second;
    return GMS_Value();
}

void GMS_Runtime::setGlobal(const std::string& name, const GMS_Value& val) {
    m_globals[name] = val;
}

std::shared_ptr<GMS_Instance> GMS_Runtime::createInstance(double x, double y, double depth, int objectIndex, const std::string& name) {
    int id = m_nextInstanceId++;
    auto inst = std::make_shared<GMS_Instance>(id, objectIndex, name);
    inst->x = x;
    inst->y = y;
    inst->xstart = x;
    inst->ystart = y;
    inst->xprevious = x;
    inst->yprevious = y;
    inst->depth = depth;

    m_pendingInstances.push_back(inst);
    return inst;
}

void GMS_Runtime::addInstance(std::shared_ptr<GMS_Instance> inst) {
    if (inst) {
        if (inst->id <= 0) {
            inst->id = m_nextInstanceId++;
        }
        m_pendingInstances.push_back(inst);
    }
}


void GMS_Runtime::destroyInstance(int instanceId) {
    for (auto& inst : m_instances) {
        if (inst && inst->id == instanceId) {
            inst->markedForDestroy = true;
            break;
        }
    }
}

std::shared_ptr<GMS_Instance> GMS_Runtime::findInstance(int instanceId) {
    for (auto& inst : m_instances) {
        if (inst && inst->id == instanceId && !inst->markedForDestroy) {
            return inst;
        }
    }
    return nullptr;
}

std::vector<std::shared_ptr<GMS_Instance>> GMS_Runtime::findInstancesByObject(const std::string& objectName) {
    std::vector<std::shared_ptr<GMS_Instance>> result;
    for (auto& inst : m_instances) {
        if (inst && inst->objectName == objectName && !inst->markedForDestroy) {
            result.push_back(inst);
        }
    }
    return result;
}

bool GMS_Runtime::placeMeeting(GMS_Instance* inst, double testX, double testY, const std::string& targetObj) {
    if (!inst) return false;
    for (auto& other : m_instances) {
        if (other && other.get() != inst && (other->objectName == targetObj || targetObj == "all") && !other->markedForDestroy) {
            double dist = GMSMath::point_distance(testX, testY, other->x, other->y);
            if (dist < 32.0) return true; // Standard 32px hitbox check
        }
    }
    return false;
}

bool GMS_Runtime::positionMeeting(double testX, double testY, const std::string& targetObj) {
    for (auto& other : m_instances) {
        if (other && (other->objectName == targetObj || targetObj == "all") && !other->markedForDestroy) {
            double dist = GMSMath::point_distance(testX, testY, other->x, other->y);
            if (dist < 16.0) return true;
        }
    }
    return false;
}

#include "menu_objects.h"

void GMS_Runtime::roomGoto(int roomId, const std::string& roomName) {
    LOGI("[GMS_Runtime] Transitioning to room: %s (ID: %d)", roomName.c_str(), roomId);
    m_currentRoomId = roomId;
    m_currentRoomName = roomName;

    // Clear non-persistent instances
    auto it = m_instances.begin();
    while (it != m_instances.end()) {
        if (*it && !(*it)->persistent) {
            (*it)->onDestroy();
            (*it)->onCleanUp();
            it = m_instances.erase(it);
        } else {
            ++it;
        }
    }

    // Spawn Room Controller Objects
    if (roomId == 0 || roomName == "room_preload") {
        addInstance(std::make_shared<ObjPreload>(1, 2, "obj_preload"));
    } else if (roomId == 1 || roomName == "room_splash_screen") {
        addInstance(std::make_shared<ObjSplashScreen>(2, 112, "obj_splash_screen"));
    } else if (roomId == 2 || roomName == "room_epilepsy") {
        addInstance(std::make_shared<ObjEpilepsy>(3, 113, "obj_epilepsy"));
    } else if (roomId == 4 || roomName == "room_menu") {
        addInstance(std::make_shared<ObjMenu>(4, 114, "obj_menu"));
    }
}

void GMS_Runtime::sortInstancesByDepth() {
    m_drawList = m_instances;
    // GameMaker draws from highest depth to lowest depth
    std::sort(m_drawList.begin(), m_drawList.end(), [](const std::shared_ptr<GMS_Instance>& a, const std::shared_ptr<GMS_Instance>& b) {
        if (!a || !b) return false;
        return a->depth > b->depth;
    });
}
