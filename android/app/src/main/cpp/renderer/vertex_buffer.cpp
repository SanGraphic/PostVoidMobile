#include "vertex_buffer.h"

VertexBuffer::VertexBuffer() {
    glGenVertexArrays(1, &m_vao);
    glGenBuffers(1, &m_vbo);
}

VertexBuffer::~VertexBuffer() {
    if (m_vbo) {
        glDeleteBuffers(1, &m_vbo);
        m_vbo = 0;
    }
    if (m_vao) {
        glDeleteVertexArrays(1, &m_vao);
        m_vao = 0;
    }
}

void VertexBuffer::begin() {
    m_vertices.clear();
    m_dirty = true;
}

void VertexBuffer::addVertex(float x, float y, float z, uint32_t color, float alpha, float u, float v) {
    GMS_Vertex vert;
    vert.x = x;
    vert.y = y;
    vert.z = z;

    vert.r = static_cast<uint8_t>(color & 0xFF);
    vert.g = static_cast<uint8_t>((color >> 8) & 0xFF);
    vert.b = static_cast<uint8_t>((color >> 16) & 0xFF);
    vert.a = static_cast<uint8_t>(alpha * 255.0f);

    vert.u = u;
    vert.v = v;

    m_vertices.push_back(vert);
    m_dirty = true;
}

void VertexBuffer::end() {
    if (m_vertices.empty()) return;

    glBindVertexArray(m_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);

    glBufferData(GL_ARRAY_BUFFER, m_vertices.size() * sizeof(GMS_Vertex), m_vertices.data(), GL_DYNAMIC_DRAW);

    // Position (location 0)
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(GMS_Vertex), (void*)offsetof(GMS_Vertex, x));
    glEnableVertexAttribArray(0);

    // Colour (location 1)
    glVertexAttribPointer(1, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(GMS_Vertex), (void*)offsetof(GMS_Vertex, r));
    glEnableVertexAttribArray(1);

    // Texcoord (location 2)
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(GMS_Vertex), (void*)offsetof(GMS_Vertex, u));
    glEnableVertexAttribArray(2);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    m_dirty = false;
}

void VertexBuffer::submit(GLenum primitiveType, GLuint textureId) {
    if (m_vertices.empty()) return;

    if (m_dirty) {
        end();
    }

    if (textureId > 0) {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, textureId);
    }

    glBindVertexArray(m_vao);
    glDrawArrays(primitiveType, 0, static_cast<GLsizei>(m_vertices.size()));
    glBindVertexArray(0);
}

void VertexBuffer::clear() {
    m_vertices.clear();
    m_dirty = true;
}
