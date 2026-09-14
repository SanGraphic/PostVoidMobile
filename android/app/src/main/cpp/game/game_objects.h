#pragma once

#include "gms_runtime.h"
#include "gms_math.h"
#include "renderer_gles.h"
#include "oboe_audio_engine.h"
#include "touch_controller.h"

// ==========================================
// OBJ_PLAYER
// ==========================================
class ObjPlayer : public GMS_Instance {
public:
    ObjPlayer(int inId, int inObjIndex, const std::string& inName);
    ~ObjPlayer() override;

    void onCreate() override;
    void onStep() override;
    void onDraw() override;
    void onDrawGUI() override;

    // Player state
    double moveSpeed = 6.0;
    double maxSpeed = 12.0;
    double slideSpeed = 16.0;
    bool isSliding = false;
    double slideTimer = 0.0;

    double idolLiquid = 100.0;     // Idol liquid timer (percentage)
    double idolDrainRate = 0.15;   // Drains every step
    int ammo = 6;
    int maxAmmo = 6;
    bool isReloading = false;
    double reloadTimer = 0.0;

    double headBob = 0.0;
    double weaponBob = 0.0;
    double recoilZ = 0.0;
    int currentWeapon = 0; // 0=Pistol, 1=Shotgun, 2=Uzi, 3=Knife

    void addIdolLiquid(double amount);
    void fireWeapon();
    void reloadWeapon();
};

// ==========================================
// OBJ_ENEMY_WALKER
// ==========================================
class ObjEnemyWalker : public GMS_Instance {
public:
    ObjEnemyWalker(int inId, int inObjIndex, const std::string& inName);
    ~ObjEnemyWalker() override;

    void onCreate() override;
    void onStep() override;
    void onDraw() override;

    double health = 30.0;
    double enemySpeed = 3.5;
    double attackCooldown = 0.0;
    bool isDead = false;

    void takeDamage(double dmg);
};

// ==========================================
// OBJ_GOAL (Level Exit Portal)
// ==========================================
class ObjGoal : public GMS_Instance {
public:
    ObjGoal(int inId, int inObjIndex, const std::string& inName);
    ~ObjGoal() override;

    void onCreate() override;
    void onStep() override;
    void onDraw() override;

    double pulseTimer = 0.0;
};
