#include "texture.h"
#include "blocks.h"
#include "noise.h"
#include <stdint.h>
#include <stdlib.h>

#define ATLAS_W (ATLAS_COLS * TILE_PX)
#define ATLAS_H (TILE_PX)

static uint8_t clamp8(int v) { return (uint8_t)(v < 0 ? 0 : v > 255 ? 255 : v); }

static void put(uint8_t *img, int tile, int x, int y, int r, int g, int b)
{
    uint8_t *p = img + ((size_t)y * ATLAS_W + tile * TILE_PX + x) * 4;
    p[0] = clamp8(r); p[1] = clamp8(g); p[2] = clamp8(b); p[3] = 255;
}

/*
 * ATENCAO a orientacao: o OpenGL considera a PRIMEIRA linha enviada como
 * v = 0. Aqui y = 15 e' o TOPO visual do tile (e v = 1 no mesh aponta p/ cima).
 */
GLuint atlas_create(void)
{
    uint8_t *img = calloc((size_t)ATLAS_W * ATLAS_H * 4, 1);

    for (int tile = 0; tile < TILE_COUNT; tile++)
    for (int y = 0; y < TILE_PX; y++)
    for (int x = 0; x < TILE_PX; x++) {
        int n = (int)(hash2(x, y, tile * 17u + 3u) % 33u) - 16;  /* ruido -16..16 */
        int r = 255, g = 0, b = 255;                              /* magenta = erro */

        switch (tile) {
        case TILE_GRASS_TOP:  r = 92 + n;  g = 156 + n; b = 56 + n / 2; break;
        case TILE_DIRT:       r = 134 + n; g = 96 + n;  b = 67 + n;     break;
        case TILE_STONE:
            r = g = b = 125 + n;
            if (hash2(x, y, 77) % 23 == 0) r = g = b = 95;   /* pintinhas escuras */
            break;
        case TILE_SAND:       r = 219 + n / 2; g = 207 + n / 2; b = 142 + n / 2; break;
        case TILE_GRASS_SIDE: {
            int drip = 13 - (int)(hash2(x, 7, 99) % 2u);  /* borda irregular */
            if (y >= drip) { r = 92 + n; g = 156 + n; b = 56 + n / 2; }
            else           { r = 134 + n; g = 96 + n; b = 67 + n; }
        } break;
        case TILE_LOG_SIDE: {
            int stripe = (x % 4 == 0) ? -22 : 0;
            r = 102 + stripe + n / 2; g = 81 + stripe + n / 2; b = 50 + stripe + n / 2;
        } break;
        case TILE_LOG_TOP: {
            float dx = x - 7.5f, dy = y - 7.5f;
            float d = (dx < 0 ? -dx : dx) > (dy < 0 ? -dy : dy)
                    ? (dx < 0 ? -dx : dx) : (dy < 0 ? -dy : dy);
            if (d > 6.5f)            { r = 102; g = 81;  b = 50; }   /* casca  */
            else if (((int)d) % 2)   { r = 176; g = 142; b = 88; }   /* aneis  */
            else                     { r = 150; g = 118; b = 70; }
            r += n / 3; g += n / 3; b += n / 3;
        } break;
        case TILE_LEAVES: {
            int dark = (hash2(x, y, 5) % 5u == 0) ? -28 : 0;
            r = 48 + dark + n / 2; g = 125 + dark + n; b = 38 + dark + n / 2;
        } break;
        case TILE_PLANKS: {
            int seam = (y % 4 == 3) || ((x + (y / 4) * 8) % 16 == 0);
            int k = seam ? -32 : 0;
            r = 172 + k + n / 3; g = 136 + k + n / 3; b = 82 + k + n / 3;
        } break;
        }
        put(img, tile, x, y, r, g, b);
    }

    GLuint tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, ATLAS_W, ATLAS_H, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, img);
    /* NEAREST = pixels "quadradoes" estilo Minecraft */
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    free(img);
    return tex;
}
