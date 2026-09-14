#pragma once

#include "gms_runtime.h"
#include "game_objects.h"
#include "renderer_gles.h"
#include "oboe_audio_engine.h"

class GameController {
public:
    static GameController& get() {
        static GameController instance;
        return instance;
    }

    void startNewGame();
    void generateLevel(int levelNumber);
    void update();
    void render();

    int getCurrentLevel() const { return m_level; }
    int getScore() const { return m_score; }
    void addScore(int points) { m_score += points; }

private:
    GameController();
    ~GameController();

    int m_level = 1;
    int m_score = 0;
    bool m_inGame = false;
};
