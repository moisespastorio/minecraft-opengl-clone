#ifndef PLAYER_HEADER_H
#define PLAYER_HEADER_H
#include <cglm/cglm.h>
#include "world.h"

#define PLAYER_WIDTH   0.6f    /* largura da caixa de colisao (AABB) */
#define PLAYER_HEIGHT 1.8f
#define EYE_HEIGHT 1.62f

typedef struct {
    vec3  pos;             /* posicao dos PES (centro da base da caixa) */
    vec3  vel;
    float yaw, pitch;      /* graus; yaw = -90 olha para -Z */
    int   on_ground;
    int   flying;
} Player;

typedef struct {
    float move_fwd, move_right;   /* -1..1 */
    int   up, down;               /* pular/subir, descer (voo) */
} PlayerInput;

void player_init(Player *p, const World *w);
void player_update(Player *p, const World *w, const PlayerInput *in, float dt);
void player_eye(const Player *p, vec3 out);
void player_front(const Player *p, vec3 out);          /* direcao do olhar */
int  player_overlaps_block(const Player *p, int x, int y, int z);

#endif
