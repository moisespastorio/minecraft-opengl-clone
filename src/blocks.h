#ifndef BLOCKS_H
#define BLOCKS_H

/* ------------------------------------------------------------------ */
/*  Tipos de bloco e mapeamento bloco -> tile do atlas de texturas     */
/* ------------------------------------------------------------------ */

typedef enum {
    BLOCK_AIR = 0,
    BLOCK_GRASS,
    BLOCK_DIRT,
    BLOCK_STONE,
    BLOCK_SAND,
    BLOCK_LOG,
    BLOCK_LEAVES,
    BLOCK_PLANKS,
    BLOCK_COUNT
} BlockType;

/* Cada tile e' um quadradinho 16x16 dentro do atlas (uma unica textura) */
typedef enum {
    TILE_GRASS_TOP = 0,
    TILE_GRASS_SIDE,
    TILE_DIRT,
    TILE_STONE,
    TILE_SAND,
    TILE_LOG_SIDE,
    TILE_LOG_TOP,
    TILE_LEAVES,
    TILE_PLANKS,
    TILE_COUNT
} Tile;

#define TILE_PX     16   /* tamanho de um tile em pixels            */
#define ATLAS_COLS  16   /* quantos tiles cabem na largura do atlas */

/* Indices de face usados no mesh e no raycast:
   0=+X  1=-X  2=+Y(topo)  3=-Y(baixo)  4=+Z  5=-Z */
static inline int block_tile(BlockType b, int face)
{
    switch (b) {
    case BLOCK_GRASS:  return face == 2 ? TILE_GRASS_TOP
                            : face == 3 ? TILE_DIRT : TILE_GRASS_SIDE;
    case BLOCK_DIRT:   return TILE_DIRT;
    case BLOCK_STONE:  return TILE_STONE;
    case BLOCK_SAND:   return TILE_SAND;
    case BLOCK_LOG:    return (face == 2 || face == 3) ? TILE_LOG_TOP : TILE_LOG_SIDE;
    case BLOCK_LEAVES: return TILE_LEAVES;
    case BLOCK_PLANKS: return TILE_PLANKS;
    default:           return TILE_STONE;
    }
}

#endif
