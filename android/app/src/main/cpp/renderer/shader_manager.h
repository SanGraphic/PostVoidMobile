#pragma once

#include <GLES3/gl3.h>
#include <string>
#include <unordered_map>
#include <vector>
#include "gms_types.h"

struct ShaderProgram {
    GLuint program = 0;
    GLint u_BaseTexture = -1;
    GLint u_FogEnabled = -1;
    GLint u_FogColour = -1;
    GLint u_FogCol = -1;
    GLint u_AlphaTestEnabled = -1;
    GLint u_AlphaRefValue = -1;
    GLint u_Time = -1;
    GLint u_Resolution = -1;
};

class ShaderManager {
public:
    static ShaderManager& get() {
        static ShaderManager instance;
        return instance;
    }

    void initialize();
    void shutdown();

    GLuint compileShader(GLenum type, const char* source);
    GLuint linkProgram(const char* vertSrc, const char* fragSrc);

    void useShader(const std::string& name);
    ShaderProgram* getShader(const std::string& name);

    void setUniformMatrix4fv(GLint location, const float* matrix);
    void setUniform1f(GLint location, float val);
    void setUniform2f(GLint location, float x, float y);
    void setUniform3f(GLint location, float x, float y, float z);
    void setUniform4f(GLint location, float x, float y, float z, float w);

private:
    ShaderManager();
    ~ShaderManager();

    std::unordered_map<std::string, ShaderProgram> m_shaders;
    std::string m_currentShader;
};
