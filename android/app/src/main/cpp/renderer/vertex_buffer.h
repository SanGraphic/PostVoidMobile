#pragma once

#include <GLES3/gl3.h>
#include <vector>
#include <cstdint>

struct GMS_Vertex {
    float x = 0.0f, y = 0.0f, z = 0.0f;
    uint8_t r = 255, g = 255, b = 255, a = 255;
    float u = 0.0f, v = 0.0f;
};

class VertexBuffer {
public:
    VertexBuffer();
    ~VertexBuffer();

    void begin();
    void addVertex(float x, float y, float z, uint32_t color, float alpha, float u, float v);
    void end();

    void submit(GLenum primitiveType, GLuint textureId);
    void clear();

    size_t getVertexCount() const { return m_vertices.size(); }

private:
    std::vector<GMS_Vertex> m_vertices;
    GLuint m_vao = 0;
    GLuint m_vbo = 0;
    bool m_dirty = true;
};
