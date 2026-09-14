#pragma once

#include "gms_types.h"
#include <unordered_map>

struct TouchPointer {
    int id = -1;
    float startX = 0.0f;
    float startY = 0.0f;
    float currentX = 0.0f;
    float currentY = 0.0f;
    bool isDown = false;
};

class TouchController {
public:
    static TouchController& get() {
        static TouchController instance;
        return instance;
    }

    void handleTouchEvent(int pointerId, int action, float x, float y);
    void update(float screenWidth, float screenHeight);

    // Movement stick (-1.0 to 1.0)
    float getMoveX() const { return m_moveX; }
    float getMoveY() const { return m_moveY; }

    // Aim Delta
    float getAimDeltaX() const { return m_aimDeltaX; }
    float getAimDeltaY() const { return m_aimDeltaY; }

    // Action buttons
    bool isFiring() const { return m_isFiring; }
    bool isReloading() const { return m_isReloading; }
    bool isJumping() const { return m_isJumping; }
    bool isSliding() const { return m_isSliding; }
    bool isAnyTouchPressed() const { return !m_pointers.empty() || m_isFiring; }

    void renderVirtualControls();


private:
    TouchController();
    ~TouchController();

    std::unordered_map<int, TouchPointer> m_pointers;
    int m_movePointerId = -1;
    int m_aimPointerId = -1;

    float m_moveX = 0.0f;
    float m_moveY = 0.0f;
    float m_aimDeltaX = 0.0f;
    float m_aimDeltaY = 0.0f;

    bool m_isFiring = false;
    bool m_isReloading = false;
    bool m_isJumping = false;
    bool m_isSliding = false;
};
