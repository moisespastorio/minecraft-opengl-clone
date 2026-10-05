#ifndef NOISE_H
#define NOISE_H
#include <stdint.h>

/* Hash inteiro 2D -> 32 bits (deterministico, sem estado global) */
uint32_t hash2(int x, int y, uint32_t seed);

/* Value noise 2D suave, retorna [0,1) */
float value_noise(float x, float y, uint32_t seed);

/* Soma de varias oitavas (fractal Brownian motion), retorna ~[0,1] */
float fbm(float x, float y, uint32_t seed, int octaves);

#endif
