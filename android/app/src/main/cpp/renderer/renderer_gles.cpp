#include "renderer_gles.h"
#include <cstring>
#include <cmath>

RendererGLES::RendererGLES() {
    std::memset(m_matProj, 0, sizeof(m_matProj));
    std::memset(m_matView, 0, sizeof(m_matView));
    std::memset(m_matWorldViewProj, 0, sizeof(m_matWorldViewProj));
}

RendererGLES::~RendererGLES() {}

void RendererGLES::initialize(int width, int height) {
    LOGI("[RendererGLES] Initializing OpenGLES 3.0 Renderer (%dx%d)...", width, height);
    m_width = width;
    m_height = height;

    glViewport(0, 0, width, height);
    glClearColor(0.05f, 0.0f, 0.1f, 1.0f); // Deep retro purple/black

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // Create 1x1 Solid White Texture
    glGenTextures(1, &m_defaultWhiteTexture);
    glBindTexture(GL_TEXTURE_2D, m_defaultWhiteTexture);
    uint32_t whitePixel = 0xFFFFFFFF;
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, &whitePixel);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glBindTexture(GL_TEXTURE_2D, 0);

    ShaderManager::get().initialize();
    set3DMode(false);
}

void RendererGLES::shutdown() {
    if (m_defaultWhiteTexture) {
        glDeleteTextures(1, &m_defaultWhiteTexture);
        m_defaultWhiteTexture = 0;
    }
    ShaderManager::get().shutdown();
}

void RendererGLES::resize(int width, int height) {
    m_width = width;
    m_height = height;
    glViewport(0, 0, width, height);
    updateMatrices();
}

void RendererGLES::beginFrame() {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void RendererGLES::endFrame() {
    glFlush();
}

void RendererGLES::set3DMode(bool enable) {
    m_is3D = enable;
    if (enable) {
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LEQUAL);
        glDepthMask(GL_TRUE);
        ShaderManager::get().useShader("shd_3D");
    } else {
        glDisable(GL_DEPTH_TEST);
        glDepthMask(GL_FALSE);
        ShaderManager::get().useShader("default");
    }
    updateMatrices();
}


void RendererGLES::setCamera(float camX, float camY, float camZ, float lookX, float lookY, float lookZ, float fov) {
    m_camPos[0] = camX;
    m_camPos[1] = camY;
    m_camPos[2] = camZ;

    m_camLook[0] = lookX;
    m_camLook[1] = lookY;
    m_camLook[2] = lookZ;

    m_fov = fov;
    updateMatrices();
}

void RendererGLES::multiplyMat4(const float* a, const float* b, float* out) {
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            out[j * 4 + i] = 
                a[0 * 4 + i] * b[j * 4 + 0] +
                a[1 * 4 + i] * b[j * 4 + 1] +
                a[2 * 4 + i] * b[j * 4 + 2] +
                a[3 * 4 + i] * b[j * 4 + 3];
        }
    }
}

void RendererGLES::updateMatrices() {
    if (!m_is3D) {
        // 2D Ortho Matrix
        float l = 0.0f, r = static_cast<float>(m_width);
        float b = static_cast<float>(m_height), t = 0.0f;
        float n = -100.0f, f = 100.0f;

        std::memset(m_matWorldViewProj, 0, sizeof(m_matWorldViewProj));
        m_matWorldViewProj[0] = 2.0f / (r - l);
        m_matWorldViewProj[5] = 2.0f / (t - b);
        m_matWorldViewProj[10] = -2.0f / (f - n);
        m_matWorldViewProj[12] = -(r + l) / (r - l);
        m_matWorldViewProj[13] = -(t + b) / (t - b);
        m_matWorldViewProj[14] = -(f + n) / (f - n);
        m_matWorldViewProj[15] = 1.0f;
    } else {
        // 3D Perspective Matrix
        float aspect = static_cast<float>(m_width) / static_cast<float>(m_height > 0 ? m_height : 1);
        float fovRad = m_fov * 3.14159265f / 180.0f;
        float tanHalfFov = std::tan(fovRad / 2.0f);
        float zNear = 1.0f;
        float zFar = 5000.0f;

        std::memset(m_matProj, 0, sizeof(m_matProj));
        m_matProj[0] = 1.0f / (aspect * tanHalfFov);
        m_matProj[5] = 1.0f / tanHalfFov;
        m_matProj[10] = -(zFar + zNear) / (zFar - zNear);
        m_matProj[11] = -1.0f;
        m_matProj[14] = -(2.0f * zFar * zNear) / (zFar - zNear);

        // 3D LookAt View Matrix
        float fx = m_camLook[0] - m_camPos[0];
        float fy = m_camLook[1] - m_camPos[1];
        float fz = m_camLook[2] - m_camPos[2];
        float fLen = std::sqrt(fx*fx + fy*fy + fz*fz);
        if (fLen > 0.0001f) { fx /= fLen; fy /= fLen; fz /= fLen; }

        // Up vector (0, 0, 1) in GMS Z-up coordinate space
        float upX = 0.0f, upY = 0.0f, upZ = 1.0f;
        float rx = fy * upZ - fz * upY;
        float ry = fz * upX - fx * upZ;
        float rz = fx * upY - fy * upX;
        float rLen = std::sqrt(rx*rx + ry*ry + rz*rz);
        if (rLen > 0.0001f) { rx /= rLen; ry /= rLen; rz /= rLen; }

        float ux = ry * fz - rz * fy;
        float uy = rz * fx - rx * fz;
        float uz = rx * fy - ry * fx;

        std::memset(m_matView, 0, sizeof(m_matView));
        m_matView[0] = rx;  m_matView[4] = ry;  m_matView[8]  = rz;  m_matView[12] = -(rx*m_camPos[0] + ry*m_camPos[1] + rz*m_camPos[2]);
        m_matView[1] = ux;  m_matView[5] = uy;  m_matView[9]  = uz;  m_matView[13] = -(ux*m_camPos[0] + uy*m_camPos[1] + uz*m_camPos[2]);
        m_matView[2] = -fx; m_matView[6] = -fy; m_matView[10] = -fz; m_matView[14] = (fx*m_camPos[0] + fy*m_camPos[1] + fz*m_camPos[2]);
        m_matView[15] = 1.0f;

        // Multiply Proj * View
        multiplyMat4(m_matProj, m_matView, m_matWorldViewProj);
    }

    ShaderProgram* current = ShaderManager::get().getShader(m_is3D ? "shd_3D" : "default");
    if (current && current->program) {
        glUseProgram(current->program);
        GLint uMat = glGetUniformLocation(current->program, "u_MatrixWorldViewProj");
        if (uMat >= 0) {
            glUniformMatrix4fv(uMat, 1, GL_FALSE, m_matWorldViewProj);
        }
        if (m_is3D) {
            GLint uCam = glGetUniformLocation(current->program, "u_CameraPos");
            if (uCam >= 0) {
                glUniform3fv(uCam, 1, m_camPos);
            }
            GLint uFog = glGetUniformLocation(current->program, "fog_col");
            if (uFog >= 0) {
                glUniform3f(uFog, 0.15f, 0.0f, 0.25f);
            }
        }
    }
}

void RendererGLES::drawQuad3D(float x1, float y1, float z1, float x2, float y2, float z2, float x3, float y3, float z3, float x4, float y4, float z4, uint32_t color) {
    m_quadBuffer.begin();
    m_quadBuffer.addVertex(x1, y1, z1, color, 1.0f, 0.0f, 0.0f);
    m_quadBuffer.addVertex(x2, y2, z2, color, 1.0f, 1.0f, 0.0f);
    m_quadBuffer.addVertex(x3, y3, z3, color, 1.0f, 0.0f, 1.0f);

    m_quadBuffer.addVertex(x2, y2, z2, color, 1.0f, 1.0f, 0.0f);
    m_quadBuffer.addVertex(x4, y4, z4, color, 1.0f, 1.0f, 1.0f);
    m_quadBuffer.addVertex(x3, y3, z3, color, 1.0f, 0.0f, 1.0f);
    m_quadBuffer.end();

    m_quadBuffer.submit(GL_TRIANGLES, m_defaultWhiteTexture);
}

void RendererGLES::drawSpriteExt(int spriteId, int subimg, float x, float y, float xscale, float yscale, float rot, uint32_t color, float alpha) {
    float w = 64.0f * xscale;
    float h = 64.0f * yscale;

    m_quadBuffer.begin();
    m_quadBuffer.addVertex(x, y, 0.0f, color, alpha, 0.0f, 0.0f);
    m_quadBuffer.addVertex(x + w, y, 0.0f, color, alpha, 1.0f, 0.0f);
    m_quadBuffer.addVertex(x, y + h, 0.0f, color, alpha, 0.0f, 1.0f);

    m_quadBuffer.addVertex(x + w, y, 0.0f, color, alpha, 1.0f, 0.0f);
    m_quadBuffer.addVertex(x + w, y + h, 0.0f, color, alpha, 1.0f, 1.0f);
    m_quadBuffer.addVertex(x, y + h, 0.0f, color, alpha, 0.0f, 1.0f);
    m_quadBuffer.end();

    m_quadBuffer.submit(GL_TRIANGLES, m_defaultWhiteTexture);
}

void RendererGLES::drawSprite3D(int spriteId, int subimg, float x, float y, float z, float scale, uint32_t color, float alpha) {
    float size = 32.0f * scale;

    m_quadBuffer.begin();
    m_quadBuffer.addVertex(x - size, y, z - size, color, alpha, 0.0f, 0.0f);
    m_quadBuffer.addVertex(x + size, y, z - size, color, alpha, 1.0f, 0.0f);
    m_quadBuffer.addVertex(x - size, y, z + size, color, alpha, 0.0f, 1.0f);

    m_quadBuffer.addVertex(x + size, y, z - size, color, alpha, 1.0f, 0.0f);
    m_quadBuffer.addVertex(x + size, y, z + size, color, alpha, 1.0f, 1.0f);
    m_quadBuffer.addVertex(x - size, y, z + size, color, alpha, 0.0f, 1.0f);
    m_quadBuffer.end();

    m_quadBuffer.submit(GL_TRIANGLES, m_defaultWhiteTexture);
}

void RendererGLES::drawRectangle(float x1, float y1, float x2, float y2, uint32_t color, bool outline) {
    m_quadBuffer.begin();
    if (!outline) {
        m_quadBuffer.addVertex(x1, y1, 0.0f, color, 1.0f, 0.0f, 0.0f);
        m_quadBuffer.addVertex(x2, y1, 0.0f, color, 1.0f, 0.0f, 0.0f);
        m_quadBuffer.addVertex(x1, y2, 0.0f, color, 1.0f, 0.0f, 0.0f);

        m_quadBuffer.addVertex(x2, y1, 0.0f, color, 1.0f, 0.0f, 0.0f);
        m_quadBuffer.addVertex(x2, y2, 0.0f, color, 1.0f, 0.0f, 0.0f);
        m_quadBuffer.addVertex(x1, y2, 0.0f, color, 1.0f, 0.0f, 0.0f);
        m_quadBuffer.end();
        m_quadBuffer.submit(GL_TRIANGLES, m_defaultWhiteTexture);
    } else {
        m_quadBuffer.addVertex(x1, y1, 0.0f, color, 1.0f, 0.0f, 0.0f);
        m_quadBuffer.addVertex(x2, y1, 0.0f, color, 1.0f, 0.0f, 0.0f);
        m_quadBuffer.addVertex(x2, y2, 0.0f, color, 1.0f, 0.0f, 0.0f);
        m_quadBuffer.addVertex(x1, y2, 0.0f, color, 1.0f, 0.0f, 0.0f);
        m_quadBuffer.end();
        m_quadBuffer.submit(GL_LINE_LOOP, m_defaultWhiteTexture);
    }
}


void RendererGLES::setZTest(bool enable) {
    if (enable) glEnable(GL_DEPTH_TEST);
    else glDisable(GL_DEPTH_TEST);
}

void RendererGLES::setZWrite(bool enable) {
    glDepthMask(enable ? GL_TRUE : GL_FALSE);
}

void RendererGLES::setBlendMode(int mode) {
    if (mode == 1) { // bm_add
        glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    } else if (mode == 2) { // bm_max
        glBlendEquation(GL_MAX);
    } else if (mode == 3) { // bm_subtract
        glBlendFunc(GL_ZERO, GL_ONE_MINUS_SRC_COLOR);
    } else { // bm_normal
        glBlendEquation(GL_FUNC_ADD);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    }
}
