#include "noise.h"
#include <math.h>

uint32_t hash2(int x, int y, uint32_t seed)
{
    uint32_t h = (uint32_t)x * 374761393u
               + (uint32_t)y * 668265263u
               + seed * 2246822519u;
    h = (h ^ (h >> 13)) * 1274126177u;
    return h ^ (h >> 16);
}

/* valor pseudo-aleatorio fixo em cada ponto inteiro da grade */
static float lattice(int x, int y, uint32_t seed)
{
    return (hash2(x, y, seed) & 0xFFFFFF) / (float)0x1000000;
}

/* curva "smoothstep": suaviza a interpolacao (derivada zero nas pontas) */
static float smooth(float t) { return t * t * (3.0f - 2.0f * t); }

float value_noise(float x, float y, uint32_t seed)
{
    int   xi = (int)floorf(x), yi = (int)floorf(y);
    float tx = smooth(x - xi),  ty = smooth(y - yi);

    float a = lattice(xi,     yi,     seed);
    float b = lattice(xi + 1, yi,     seed);
    float c = lattice(xi,     yi + 1, seed);
    float d = lattice(xi + 1, yi + 1, seed);

    float top = a + (b - a) * tx;
    float bot = c + (d - c) * tx;
    return top + (bot - top) * ty;
}

float fbm(float x, float y, uint32_t seed, int octaves)
{
    float sum = 0.0f, amp = 1.0f, norm = 0.0f, freq = 1.0f;
    for (int i = 0; i < octaves; i++) {
        sum  += value_noise(x * freq, y * freq, seed + (uint32_t)i) * amp;
        norm += amp;
        amp  *= 0.5f;   /* cada oitava contribui metade da anterior */
        freq *= 2.0f;   /* ...com o dobro de detalhe                */
    }
    return sum / norm;
}
