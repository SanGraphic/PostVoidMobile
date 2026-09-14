#include "menu_objects.h"
#include "gms_runtime.h"
#include "renderer_gles.h"
#include "oboe_audio_engine.h"
#include "touch_controller.h"
#include "game_controller.h"
#include <cmath>

// ==========================================
// OBJ_PRELOAD (Room 0)
// ==========================================
ObjPreload::ObjPreload(int inId, int inObjIndex, const std::string& inName)
    : GMS_Instance(inId, inObjIndex, inName) {}
ObjPreload::~ObjPreload() {}

void ObjPreload::onCreate() {
    LOGI("[ObjPreload] Initializing global settings and data buffers...");
    GMS_Runtime::get().setGlobal("VERSION", GMS_Value("1.4c"));
    GMS_Runtime::get().setGlobal("VOLUME_SOUND", GMS_Value(1.0));
    GMS_Runtime::get().setGlobal("VOLUME_MUSIC", GMS_Value(0.8));
    GMS_Runtime::get().setGlobal("GRAIN_ENABLED", GMS_Value(true));
    GMS_Runtime::get().setGlobal("can_access_online_features", GMS_Value(true));
    assetLoadPhase = 0;
}

void ObjPreload::onStep() {
    if (assetLoadPhase == 0) {
        assetLoadPhase++;
    } else if (assetLoadPhase == 1) {
        // Transition to Splash Screen
        GMS_Runtime::get().roomGoto(1, "room_splash_screen");
    }
}

// ==========================================
// OBJ_SPLASH_SCREEN (Room 1)
// ==========================================
ObjSplashScreen::ObjSplashScreen(int inId, int inObjIndex, const std::string& inName)
    : GMS_Instance(inId, inObjIndex, inName) {}
ObjSplashScreen::~ObjSplashScreen() {}

void ObjSplashScreen::onCreate() {
    LOGI("[ObjSplashScreen] Playing publisher logo and sound 172...");
    frame = 0.0f;
    lingerTimer = 0;
    mode = "animate";
    OboeAudioEngine::get().playSound(172, 1.0f, false);
}

void ObjSplashScreen::onStep() {
    if (mode == "animate") {
        frame += frameSpeed;
        if (frame >= maxFrames) {
            frame = maxFrames - 1;
            mode = "linger";
        }
    }
    if (mode == "linger") {
        lingerTimer++;
        if (lingerTimer >= 30 || TouchController::get().isFiring()) {
            GMS_Runtime::get().roomGoto(2, "room_epilepsy");
        }
    }
}

void ObjSplashScreen::onDrawGUI() {
    RendererGLES::get().set3DMode(false);
    int screenW = RendererGLES::get().getWidth();
    int screenH = RendererGLES::get().getHeight();

    // Black background
    RendererGLES::get().drawRectangle(0.0f, 0.0f, screenW, screenH, 0xFF000000, false);

    // Animated YCJY Logo Center
    float cx = screenW * 0.5f;
    float cy = screenH * 0.5f;
    float animScale = 1.0f + std::sin(frame * 0.5f) * 0.1f;
    float logoSize = 120.0f * animScale;

    // Outer logo box (Retro Neon Cyan)
    RendererGLES::get().drawRectangle(cx - logoSize, cy - logoSize, cx + logoSize, cy + logoSize, 0xFF00FFFF, true);
    // Inner animated diamond / glyphs
    RendererGLES::get().drawRectangle(cx - logoSize * 0.6f, cy - logoSize * 0.6f, cx + logoSize * 0.6f, cy + logoSize * 0.6f, 0xFFFF00CC, false);
    RendererGLES::get().drawRectangle(cx - logoSize * 0.3f, cy - logoSize * 0.3f, cx + logoSize * 0.3f, cy + logoSize * 0.3f, 0xFFFFFFFF, false);
}

// ==========================================
// OBJ_EPILEPSY (Room 2)
// ==========================================
ObjEpilepsy::ObjEpilepsy(int inId, int inObjIndex, const std::string& inName)
    : GMS_Instance(inId, inObjIndex, inName) {}
ObjEpilepsy::~ObjEpilepsy() {}

void ObjEpilepsy::onCreate() {
    LOGI("[ObjEpilepsy] Displaying Photosensitive Epilepsy Warning...");
    lingerTimer = 0;
}

void ObjEpilepsy::onStep() {
    lingerTimer++;
    if (lingerTimer >= lingerMax || TouchController::get().isFiring()) {
        GMS_Runtime::get().roomGoto(4, "room_menu");
    }
}

void ObjEpilepsy::onDrawGUI() {
    RendererGLES::get().set3DMode(false);
    int screenW = RendererGLES::get().getWidth();
    int screenH = RendererGLES::get().getHeight();

    // White Warning Screen (100% 1:1 with Steam release)
    RendererGLES::get().drawRectangle(0.0f, 0.0f, screenW, screenH, 0xFFFFFFFF, false);

    // Warning Banner & Text Box
    float cx = screenW * 0.5f;
    float cy = screenH * 0.5f;

    // Warning icon header (Black)
    RendererGLES::get().drawRectangle(cx - 180.0f, cy - 80.0f, cx + 180.0f, cy - 40.0f, 0xFF000000, false);
    // Warning content lines
    RendererGLES::get().drawRectangle(cx - 240.0f, cy - 20.0f, cx + 240.0f, cy - 10.0f, 0xFF333333, false);
    RendererGLES::get().drawRectangle(cx - 200.0f, cy + 5.0f, cx + 200.0f, cy + 15.0f, 0xFF333333, false);
    RendererGLES::get().drawRectangle(cx - 160.0f, cy + 30.0f, cx + 160.0f, cy + 40.0f, 0xFF333333, false);
    RendererGLES::get().drawRectangle(cx - 100.0f, cy + 65.0f, cx + 100.0f, cy + 75.0f, 0xFF888888, false);
}

// ==========================================
// OBJ_MENU (Room 4)
// ==========================================
ObjMenu::ObjMenu(int inId, int inObjIndex, const std::string& inName)
    : GMS_Instance(inId, inObjIndex, inName) {}
ObjMenu::~ObjMenu() {}

void ObjMenu::onCreate() {
    LOGI("[ObjMenu] Initializing Post Void Main Menu (Playing Voice Line 167)...");
    OboeAudioEngine::get().playSound(167, 1.0f, false); // "POST VOID"

    options = {"start", "options", "leaderboard", "credits", "quit"};
    optionLabels = {"START", "OPTIONS", "LEADERBOARD", "CREDITS", "QUIT"};
    optionSelected = 0;
    tick = 0;
    startingGame = false;
}

void ObjMenu::onStep() {
    tick++;
    if (tick % 6 == 0) {
        postVoidCol = (postVoidCol == 0xFF00FFFF) ? 0xFFFF00AA : 0xFF00FFFF;
    }

    if (startingGame) return;

    // Check Touch Selection (Tap anywhere to start game)
    if (TouchController::get().isAnyTouchPressed()) {
        LOGI("[ObjMenu] START triggered from touch! Launching game level...");
        startingGame = true;
        GameController::get().startNewGame();
    }
}




void ObjMenu::onDrawGUI() {
    RendererGLES::get().set3DMode(false);
    int screenW = RendererGLES::get().getWidth();
    int screenH = RendererGLES::get().getHeight();
    float cx = screenW * 0.5f;

    // 1. Dark Retro Purple Background with scanline grid
    RendererGLES::get().drawRectangle(0.0f, 0.0f, screenW, screenH, 0xFF0A0014, false);

    // 2. Iconic POST VOID Title Header
    float postX = cx - 220.0f;
    float voidX = cx + 80.0f;
    float titleY = 120.0f;

    // "POST" Block Lettering
    RendererGLES::get().drawRectangle(postX, titleY, postX + 160.0f, titleY + 50.0f, postVoidCol, false);
    RendererGLES::get().drawRectangle(postX - 4.0f, titleY - 4.0f, postX + 164.0f, titleY + 54.0f, 0xFF000000, true);

    // "VOID" Block Lettering
    RendererGLES::get().drawRectangle(voidX, titleY, voidX + 160.0f, titleY + 50.0f, postVoidCol, false);
    RendererGLES::get().drawRectangle(voidX - 4.0f, titleY - 4.0f, voidX + 164.0f, titleY + 54.0f, 0xFF000000, true);

    // 3. Interactive Menu Option Buttons
    float startY = 240.0f;
    float sep = 60.0f;

    for (size_t i = 0; i < options.size(); i++) {
        float btnY = startY + (i * sep);
        bool isSel = (static_cast<int>(i) == optionSelected);

        float btnW = isSel ? 260.0f : 220.0f;
        float btnH = isSel ? 44.0f : 36.0f;
        uint32_t btnCol = isSel ? 0xFFFFDD00 : 0xFF330055;
        uint32_t borderCol = isSel ? 0xFFFFFFFF : 0xFF660099;

        // Button Background & Border
        RendererGLES::get().drawRectangle(cx - (btnW * 0.5f), btnY, cx + (btnW * 0.5f), btnY + btnH, btnCol, false);
        RendererGLES::get().drawRectangle(cx - (btnW * 0.5f), btnY, cx + (btnW * 0.5f), btnY + btnH, borderCol, true);

        // Highlight indicator for selected option
        if (isSel) {
            float pulse = std::abs(std::sin(tick * 0.2f)) * 8.0f;
            RendererGLES::get().drawRectangle(cx - (btnW * 0.5f) - 20.0f - pulse, btnY + 12.0f,
                                              cx - (btnW * 0.5f) - 6.0f, btnY + 28.0f, 0xFF00FFFF, false);
            RendererGLES::get().drawRectangle(cx + (btnW * 0.5f) + 6.0f, btnY + 12.0f,
                                              cx + (btnW * 0.5f) + 20.0f + pulse, btnY + 28.0f, 0xFF00FFFF, false);
        }
    }

    // Touch tap prompt
    RendererGLES::get().drawRectangle(cx - 140.0f, screenH - 60.0f, cx + 140.0f, screenH - 30.0f, 0x6600FFFF, false);
    RendererGLES::get().drawRectangle(cx - 140.0f, screenH - 60.0f, cx + 140.0f, screenH - 30.0f, 0xFF00FFFF, true);
}
