#pragma once

#include <GLES3/gl3.h>
#include <vector>
#include <string>
#include "gms_types.h"
#include "shader_manager.h"
#include "vertex_buffer.h"

class RendererGLES {
public:
    static RendererGLES& get() {
        static RendererGLES instance;
        return instance;
    }

    void initialize(int width, int height);
    void shutdown();
    void resize(int width, int height);

    void beginFrame();
    void endFrame();

    // 2D / 3D State
    void set3DMode(bool enable);
    void setCamera(float camX, float camY, float camZ, float lookX, float lookY, float lookZ, float fov);

    // GMS Draw API wrappers
    void drawSpriteExt(int spriteId, int subimg, float x, float y, float xscale, float yscale, float rot, uint32_t color, float alpha);
    void drawSprite3D(int spriteId, int subimg, float x, float y, float z, float scale, uint32_t color, float alpha);
    void drawRectangle(float x1, float y1, float x2, float y2, uint32_t color, bool outline);
    void drawQuad3D(float x1, float y1, float z1, float x2, float y2, float z2, float x3, float y3, float z3, float x4, float y4, float z4, uint32_t color);

    // GPU State
    void setZTest(bool enable);
    void setZWrite(bool enable);
    void setBlendMode(int mode); // 0=normal, 1=add, 2=max, 3=subtract

    int getWidth() const { return m_width; }
    int getHeight() const { return m_height; }

private:
    RendererGLES();
    ~RendererGLES();

    void updateMatrices();
    void multiplyMat4(const float* a, const float* b, float* out);

    int m_width = 1920;
    int m_height = 1080;
    bool m_is3D = false;

    float m_camPos[3] = {0.0f, 0.0f, 0.0f};
    float m_camLook[3] = {0.0f, 0.0f, 0.0f};
    float m_fov = 80.0f;

    float m_matProj[16];
    float m_matView[16];
    float m_matWorldViewProj[16];

    GLuint m_defaultWhiteTexture = 0;
    VertexBuffer m_quadBuffer;

};
