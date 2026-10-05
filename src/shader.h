#ifndef SHADER_H
#define SHADER_H
#include <glad/glad.h>

/* Compila vertex+fragment e linka. Retorna 0 em caso de erro (log no stderr). */
GLuint shader_create(const char *vs_src, const char *fs_src);

/* Fontes GLSL embutidas (assim o executavel nao depende do diretorio atual) */
extern const char *BLOCK_VS, *BLOCK_FS;   /* blocos com textura + neblina */
extern const char *LINE_VS,  *LINE_FS;    /* linhas de cor solida (contorno, mira) */

#endif
