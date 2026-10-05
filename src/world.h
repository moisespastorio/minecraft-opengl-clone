#ifndef WORLD_H
#define WORLD_H
#include <stdint.h>
#include <glad/glad.h>
#include <cglm/cglm.h>
#include "blocks.h"

#define CHUNK_W      16
#define CHUNK_H      64
#define WORLD_CX     8                      /* chunks no eixo X */
#define WORLD_CZ     8                      /* chunks no eixo Z */
#define WORLD_SIZE_X (WORLD_CX * CHUNK_W)
#define WORLD_SIZE_Z (WORLD_CZ * CHUNK_W)
#define SEA_LEVEL    22

typedef struct {
    uint8_t blocks[CHUNK_W][CHUNK_H][CHUNK_W];  /* [x][y][z] */
    GLuint  vao, vbo;
    int     vertex_count;
    int     dirty;                              /* precisa remontar o mesh? */
} Chunk;

typedef struct {
    Chunk   *chunks;                            /* indice = cx * WORLD_CZ + cz */
    uint32_t seed;
} World;

typedef struct {
    int   hit;
    ivec3 block;   /* bloco atingido                      */
    ivec3 prev;    /* ultima celula vazia antes do bloco  */
} RayHit;

World *world_create(uint32_t seed);
void   world_destroy(World *w);

/* AIR fora dos limites */
BlockType world_get_block(const World *w, int x, int y, int z);
/* para colisao: fora do mundo (lados e fundo) conta como solido */
int        world_is_solid(const World *w, int x, int y, int z);
/* muda o bloco e marca chunk (e vizinhos, se na borda) para remontar */
void       world_set_block(World *w, int x, int y, int z, BlockType b);
/* y do bloco mais alto na coluna (x,z), ou -1 */
int        world_surface_height(const World *w, int x, int z);

/* remonta ate 'budget' chunks sujos */
void       world_update_meshes(World *w, int budget);
void       world_draw(const World *w);

/* DDA (Amanatides & Woo): percorre a grade voxel a voxel ao longo do raio */
RayHit     world_raycast(const World *w, const vec3 origin, const vec3 dir, float max_dist);

#endif
