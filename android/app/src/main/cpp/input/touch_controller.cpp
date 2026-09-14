#include "touch_controller.h"
#include "renderer_gles.h"
#include <cmath>

TouchController::TouchController() {}
TouchController::~TouchController() {}

void TouchController::handleTouchEvent(int pointerId, int action, float x, float y) {
    // Android MotionEvent actions: 0=DOWN, 1=UP, 2=MOVE, 5=POINTER_DOWN, 6=POINTER_UP, 3=CANCEL
    if (action == 0 || action == 5) {
        TouchPointer p;
        p.id = pointerId;
        p.startX = x;
        p.startY = y;
        p.currentX = x;
        p.currentY = y;
        p.isDown = true;
        m_pointers[pointerId] = p;

        float screenW = static_cast<float>(RendererGLES::get().getWidth());
        if (x < screenW * 0.5f) {
            if (m_movePointerId == -1) {
                m_movePointerId = pointerId;
            }
        } else {
            if (m_aimPointerId == -1) {
                m_aimPointerId = pointerId;
            }
            // Tap right side = Fire
            m_isFiring = true;
        }
    } else if (action == 2) {
        auto it = m_pointers.find(pointerId);
        if (it != m_pointers.end()) {
            float dx = x - it->second.currentX;
            float dy = y - it->second.currentY;

            it->second.currentX = x;
            it->second.currentY = y;

            if (pointerId == m_aimPointerId) {
                m_aimDeltaX += dx * 0.15f; // Aim sensitivity
                m_aimDeltaY += dy * 0.15f;
            } else if (pointerId == m_movePointerId) {
                float stickDx = x - it->second.startX;
                float stickDy = y - it->second.startY;
                float maxDist = 120.0f;

                float len = std::sqrt(stickDx * stickDx + stickDy * stickDy);
                if (len > maxDist) {
                    stickDx = (stickDx / len) * maxDist;
                    stickDy = (stickDy / len) * maxDist;
                }
                m_moveX = stickDx / maxDist;
                m_moveY = stickDy / maxDist;
            }
        }
    } else if (action == 1 || action == 6 || action == 3) {
        if (pointerId == m_movePointerId) {
            m_movePointerId = -1;
            m_moveX = 0.0f;
            m_moveY = 0.0f;
        }
        if (pointerId == m_aimPointerId) {
            m_aimPointerId = -1;
            m_isFiring = false;
        }
        m_pointers.erase(pointerId);
    }
}

void TouchController::update(float screenWidth, float screenHeight) {
    // Decay aim delta per frame
    m_aimDeltaX = 0.0f;
    m_aimDeltaY = 0.0f;
}

void TouchController::renderVirtualControls() {
    int screenW = RendererGLES::get().getWidth();
    int screenH = RendererGLES::get().getHeight();

    // 1. Move Stick (Bottom Left Area)
    float baseStickX = 220.0f;
    float baseStickY = screenH - 220.0f;
    float stickRadius = 90.0f;

    // Outer joystick ring
    RendererGLES::get().drawRectangle(baseStickX - stickRadius, baseStickY - stickRadius,
                                      baseStickX + stickRadius, baseStickY + stickRadius, 0x22FFFFFF, false);
    RendererGLES::get().drawRectangle(baseStickX - stickRadius, baseStickY - stickRadius,
                                      baseStickX + stickRadius, baseStickY + stickRadius, 0x6600FFFF, true);

    // Inner joystick thumb knob
    float knobX = baseStickX + (m_moveX * 60.0f);
    float knobY = baseStickY + (m_moveY * 60.0f);
    RendererGLES::get().drawRectangle(knobX - 35.0f, knobY - 35.0f, knobX + 35.0f, knobY + 35.0f, 0x8800FFFF, false);

    // 2. Action Buttons (Bottom Right Area)
    // FIRE button (Large Neon Red/Pink)
    float fireX = screenW - 200.0f;
    float fireY = screenH - 220.0f;
    uint32_t fireCol = m_isFiring ? 0xFFFF0055 : 0x66FF0055;
    RendererGLES::get().drawRectangle(fireX - 65.0f, fireY - 65.0f, fireX + 65.0f, fireY + 65.0f, fireCol, false);
    RendererGLES::get().drawRectangle(fireX - 65.0f, fireY - 65.0f, fireX + 65.0f, fireY + 65.0f, 0xFFFF0055, true);

    // RELOAD button (Cyan)
    float reloadX = screenW - 120.0f;
    float reloadY = screenH - 370.0f;
    uint32_t reloadCol = m_isReloading ? 0xFF00FFFF : 0x5500FFFF;
    RendererGLES::get().drawRectangle(reloadX - 45.0f, reloadY - 45.0f, reloadX + 45.0f, reloadY + 45.0f, reloadCol, false);
    RendererGLES::get().drawRectangle(reloadX - 45.0f, reloadY - 45.0f, reloadX + 45.0f, reloadY + 45.0f, 0xFF00FFFF, true);

    // JUMP button (Yellow)
    float jumpX = screenW - 320.0f;
    float jumpY = screenH - 160.0f;
    RendererGLES::get().drawRectangle(jumpX - 45.0f, jumpY - 45.0f, jumpX + 45.0f, jumpY + 45.0f, 0x55FFEE00, false);
    RendererGLES::get().drawRectangle(jumpX - 45.0f, jumpY - 45.0f, jumpX + 45.0f, jumpY + 45.0f, 0xFFFFEE00, true);
}

