#include "game_objects.h"

// ==========================================
// OBJ_PLAYER IMPLEMENTATION
// ==========================================
ObjPlayer::ObjPlayer(int inId, int inObjIndex, const std::string& inName)
    : GMS_Instance(inId, inObjIndex, inName) {
    depth = -100.0;
}

ObjPlayer::~ObjPlayer() {}

void ObjPlayer::onCreate() {
    LOGI("[ObjPlayer] Player instance spawned at (%.1f, %.1f, %.1f)", x, y, z);
    idolLiquid = 100.0;
    ammo = maxAmmo;
    direction = 0.0;
    z_aim = 0.0;
}

void ObjPlayer::onStep() {
    auto& input = TouchController::get();

    // Look / Aim update
    direction += input.getAimDeltaX();
    z_aim = GMSMath::clamp(z_aim + input.getAimDeltaY(), -60.0, 60.0);

    // Movement calculation
    float moveX = input.getMoveX();
    float moveY = input.getMoveY();

    if (std::abs(moveX) > 0.1f || std::abs(moveY) > 0.1f) {
        double moveAngle = direction + GMSMath::darctan2(-moveY, moveX);
        double targetSpeed = isSliding ? slideSpeed : moveSpeed;

        hspeed = GMSMath::lengthdir_x(targetSpeed, moveAngle);
        vspeed = GMSMath::lengthdir_y(targetSpeed, moveAngle);

        headBob += 0.2;
    } else {
        hspeed = GMSMath::lerp(hspeed, 0.0, 0.2);
        vspeed = GMSMath::lerp(vspeed, 0.0, 0.2);
    }

    // Fire weapon input
    if (input.isFiring() && !isReloading && ammo > 0) {
        fireWeapon();
    }

    // Idol Liquid Drain Mechanics
    idolLiquid -= idolDrainRate;
    if (idolLiquid <= 0.0) {
        idolLiquid = 0.0;
        LOGI("[ObjPlayer] Idol is EMPTY! Player death triggered.");
        // Restart or Game Over
    }

    // Reload timer
    if (isReloading) {
        reloadTimer -= 1.0;
        if (reloadTimer <= 0.0) {
            isReloading = false;
            ammo = maxAmmo;
        }
    }

    // Smooth recoil recovery
    recoilZ = GMSMath::lerp(recoilZ, 0.0, 0.15);

    // Update 3D Camera with Player headbob & pitch
    float camHeight = static_cast<float>(z + 32.0 + std::sin(headBob) * 2.0);
    float lookX = static_cast<float>(x + GMSMath::dcos(direction) * 100.0);
    float lookY = static_cast<float>(y + GMSMath::dsin(direction) * 100.0);
    float lookZ = static_cast<float>(camHeight + GMSMath::dsin(-z_aim) * 50.0);

    RendererGLES::get().setCamera(static_cast<float>(x), static_cast<float>(y), camHeight,
                                  lookX, lookY, lookZ, 80.0f);

}

void ObjPlayer::fireWeapon() {
    ammo--;
    recoilZ = 8.0;
    LOGI("[ObjPlayer] BANG! Ammo remaining: %d", ammo);
    OboeAudioEngine::get().playSound(0, 1.0f, false);

    // Raycast hitscan to hit enemies in front of crosshair
    auto enemies = GMS_Runtime::get().findInstancesByObject("obj_enemy_walker");
    for (auto& enemyInst : enemies) {
        auto enemy = std::dynamic_pointer_cast<ObjEnemyWalker>(enemyInst);
        if (enemy && !enemy->isDead) {
            double dist = GMSMath::point_distance(x, y, enemy->x, enemy->y);
            if (dist < 800.0) {
                double angleToEnemy = GMSMath::point_direction(x, y, enemy->x, enemy->y);
                double angleDiff = std::abs(GMSMath::angle_difference(direction, angleToEnemy));
                if (angleDiff < 15.0) {
                    enemy->takeDamage(35.0);
                    addIdolLiquid(25.0); // Reward idol liquid on hit!
                    break;
                }
            }
        }
    }

    if (ammo <= 0) {
        reloadWeapon();
    }
}

void ObjPlayer::reloadWeapon() {
    isReloading = true;
    reloadTimer = 45.0; // 0.75s reload animation
    LOGI("[ObjPlayer] Reloading weapon...");
}

void ObjPlayer::addIdolLiquid(double amount) {
    idolLiquid = GMSMath::clamp(idolLiquid + amount, 0.0, 100.0);
}

void ObjPlayer::onDraw() {
    // Player is first-person, drawn via GUI idol & weapon model
}

void ObjPlayer::onDrawGUI() {
    RendererGLES::get().set3DMode(false);

    int screenW = RendererGLES::get().getWidth();
    int screenH = RendererGLES::get().getHeight();
    float cx = screenW * 0.5f;
    float cy = screenH * 0.5f;

    // 1. Draw Crosshair (Bright White & Yellow)
    RendererGLES::get().drawRectangle(cx - 8.0f, cy - 1.0f, cx + 8.0f, cy + 1.0f, 0xFFFFFFFF, false);
    RendererGLES::get().drawRectangle(cx - 1.0f, cy - 8.0f, cx + 1.0f, cy + 8.0f, 0xFFFFFFFF, false);
    RendererGLES::get().drawRectangle(cx - 2.0f, cy - 2.0f, cx + 2.0f, cy + 2.0f, 0xFF00FFFF, false);

    // 2. Draw First-Person Revolver (Bottom Center-Right with Recoil & Bob)
    float gunX = cx + 80.0f + static_cast<float>(std::cos(headBob) * 10.0);
    float gunY = screenH - 180.0f + static_cast<float>(std::abs(std::sin(headBob)) * 12.0) - static_cast<float>(recoilZ * 4.0);

    // Gun Barrel & Body
    RendererGLES::get().drawRectangle(gunX, gunY, gunX + 50.0f, gunY + 120.0f, 0xFF222222, false);
    RendererGLES::get().drawRectangle(gunX + 10.0f, gunY - 40.0f, gunX + 40.0f, gunY + 30.0f, 0xFF444444, false);
    RendererGLES::get().drawRectangle(gunX + 18.0f, gunY - 70.0f, gunX + 32.0f, gunY - 40.0f, 0xFF111111, false);
    // Cylinder / Accents (Neon Yellow)
    RendererGLES::get().drawRectangle(gunX + 8.0f, gunY + 10.0f, gunX + 42.0f, gunY + 45.0f, 0xFF00EEFF, false);

    // Muzzle flash on recoil
    if (recoilZ > 2.0) {
        float flashX = gunX + 25.0f;
        float flashY = gunY - 80.0f;
        RendererGLES::get().drawRectangle(flashX - 25.0f, flashY - 25.0f, flashX + 25.0f, flashY + 25.0f, 0xFF00FFFF, false);
        RendererGLES::get().drawRectangle(flashX - 12.0f, flashY - 12.0f, flashX + 12.0f, flashY + 12.0f, 0xFFFFFFFF, false);
    }

    // 3. Draw Idol (Liquid Hourglass) in Bottom-Left Hand
    float idolX = 60.0f;
    float idolY = screenH - 180.0f;
    float idolH = 140.0f;
    float idolW = 60.0f;

    // Idol Head Outline / Shell
    RendererGLES::get().drawRectangle(idolX, idolY, idolX + idolW, idolY + idolH, 0xFF330033, false);
    RendererGLES::get().drawRectangle(idolX - 2.0f, idolY - 2.0f, idolX + idolW + 2.0f, idolY + idolH + 2.0f, 0xFFFF00CC, true);

    // Idol Liquid Fill (Drains down)
    float liquidHeight = static_cast<float>(idolH * (idolLiquid / 100.0));
    uint32_t liquidCol = (idolLiquid < 30.0) ? 0xFFFF0033 : 0xFFFFCC00;
    RendererGLES::get().drawRectangle(idolX + 4.0f, idolY + idolH - liquidHeight, idolX + idolW - 4.0f, idolY + idolH - 4.0f, liquidCol, false);

    // 4. Draw Ammo Counters (Bottom Right)
    for (int i = 0; i < maxAmmo; i++) {
        uint32_t bulletCol = (i < ammo) ? 0xFF00FFFF : 0xFF333333;
        RendererGLES::get().drawRectangle(screenW - 50.0f - (i * 22.0f), screenH - 50.0f, screenW - 35.0f - (i * 22.0f), screenH - 20.0f, bulletCol, false);
    }

    // Touch controls overlay
    TouchController::get().renderVirtualControls();
}


// ==========================================
// OBJ_ENEMY_WALKER IMPLEMENTATION
// ==========================================
ObjEnemyWalker::ObjEnemyWalker(int inId, int inObjIndex, const std::string& inName)
    : GMS_Instance(inId, inObjIndex, inName) {
    depth = 0.0;
}

ObjEnemyWalker::~ObjEnemyWalker() {}

void ObjEnemyWalker::onCreate() {
    health = 30.0;
    isDead = false;
}

void ObjEnemyWalker::onStep() {
    if (isDead) return;

    // Track Player
    auto playerList = GMS_Runtime::get().findInstancesByObject("obj_player");
    if (!playerList.empty()) {
        auto player = playerList[0];
        double dist = GMSMath::point_distance(x, y, player->x, player->y);
        if (dist > 40.0 && dist < 1200.0) {
            double dirToPlayer = GMSMath::point_direction(x, y, player->x, player->y);
            hspeed = GMSMath::lengthdir_x(enemySpeed, dirToPlayer);
            vspeed = GMSMath::lengthdir_y(enemySpeed, dirToPlayer);
        } else if (dist <= 40.0) {
            hspeed = 0.0;
            vspeed = 0.0;
            // Attack player
            auto p = std::dynamic_pointer_cast<ObjPlayer>(player);
            if (p) {
                p->idolLiquid -= 1.0; // Drain player idol on contact
            }
        }
    }
}

void ObjEnemyWalker::takeDamage(double dmg) {
    health -= dmg;
    if (health <= 0.0 && !isDead) {
        isDead = true;
        LOGI("[ObjEnemyWalker] Enemy eliminated! Splattering blood.");
        // Spawn blood effect and destroy
        markedForDestroy = true;
    }
}

void ObjEnemyWalker::onDraw() {
    if (isDead) return;

    RendererGLES::get().set3DMode(true);
    // Draw 3D Billboard Sprite
    RendererGLES::get().drawSprite3D(sprite_index, 0, static_cast<float>(x), static_cast<float>(y), static_cast<float>(z), 1.0f, 0xFFFFFFFF, 1.0f);
}

// ==========================================
// OBJ_GOAL IMPLEMENTATION
// ==========================================
ObjGoal::ObjGoal(int inId, int inObjIndex, const std::string& inName)
    : GMS_Instance(inId, inObjIndex, inName) {}

ObjGoal::~ObjGoal() {}

void ObjGoal::onCreate() {}

void ObjGoal::onStep() {
    pulseTimer += 0.05;

    // Check collision with player
    auto players = GMS_Runtime::get().findInstancesByObject("obj_player");
    if (!players.empty()) {
        double dist = GMSMath::point_distance(x, y, players[0]->x, players[0]->y);
        if (dist < 64.0) {
            LOGI("[ObjGoal] Level Complete! Entering next level.");
            // Advance to next room / stage
            GMS_Runtime::get().roomGoto(GMS_Runtime::get().getCurrentRoomId() + 1, "room_stage_next");
        }
    }
}

void ObjGoal::onDraw() {
    RendererGLES::get().set3DMode(true);
    float pulse = static_cast<float>(1.0 + std::sin(pulseTimer) * 0.2);
    RendererGLES::get().drawSprite3D(19, 0, static_cast<float>(x), static_cast<float>(y), static_cast<float>(z), pulse, 0xFF00FF00, 0.9f);
}
