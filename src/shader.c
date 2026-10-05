#include "shader.h"
#include <stdio.h>

static GLuint compile(GLenum type, const char *src)
{
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, NULL);
    glCompileShader(s);

    GLint ok;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[1024];
        glGetShaderInfoLog(s, sizeof log, NULL, log);
        fprintf(stderr, "[shader] erro de compilacao (%s):\n%s\n",
                type == GL_VERTEX_SHADER ? "vertex" : "fragment", log);
        glDeleteShader(s);
        return 0;
    }
    return s;
}

GLuint shader_create(const char *vs_src, const char *fs_src)
{
    GLuint vs = compile(GL_VERTEX_SHADER, vs_src);
    GLuint fs = compile(GL_FRAGMENT_SHADER, fs_src);
    if (!vs || !fs) return 0;

    GLuint prog = glCreateProgram();
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);
    glLinkProgram(prog);
    glDeleteShader(vs);   /* ja foram copiados para o programa */
    glDeleteShader(fs);

    GLint ok;
    glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[1024];
        glGetProgramInfoLog(prog, sizeof log, NULL, log);
        fprintf(stderr, "[shader] erro de link:\n%s\n", log);
        glDeleteProgram(prog);
        return 0;
    }
    return prog;
}

/* ---------------------------- blocos ------------------------------ */
const char *BLOCK_VS =
"#version 330 core\n"
"layout(location = 0) in vec3  aPos;    /* ja em coordenadas de MUNDO */\n"
"layout(location = 1) in vec2  aUV;\n"
"layout(location = 2) in float aShade;  /* brilho fixo por direcao da face */\n"
"uniform mat4 uVP;\n"
"out vec2  vUV;\n"
"out float vShade;\n"
"out vec3  vWorld;\n"
"void main() {\n"
"    vUV = aUV; vShade = aShade; vWorld = aPos;\n"
"    gl_Position = uVP * vec4(aPos, 1.0);\n"
"}\n";

const char *BLOCK_FS =
"#version 330 core\n"
"in vec2  vUV;\n"
"in float vShade;\n"
"in vec3  vWorld;\n"
"uniform sampler2D uAtlas;\n"
"uniform vec3 uCamPos;\n"
"uniform vec3 uFogColor;\n"
"uniform vec2 uFogRange;   /* x = inicio, y = fim da neblina */\n"
"out vec4 FragColor;\n"
"void main() {\n"
"    vec4 t = texture(uAtlas, vUV);\n"
"    if (t.a < 0.5) discard;           /* pronto p/ texturas com buracos */\n"
"    vec3 c = t.rgb * vShade;\n"
"    float d = length(vWorld - uCamPos);\n"
"    float f = clamp((d - uFogRange.x) / (uFogRange.y - uFogRange.x), 0.0, 1.0);\n"
"    FragColor = vec4(mix(c, uFogColor, f), 1.0);\n"
"}\n";

/* ----------------------- linhas de cor solida --------------------- */
const char *LINE_VS =
"#version 330 core\n"
"layout(location = 0) in vec3 aPos;\n"
"uniform mat4 uMVP;\n"
"void main() { gl_Position = uMVP * vec4(aPos, 1.0); }\n";

const char *LINE_FS =
"#version 330 core\n"
"uniform vec3 uColor;\n"
"out vec4 FragColor;\n"
"void main() { FragColor = vec4(uColor, 1.0); }\n";
