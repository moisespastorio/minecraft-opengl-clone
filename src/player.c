#include "player.h"
#include <math.h>

#define GRAVITY      28.0f
#define JUMP_SPEED    8.5f
#define MAX_FALL     50.0f
#define WALK_SPEED    4.5f
#define FLY_SPEED    12.0f

void player_init(Player *p, const World *w)
{
    int x = WORLD_SIZE_X / 2, z = WORLD_SIZE_Z / 2;
    glm_vec3_copy((vec3){x + 0.5f, world_surface_height(w, x, z) + 1.0f, z + 0.5f}, p->pos);
    glm_vec3_zero(p->vel);
    p->yaw = -90.0f;
    p->pitch = 0.0f;
    p->on_ground = 0;
    p->flying = 0;
}

void player_eye(const Player *p, vec3 out)
{
    glm_vec3_copy((vec3){p->pos[0], p->pos[1] + EYE_HEIGHT, p->pos[2]}, out);
}

void player_front(const Player *p, vec3 out)
{
    float yaw = glm_rad(p->yaw), pitch = glm_rad(p->pitch);
    out[0] = cosf(yaw) * cosf(pitch);
    out[1] = sinf(pitch);
    out[2] = sinf(yaw) * cosf(pitch);
}

/* a caixa [min,max) cobre as celulas floor(min) .. ceil(max)-1 */
static int box_collides(const World *w, const vec3 pos)
{
    float hw = PLAYER_WIDTH * 0.5f;
    int x0 = (int)floorf(pos[0] - hw), x1 = (int)ceilf(pos[0] + hw) - 1;
    int y0 = (int)floorf(pos[1]),      y1 = (int)ceilf(pos[1] + PLAYER_HEIGHT) - 1;
    int z0 = (int)floorf(pos[2] - hw), z1 = (int)ceilf(pos[2] + hw) - 1;

    for (int x = x0; x <= x1; x++)
    for (int y = y0; y <= y1; y++)
    for (int z = z0; z <= z1; z++)
        if (world_is_solid(w, x, y, z)) return 1;
    return 0;
}

/* Move um eixo por vez: se bater, desfaz o movimento naquele eixo.
   Isso faz o jogador "deslizar" em paredes em vez de grudar. */
static void move_axis(Player *p, const World *w, int axis, float delta)
{
    p->pos[axis] += delta;
    if (box_collides(w, p->pos)) {
        p->pos[axis] -= delta;
        if (axis == 1 && delta < 0.0f) p->on_ground = 1;
        p->vel[axis] = 0.0f;
    }
}

void player_update(Player *p, const World *w, const PlayerInput *in, float dt)
{
    if (dt > 0.05f) dt = 0.05f;   /* evita atravessar blocos em travadas */

    float yaw = glm_rad(p->yaw);
    vec3 fwd   = { cosf(yaw), 0.0f, sinf(yaw) };
    vec3 right = { -sinf(yaw), 0.0f, cosf(yaw) };

    vec3 wish = {0};
    glm_vec3_muladds(fwd,   in->move_fwd,   wish);
    glm_vec3_muladds(right, in->move_right, wish);
    if (glm_vec3_norm(wish) > 1.0f) glm_vec3_normalize(wish);

    float speed = p->flying ? FLY_SPEED : WALK_SPEED;
    p->vel[0] = wish[0] * speed;
    p->vel[2] = wish[2] * speed;

    if (p->flying) {
        p->vel[1] = ((in->up ? 1.0f : 0.0f) - (in->down ? 1.0f : 0.0f)) * speed;
    } else {
        p->vel[1] -= GRAVITY * dt;
        if (p->vel[1] < -MAX_FALL) p->vel[1] = -MAX_FALL;
        if (in->up && p->on_ground) p->vel[1] = JUMP_SPEED;
    }

    p->on_ground = 0;   /* sera' reativado se o eixo Y bater no chao */
    move_axis(p, w, 0, p->vel[0] * dt);
    move_axis(p, w, 2, p->vel[2] * dt);
    move_axis(p, w, 1, p->vel[1] * dt);
}

int player_overlaps_block(const Player *p, int x, int y, int z)
{
    float hw = PLAYER_WIDTH * 0.5f;
    return p->pos[0] + hw > x && p->pos[0] - hw < x + 1 &&
           p->pos[1] + PLAYER_HEIGHT > y && p->pos[1] < y + 1 &&
           p->pos[2] + hw > z && p->pos[2] - hw < z + 1;
}
