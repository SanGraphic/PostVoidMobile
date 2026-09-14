#include "game_controller.h"

GameController::GameController() {}
GameController::~GameController() {}

void GameController::startNewGame() {
    LOGI("[GameController] Starting new Post Void run...");
    m_level = 1;
    m_score = 0;
    m_inGame = true;

    generateLevel(m_level);
}

void GameController::generateLevel(int levelNumber) {
    LOGI("[GameController] Generating procedural stage %d...", levelNumber);

    // Spawn Player at Origin
    auto player = std::make_shared<ObjPlayer>(100000, 0, "obj_player");
    player->x = 0.0;
    player->y = 50.0;
    player->z = 0.0;
    player->direction = 90.0; // Look forward down corridor (+Y)
    GMS_Runtime::get().addInstance(player);

    // Spawn procedural enemies along corridor
    for (int i = 1; i <= 10 + (levelNumber * 2); i++) {
        double ex = GMSMath::gms_random_range(-120.0, 120.0);
        double ey = 200.0 + (i * 220.0);

        auto enemy = std::make_shared<ObjEnemyWalker>(100000 + i, 1, "obj_enemy_walker");
        enemy->x = ex;
        enemy->y = ey;
        enemy->z = 0.0;
        GMS_Runtime::get().addInstance(enemy);
    }

    // Spawn Goal Portal at end of corridor
    double goalY = (12 + (levelNumber * 2)) * 220.0;
    auto goal = std::make_shared<ObjGoal>(200000, 2, "obj_goal");
    goal->x = 0.0;
    goal->y = goalY;
    goal->z = 0.0;
    GMS_Runtime::get().addInstance(goal);

    LOGI("[GameController] Level %d generated with Goal at y=%.1f", levelNumber, goalY);
}


void GameController::update() {
    if (m_inGame) {
        // Core game logic update handled by GMS_Runtime instance loop
    }
}

void GameController::render() {
    if (m_inGame) {
        RendererGLES::get().set3DMode(true);

        // Floor: Multi-section checkered synthwave corridor
        float corridorWidth = 250.0f;
        float corridorLength = 4000.0f;
        float wallHeight = 120.0f;

        // Render Floor Tiles
        for (float y = 0.0f; y < corridorLength; y += 100.0f) {
            uint32_t floorCol = (static_cast<int>(y / 100.0f) % 2 == 0) ? 0xFF2A0044 : 0xFF150022;
            RendererGLES::get().drawQuad3D(-corridorWidth, y, 0.0f,
                                          corridorWidth, y, 0.0f,
                                          -corridorWidth, y + 100.0f, 0.0f,
                                          corridorWidth, y + 100.0f, 0.0f,
                                          floorCol);
        }

        // Left Wall (Neon Magenta)
        for (float y = 0.0f; y < corridorLength; y += 200.0f) {
            uint32_t wallCol = (static_cast<int>(y / 200.0f) % 2 == 0) ? 0xFFFF0066 : 0xFFCC0044;
            RendererGLES::get().drawQuad3D(-corridorWidth, y, 0.0f,
                                          -corridorWidth, y + 200.0f, 0.0f,
                                          -corridorWidth, y, wallHeight,
                                          -corridorWidth, y + 200.0f, wallHeight,
                                          wallCol);
        }

        // Right Wall (Electric Yellow/Cyan)
        for (float y = 0.0f; y < corridorLength; y += 200.0f) {
            uint32_t wallCol = (static_cast<int>(y / 200.0f) % 2 == 0) ? 0xFF00FFCC : 0xFF00BBAA;
            RendererGLES::get().drawQuad3D(corridorWidth, y, 0.0f,
                                          corridorWidth, y + 200.0f, 0.0f,
                                          corridorWidth, y, wallHeight,
                                          corridorWidth, y + 200.0f, wallHeight,
                                          wallCol);
        }

        // Ceiling (Deep Indigo)
        RendererGLES::get().drawQuad3D(-corridorWidth, 0.0f, wallHeight,
                                      corridorWidth, 0.0f, wallHeight,
                                      -corridorWidth, corridorLength, wallHeight,
                                      corridorWidth, corridorLength, wallHeight,
                                      0xFF110033);
    }
}

