#include "world.h"
#include "noise.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* =================================================================== */
/*  Acesso a chunks e blocos                                           */
/* =================================================================== */

static Chunk *chunk_at(const World *w, int cx, int cz)
{
    return &w->chunks[cx * WORLD_CZ + cz];
}

BlockType world_get_block(const World *w, int x, int y, int z)
{
    if (x < 0 || z < 0 || y < 0 || y >= CHUNK_H) return BLOCK_AIR;
    int cx = x / CHUNK_W, cz = z / CHUNK_W;
    if (cx >= WORLD_CX || cz >= WORLD_CZ) return BLOCK_AIR;
    return (BlockType)chunk_at(w, cx, cz)->blocks[x % CHUNK_W][y][z % CHUNK_W];
}

int world_is_solid(const World *w, int x, int y, int z)
{
    if (x < 0 || z < 0 || x >= WORLD_SIZE_X || z >= WORLD_SIZE_Z || y < 0) return 1;
    if (y >= CHUNK_H) return 0;
    return world_get_block(w, x, y, z) != BLOCK_AIR;
}

void world_set_block(World *w, int x, int y, int z, BlockType b)
{
    if (x < 0 || z < 0 || y < 0 || y >= CHUNK_H) return;
    int cx = x / CHUNK_W, cz = z / CHUNK_W;
    if (cx >= WORLD_CX || cz >= WORLD_CZ) return;
    int lx = x % CHUNK_W, lz = z % CHUNK_W;

    Chunk *c = chunk_at(w, cx, cz);
    c->blocks[lx][y][lz] = (uint8_t)b;
    c->dirty = 1;

    /* se o bloco esta na borda, o chunk vizinho tambem muda de aparencia
       (as faces entre os dois chunks dependem desse bloco) */
    if (lx == 0           && cx > 0)           chunk_at(w, cx - 1, cz)->dirty = 1;
    if (lx == CHUNK_W - 1 && cx < WORLD_CX - 1) chunk_at(w, cx + 1, cz)->dirty = 1;
    if (lz == 0           && cz > 0)           chunk_at(w, cx, cz - 1)->dirty = 1;
    if (lz == CHUNK_W - 1 && cz < WORLD_CZ - 1) chunk_at(w, cx, cz + 1)->dirty = 1;
}

int world_surface_height(const World *w, int x, int z)
{
    for (int y = CHUNK_H - 1; y >= 0; y--)
        if (world_get_block(w, x, y, z) != BLOCK_AIR) return y;
    return -1;
}

/* =================================================================== */
/*  Geracao de terreno                                                 */
/* =================================================================== */

static void generate_chunk(World *w, int cx, int cz)
{
    Chunk *c = chunk_at(w, cx, cz);
    memset(c->blocks, 0, sizeof c->blocks);

    /* 1) relevo: altura por coluna vinda do ruido fractal */
    for (int x = 0; x < CHUNK_W; x++)
    for (int z = 0; z < CHUNK_W; z++) {
        int wx = cx * CHUNK_W + x, wz = cz * CHUNK_W + z;
        float n = fbm(wx * 0.015f, wz * 0.015f, w->seed, 4);
        int h = 30 + (int)((n - 0.5f) * 80.0f);
        if (h < 1) h = 1;
        if (h > CHUNK_H - 12) h = CHUNK_H - 12;

        int beach = (h <= SEA_LEVEL + 1);
        for (int y = 0; y <= h; y++) {
            BlockType b;
            if      (y == h)     b = beach ? BLOCK_SAND : BLOCK_GRASS;
            else if (y >= h - 3) b = beach ? BLOCK_SAND : BLOCK_DIRT;
            else                 b = BLOCK_STONE;
            c->blocks[x][y][z] = (uint8_t)b;
        }
    }

    /* 2) arvores. Margem de 2 blocos da borda do chunk: a copa (raio 2)
          nunca vaza para o chunk vizinho -> codigo bem mais simples. */
    for (int x = 2; x < CHUNK_W - 2; x++)
    for (int z = 2; z < CHUNK_W - 2; z++) {
        int wx = cx * CHUNK_W + x, wz = cz * CHUNK_W + z;
        if (hash2(wx, wz, w->seed ^ 0xBEEFu) % 90u != 0) continue;

        int ground = -1;
        for (int y = CHUNK_H - 1; y >= 0; y--)
            if (c->blocks[x][y][z] != BLOCK_AIR) { ground = y; break; }
        if (ground < 0 || c->blocks[x][ground][z] != BLOCK_GRASS) continue;

        int trunk = 4 + (int)((hash2(wx, wz, w->seed + 1) >> 8) % 2u);
        int top = ground + trunk;

        for (int ly = top - 2; ly <= top + 1; ly++) {          /* copa */
            int r = (ly >= top) ? 1 : 2;
            for (int dx = -r; dx <= r; dx++)
            for (int dz = -r; dz <= r; dz++) {
                if (r == 2 && abs(dx) == 2 && abs(dz) == 2) continue; /* arredonda */
                if (c->blocks[x + dx][ly][z + dz] == BLOCK_AIR)
                    c->blocks[x + dx][ly][z + dz] = BLOCK_LEAVES;
            }
        }
        for (int y = ground + 1; y <= top; y++)                /* tronco */
            c->blocks[x][y][z] = BLOCK_LOG;
    }
    c->dirty = 1;
}

World *world_create(uint32_t seed)
{
    World *w = calloc(1, sizeof *w);
    w->seed = seed;
    w->chunks = calloc((size_t)WORLD_CX * WORLD_CZ, sizeof(Chunk));
    for (int cx = 0; cx < WORLD_CX; cx++)
        for (int cz = 0; cz < WORLD_CZ; cz++)
            generate_chunk(w, cx, cz);
    return w;
}

void world_destroy(World *w)
{
    for (int i = 0; i < WORLD_CX * WORLD_CZ; i++) {
        if (w->chunks[i].vbo) glDeleteBuffers(1, &w->chunks[i].vbo);
        if (w->chunks[i].vao) glDeleteVertexArrays(1, &w->chunks[i].vao);
    }
    free(w->chunks);
    free(w);
}

/* =================================================================== */
/*  Geracao de mesh (face culling)                                     */
/* =================================================================== */

/* Cada face: vizinho a checar, 4 cantos (CCW visto de fora), 4 UVs, brilho.
   v = 1 aponta para cima nas laterais. */
typedef struct {
    int   dx, dy, dz;
    float v[4][3];
    float uv[4][2];
    float shade;
} Face;

static const Face FACES[6] = {
    /* +X */ { 1, 0, 0, {{1,0,0},{1,1,0},{1,1,1},{1,0,1}}, {{1,0},{1,1},{0,1},{0,0}}, 0.80f },
    /* -X */ {-1, 0, 0, {{0,0,1},{0,1,1},{0,1,0},{0,0,0}}, {{1,0},{1,1},{0,1},{0,0}}, 0.80f },
    /* +Y */ { 0, 1, 0, {{0,1,1},{1,1,1},{1,1,0},{0,1,0}}, {{0,0},{1,0},{1,1},{0,1}}, 1.00f },
    /* -Y */ { 0,-1, 0, {{0,0,0},{1,0,0},{1,0,1},{0,0,1}}, {{0,0},{1,0},{1,1},{0,1}}, 0.50f },
    /* +Z */ { 0, 0, 1, {{0,0,1},{1,0,1},{1,1,1},{0,1,1}}, {{0,0},{1,0},{1,1},{0,1}}, 0.65f },
    /* -Z */ { 0, 0,-1, {{1,0,0},{0,0,0},{0,1,0},{1,1,0}}, {{0,0},{1,0},{1,1},{0,1}}, 0.65f },
};
static const int QUAD_IDX[6] = {0, 1, 2, 0, 2, 3};   /* quad -> 2 triangulos */

#define FLOATS_PER_VERTEX 6   /* x y z u v shade */
#define UV_EPS 0.0005f        /* evita "sangrar" pixel do tile vizinho   */

typedef struct { float *d; size_t len, cap; } Vec;
static Vec g_vec;             /* buffer reutilizado a cada remontagem */

static void vec_push(Vec *v, const float *src, size_t n)
{
    if (v->len + n > v->cap) {
        v->cap = v->cap ? v->cap * 2 : 1 << 16;
        while (v->len + n > v->cap) v->cap *= 2;
        v->d = realloc(v->d, v->cap * sizeof(float));
    }
    memcpy(v->d + v->len, src, n * sizeof(float));
    v->len += n;
}

static void chunk_build_mesh(World *w, int cx, int cz)
{
    Chunk *c = chunk_at(w, cx, cz);
    g_vec.len = 0;

    for (int x = 0; x < CHUNK_W; x++)
    for (int y = 0; y < CHUNK_H; y++)
    for (int z = 0; z < CHUNK_W; z++) {
        BlockType b = (BlockType)c->blocks[x][y][z];
        if (b == BLOCK_AIR) continue;

        int wx = cx * CHUNK_W + x, wz = cz * CHUNK_W + z;

        for (int f = 0; f < 6; f++) {
            const Face *F = &FACES[f];
            if (y + F->dy < 0) continue;                 /* fundo do mundo: invisivel */
            if (world_get_block(w, wx + F->dx, y + F->dy, wz + F->dz) != BLOCK_AIR)
                continue;                                /* face escondida: pula */

            float u0 = (float)block_tile(b, f) / ATLAS_COLS;
            for (int k = 0; k < 6; k++) {
                int i = QUAD_IDX[k];
                float vert[FLOATS_PER_VERTEX] = {
                    wx + F->v[i][0], y + F->v[i][1], wz + F->v[i][2],
                    u0 + (UV_EPS + F->uv[i][0] * (1.0f - 2 * UV_EPS)) / ATLAS_COLS,
                    UV_EPS + F->uv[i][1] * (1.0f - 2 * UV_EPS),
                    F->shade
                };
                vec_push(&g_vec, vert, FLOATS_PER_VERTEX);
            }
        }
    }

    if (!c->vao) {                                       /* cria sob demanda */
        glGenVertexArrays(1, &c->vao);
        glGenBuffers(1, &c->vbo);
        glBindVertexArray(c->vao);
        glBindBuffer(GL_ARRAY_BUFFER, c->vbo);
        GLsizei stride = FLOATS_PER_VERTEX * sizeof(float);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void *)0);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, stride, (void *)(3 * sizeof(float)));
        glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, stride, (void *)(5 * sizeof(float)));
        glEnableVertexAttribArray(0);
        glEnableVertexAttribArray(1);
        glEnableVertexAttribArray(2);
    }
    glBindVertexArray(c->vao);
    glBindBuffer(GL_ARRAY_BUFFER, c->vbo);
    glBufferData(GL_ARRAY_BUFFER, g_vec.len * sizeof(float), g_vec.d, GL_STATIC_DRAW);
    c->vertex_count = (int)(g_vec.len / FLOATS_PER_VERTEX);
    c->dirty = 0;
}

void world_update_meshes(World *w, int budget)
{
    for (int cx = 0; cx < WORLD_CX && budget > 0; cx++)
    for (int cz = 0; cz < WORLD_CZ && budget > 0; cz++)
        if (chunk_at(w, cx, cz)->dirty) {
            chunk_build_mesh(w, cx, cz);
            budget--;
        }
}

void world_draw(const World *w)
{
    for (int i = 0; i < WORLD_CX * WORLD_CZ; i++) {
        const Chunk *c = &w->chunks[i];
        if (c->vertex_count == 0) continue;
        glBindVertexArray(c->vao);
        glDrawArrays(GL_TRIANGLES, 0, c->vertex_count);
    }
}

/* =================================================================== */
/*  Raycast                                                            */
/* =================================================================== */

RayHit world_raycast(const World *w, const vec3 origin, const vec3 dir, float max_dist)
{
    RayHit r = {0};
    int   cell[3], prev[3], step[3];
    float t_max[3], t_delta[3];

    for (int i = 0; i < 3; i++) {
        cell[i] = (int)floorf(origin[i]);
        if (dir[i] > 0)      { step[i] =  1; t_delta[i] =  1.0f / dir[i];
                               t_max[i] = (cell[i] + 1 - origin[i]) / dir[i]; }
        else if (dir[i] < 0) { step[i] = -1; t_delta[i] = -1.0f / dir[i];
                               t_max[i] = (origin[i] - cell[i]) / -dir[i]; }
        else                 { step[i] =  0; t_delta[i] = t_max[i] = INFINITY; }
        prev[i] = cell[i];
    }

    for (;;) {
        if (world_get_block(w, cell[0], cell[1], cell[2]) != BLOCK_AIR) {
            r.hit = 1;
            glm_ivec3_copy(cell, r.block);
            glm_ivec3_copy(prev, r.prev);
            return r;
        }
        /* avanca no eixo cuja proxima fronteira de celula esta mais perto */
        int a = (t_max[0] < t_max[1]) ? ((t_max[0] < t_max[2]) ? 0 : 2)
                                      : ((t_max[1] < t_max[2]) ? 1 : 2);
        if (t_max[a] > max_dist) return r;
        memcpy(prev, cell, sizeof prev);
        cell[a]  += step[a];
        t_max[a] += t_delta[a];
    }
}
