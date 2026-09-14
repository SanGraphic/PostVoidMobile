#include "shader_manager.h"

// Default GMS2 GLSL ES Vertex Shader
static const char* PASSTHROUGH_VERT = R"(#version 300 es
layout(location = 0) in vec3 in_Position;
layout(location = 1) in vec4 in_Colour;
layout(location = 2) in vec2 in_TextureCoord;

out vec4 v_vColour;
out vec2 v_vTexcoord;

uniform mat4 u_MatrixWorldViewProj;

void main() {
    v_vColour = in_Colour;
    v_vTexcoord = in_TextureCoord;
    gl_Position = u_MatrixWorldViewProj * vec4(in_Position, 1.0);
}
)";

// Default GMS2 GLSL ES Fragment Shader
static const char* PASSTHROUGH_FRAG = R"(#version 300 es
precision mediump float;

in vec4 v_vColour;
in vec2 v_vTexcoord;
out vec4 fragColor;

uniform sampler2D gm_BaseTexture;
uniform bool u_HasTexture;

void main() {
    vec4 texCol = u_HasTexture ? texture(gm_BaseTexture, v_vTexcoord) : vec4(1.0);
    fragColor = v_vColour * texCol;
}
)";

// Post Void 3D Core Shader
static const char* SHD_3D_VERT = R"(#version 300 es
layout(location = 0) in vec3 in_Position;
layout(location = 1) in vec4 in_Colour;
layout(location = 2) in vec2 in_TextureCoord;

out vec4 v_vColour;
out vec2 v_vTexcoord;
out vec3 v_vPosition;
out float fog_factor;

uniform mat4 u_MatrixWorldViewProj;
uniform vec3 u_CameraPos;

void main() {
    v_vColour = in_Colour;
    v_vTexcoord = in_TextureCoord;
    v_vPosition = in_Position;

    float dist = distance(in_Position, u_CameraPos);
    fog_factor = clamp(dist / 3500.0, 0.0, 1.0);

    gl_Position = u_MatrixWorldViewProj * vec4(in_Position, 1.0);
}
)";

static const char* SHD_3D_FRAG = R"(#version 300 es
precision mediump float;

in vec4 v_vColour;
in vec2 v_vTexcoord;
in vec3 v_vPosition;
in float fog_factor;

out vec4 fragColor;

uniform sampler2D gm_BaseTexture;
uniform vec3 fog_col;
uniform bool u_HasTexture;

void main() {
    vec4 base_col = u_HasTexture ? texture(gm_BaseTexture, v_vTexcoord) : vec4(1.0);
    if (u_HasTexture && base_col.a < 0.1) {
        discard;
    }
    vec3 col = mix(base_col.rgb, fog_col, fog_factor);
    fragColor = v_vColour * vec4(col, base_col.a);
}
)";


ShaderManager::ShaderManager() {}
ShaderManager::~ShaderManager() {}

void ShaderManager::initialize() {
    LOGI("[ShaderManager] Compiling and linking GMS GLSL ES shaders...");
    GLuint progDefault = linkProgram(PASSTHROUGH_VERT, PASSTHROUGH_FRAG);
    if (progDefault) {
        ShaderProgram sp;
        sp.program = progDefault;
        sp.u_BaseTexture = glGetUniformLocation(progDefault, "gm_BaseTexture");
        sp.u_AlphaTestEnabled = glGetUniformLocation(progDefault, "gm_AlphaTestEnabled");
        sp.u_AlphaRefValue = glGetUniformLocation(progDefault, "gm_AlphaRefValue");
        m_shaders["default"] = sp;
    }

    GLuint prog3D = linkProgram(SHD_3D_VERT, SHD_3D_FRAG);
    if (prog3D) {
        ShaderProgram sp;
        sp.program = prog3D;
        sp.u_BaseTexture = glGetUniformLocation(prog3D, "gm_BaseTexture");
        sp.u_FogCol = glGetUniformLocation(prog3D, "fog_col");
        m_shaders["shd_3D"] = sp;
    }

    LOGI("[ShaderManager] Shader compilation complete.");
}

void ShaderManager::shutdown() {
    for (auto& pair : m_shaders) {
        if (pair.second.program != 0) {
            glDeleteProgram(pair.second.program);
            pair.second.program = 0;
        }
    }
    m_shaders.clear();
}

GLuint ShaderManager::compileShader(GLenum type, const char* source) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);

    GLint compiled = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    if (!compiled) {
        GLint infoLen = 0;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &infoLen);
        if (infoLen > 0) {
            std::vector<char> infoLog(infoLen);
            glGetShaderInfoLog(shader, infoLen, nullptr, infoLog.data());
            LOGE("[ShaderManager] Shader compile failed:\n%s", infoLog.data());
        }
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

GLuint ShaderManager::linkProgram(const char* vertSrc, const char* fragSrc) {
    GLuint vertShader = compileShader(GL_VERTEX_SHADER, vertSrc);
    if (!vertShader) return 0;

    GLuint fragShader = compileShader(GL_FRAGMENT_SHADER, fragSrc);
    if (!fragShader) {
        glDeleteShader(vertShader);
        return 0;
    }

    GLuint prog = glCreateProgram();
    glAttachShader(prog, vertShader);
    glAttachShader(prog, fragShader);
    glLinkProgram(prog);

    GLint linked = 0;
    glGetProgramiv(prog, GL_LINK_STATUS, &linked);
    if (!linked) {
        GLint infoLen = 0;
        glGetProgramiv(prog, GL_INFO_LOG_LENGTH, &infoLen);
        if (infoLen > 0) {
            std::vector<char> infoLog(infoLen);
            glGetProgramInfoLog(prog, infoLen, nullptr, infoLog.data());
            LOGE("[ShaderManager] Program link failed:\n%s", infoLog.data());
        }
        glDeleteProgram(prog);
        prog = 0;
    }

    glDeleteShader(vertShader);
    glDeleteShader(fragShader);
    return prog;
}

void ShaderManager::useShader(const std::string& name) {
    auto it = m_shaders.find(name);
    if (it != m_shaders.end()) {
        glUseProgram(it->second.program);
        m_currentShader = name;
    } else {
        auto def = m_shaders.find("default");
        if (def != m_shaders.end()) {
            glUseProgram(def->second.program);
            m_currentShader = "default";
        }
    }
}

ShaderProgram* ShaderManager::getShader(const std::string& name) {
    auto it = m_shaders.find(name);
    if (it != m_shaders.end()) return &it->second;
    return nullptr;
}
