#pragma once

#include "gms_runtime.h"
#include <string>
#include <vector>


// obj_preload (Room 0)
class ObjPreload : public GMS_Instance {
public:
    ObjPreload(int inId, int inObjIndex, const std::string& inName);
    virtual ~ObjPreload();

    void onCreate() override;
    void onStep() override;
private:
    int assetLoadPhase = 0;
};

// obj_splash_screen (Room 1)
class ObjSplashScreen : public GMS_Instance {
public:
    ObjSplashScreen(int inId, int inObjIndex, const std::string& inName);
    virtual ~ObjSplashScreen();

    void onCreate() override;
    void onStep() override;
    void onDrawGUI() override;

private:
    float frame = 0.0f;
    float frameSpeed = 0.25f;
    int maxFrames = 12;
    int lingerTimer = 0;
    std::string mode = "animate";
};

// obj_epilepsy (Room 2)
class ObjEpilepsy : public GMS_Instance {
public:
    ObjEpilepsy(int inId, int inObjIndex, const std::string& inName);
    virtual ~ObjEpilepsy();

    void onCreate() override;
    void onStep() override;
    void onDrawGUI() override;

private:
    int lingerTimer = 0;
    int lingerMax = 120;
};

// obj_menu (Room 4)
class ObjMenu : public GMS_Instance {
public:
    ObjMenu(int inId, int inObjIndex, const std::string& inName);
    virtual ~ObjMenu();

    void onCreate() override;
    void onStep() override;
    void onDrawGUI() override;

private:
    int optionSelected = 0;
    std::vector<std::string> options;
    std::vector<std::string> optionLabels;
    uint32_t postVoidCol = 0xFF00FFFF;
    int tick = 0;
    bool startingGame = false;
};
