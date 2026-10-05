#include <glad/glad.h>      /* sempre ANTES do glfw3.h */
#include <GLFW/glfw3.h>
#include <cglm/cglm.h>
#include <stdio.h>
#include <stdlib.h>

#include "world.h"
#include "player.h"
#include "shader.h"
#include "texture.h"

#define START_W 1280
#define START_H 720
#define MOUSE_SENS 0.10f
#define REACH 6.0f

static const vec3 SKY = {0.53f, 0.74f, 0.92f};

static const BlockType HOTBAR[] = {
    BLOCK_GRASS, BLOCK_DIRT, BLOCK_STONE, BLOCK_SAND,
    BLOCK_LOG, BLOCK_LEAVES, BLOCK_PLANKS
};
static const char *HOTBAR_NAME[] = {
    "Grama", "Terra", "Pedra", "Areia", "Tronco", "Folhas", "Tabuas"
};
#define HOTBAR_N ((int)(sizeof HOTBAR / sizeof HOTBAR[0]))

/* Estado compartilhado com os callbacks (via glfwSetWindowUserPointer) */
typedef struct {
    Player player;
    int    selected;
    int    fb_w, fb_h;
    double last_x, last_y;
    int    first_mouse;
    int    click_break, click_place;   /* viram 1 no clique, consumidos no loop */
} App;

/* ------------------------------ callbacks ------------------------------ */
static void on_resize(GLFWwindow *win, int w, int h)
{
    App *app = glfwGetWindowUserPointer(win);
    app->fb_w = w; app->fb_h = h;
    glViewport(0, 0, w, h);
}

static void on_mouse_move(GLFWwindow *win, double x, double y)
{
    App *app = glfwGetWindowUserPointer(win);
    if (app->first_mouse) { app->last_x = x; app->last_y = y; app->first_mouse = 0; }
    float dx = (float)(x - app->last_x);
    float dy = (float)(app->last_y - y);        /* y da tela cresce para baixo */
    app->last_x = x; app->last_y = y;

    app->player.yaw   += dx * MOUSE_SENS;
    app->player.pitch += dy * MOUSE_SENS;
    if (app->player.pitch >  89.0f) app->player.pitch =  89.0f;
    if (app->player.pitch < -89.0f) app->player.pitch = -89.0f;
}

static void on_mouse_button(GLFWwindow *win, int button, int action, int mods)
{
    (void)mods;
    App *app = glfwGetWindowUserPointer(win);
    if (action != GLFW_PRESS) return;
    if (button == GLFW_MOUSE_BUTTON_LEFT)  app->click_break = 1;
    if (button == GLFW_MOUSE_BUTTON_RIGHT) app->click_place = 1;
}

static void on_scroll(GLFWwindow *win, double dx, double dy)
{
    (void)dx;
    App *app = glfwGetWindowUserPointer(win);
    int dir = dy > 0 ? 1 : -1;
    app->selected = ((app->selected - dir) % HOTBAR_N + HOTBAR_N) % HOTBAR_N;
}

static void on_key(GLFWwindow *win, int key, int sc, int action, int mods)
{
    (void)sc; (void)mods;
    App *app = glfwGetWindowUserPointer(win);
    if (action != GLFW_PRESS) return;

    if (key == GLFW_KEY_ESCAPE) glfwSetWindowShouldClose(win, GLFW_TRUE);
    if (key == GLFW_KEY_F)      app->player.flying = !app->player.flying;
    if (key >= GLFW_KEY_1 && key < GLFW_KEY_1 + HOTBAR_N)
        app->selected = key - GLFW_KEY_1;
}

/* --------------------------- geometria auxiliar ------------------------ */
static GLuint make_vao(const float *verts, int count)   /* count = n de floats */
{
    GLuint vao, vbo;
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, count * sizeof(float), verts, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void *)0);
    glEnableVertexAttribArray(0);
    return vao;
}

/* contorno de um cubo unitario (12 arestas = 24 vertices), levemente maior
   que o bloco para nao brigar com a profundidade (z-fighting) */
static GLuint make_outline_vao(void)
{
    static const float C[8][3] = {
        {0,0,0},{1,0,0},{1,0,1},{0,0,1},{0,1,0},{1,1,0},{1,1,1},{0,1,1}
    };
    static const int E[12][2] = {
        {0,1},{1,2},{2,3},{3,0},{4,5},{5,6},{6,7},{7,4},{0,4},{1,5},{2,6},{3,7}
    };
    float v[24 * 3];
    for (int e = 0; e < 12; e++)
    for (int k = 0; k < 2; k++)
    for (int a = 0; a < 3; a++)
        v[(e * 2 + k) * 3 + a] = C[E[e][k]][a] * 1.004f - 0.002f;
    return make_vao(v, 24 * 3);
}

static GLuint make_crosshair_vao(void)
{
    const float s = 0.03f;
    const float v[] = { -s,0,0,  s,0,0,   0,-s,0,  0,s,0 };
    return make_vao(v, 12);
}

/* ================================ main ================================== */
int main(void)
{
    if (!glfwInit()) { fprintf(stderr, "falha ao iniciar GLFW\n"); return 1; }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
#endif

    GLFWwindow *win = glfwCreateWindow(START_W, START_H, "MiniCraft", NULL, NULL);
    if (!win) { fprintf(stderr, "falha ao criar janela\n"); glfwTerminate(); return 1; }
    glfwMakeContextCurrent(win);
    glfwSwapInterval(1);                                  /* vsync */

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        fprintf(stderr, "falha ao carregar GLAD\n"); return 1;
    }

    static App app;                                        /* zerado por ser static */
    app.first_mouse = 1;
    glfwGetFramebufferSize(win, &app.fb_w, &app.fb_h);     /* difere de START_* em telas HiDPI */
    glfwSetWindowUserPointer(win, &app);
    glfwSetFramebufferSizeCallback(win, on_resize);
    glfwSetCursorPosCallback(win, on_mouse_move);
    glfwSetMouseButtonCallback(win, on_mouse_button);
    glfwSetScrollCallback(win, on_scroll);
    glfwSetKeyCallback(win, on_key);
    glfwSetInputMode(win, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    glViewport(0, 0, app.fb_w, app.fb_h);

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);       /* descarta faces de tras (winding CCW = frente) */
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);

    GLuint block_prog = shader_create(BLOCK_VS, BLOCK_FS);
    GLuint line_prog  = shader_create(LINE_VS, LINE_FS);
    if (!block_prog || !line_prog) return 1;

    GLuint atlas = atlas_create();
    GLuint outline_vao   = make_outline_vao();
    GLuint crosshair_vao = make_crosshair_vao();

    World *world = world_create(1337);
    world_update_meshes(world, WORLD_CX * WORLD_CZ);       /* monta tudo de uma vez */
    player_init(&app.player, world);

    /* uniforms que nao mudam: pegamos a location uma vez so */
    GLint u_vp   = glGetUniformLocation(block_prog, "uVP");
    GLint u_atl  = glGetUniformLocation(block_prog, "uAtlas");
    GLint u_cam  = glGetUniformLocation(block_prog, "uCamPos");
    GLint u_fogc = glGetUniformLocation(block_prog, "uFogColor");
    GLint u_fogr = glGetUniformLocation(block_prog, "uFogRange");
    GLint u_mvp  = glGetUniformLocation(line_prog,  "uMVP");
    GLint u_col  = glGetUniformLocation(line_prog,  "uColor");

    double last_time = glfwGetTime(), fps_timer = 0.0;
    int frames = 0;

    while (!glfwWindowShouldClose(win)) {
        double now = glfwGetTime();
        float dt = (float)(now - last_time);
        last_time = now;

        glfwPollEvents();

        /* ---------- entrada continua (teclas seguradas) ---------- */
        PlayerInput in = {0};
        if (glfwGetKey(win, GLFW_KEY_W) == GLFW_PRESS) in.move_fwd   += 1.0f;
        if (glfwGetKey(win, GLFW_KEY_S) == GLFW_PRESS) in.move_fwd   -= 1.0f;
        if (glfwGetKey(win, GLFW_KEY_D) == GLFW_PRESS) in.move_right += 1.0f;
        if (glfwGetKey(win, GLFW_KEY_A) == GLFW_PRESS) in.move_right -= 1.0f;
        in.up   = glfwGetKey(win, GLFW_KEY_SPACE)      == GLFW_PRESS;
        in.down = glfwGetKey(win, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS;

        player_update(&app.player, world, &in, dt);

        /* ---------- mira: raycast do olho na direcao do olhar ---------- */
        vec3 eye, front;
        player_eye(&app.player, eye);
        player_front(&app.player, front);
        RayHit hit = world_raycast(world, eye, front, REACH);

        if (app.click_break && hit.hit)
            world_set_block(world, hit.block[0], hit.block[1], hit.block[2], BLOCK_AIR);
        if (app.click_place && hit.hit &&
            !player_overlaps_block(&app.player, hit.prev[0], hit.prev[1], hit.prev[2]))
            world_set_block(world, hit.prev[0], hit.prev[1], hit.prev[2], HOTBAR[app.selected]);
        app.click_break = app.click_place = 0;

        world_update_meshes(world, 4);     /* limite por frame evita travadas */

        /* ---------- matrizes ---------- */
        float aspect = app.fb_h > 0 ? (float)app.fb_w / (float)app.fb_h : 1.0f;
        mat4 proj, view, vp;
        vec3 center, up = {0, 1, 0};
        glm_perspective(glm_rad(70.0f), aspect, 0.1f, 300.0f, proj);
        glm_vec3_add(eye, front, center);
        glm_lookat(eye, center, up, view);
        glm_mat4_mul(proj, view, vp);

        /* ---------- desenho ---------- */
        glClearColor(SKY[0], SKY[1], SKY[2], 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glUseProgram(block_prog);
        glUniformMatrix4fv(u_vp, 1, GL_FALSE, (float *)vp);
        glUniform3fv(u_cam, 1, eye);
        glUniform3fv(u_fogc, 1, SKY);
        glUniform2f(u_fogr, 50.0f, 110.0f);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, atlas);
        glUniform1i(u_atl, 0);
        world_draw(world);

        glUseProgram(line_prog);
        if (hit.hit) {                                      /* contorno do bloco mirado */
            mat4 model, mvp;
            glm_translate_make(model, (vec3){(float)hit.block[0], (float)hit.block[1], (float)hit.block[2]});
            glm_mat4_mul(vp, model, mvp);
            glUniformMatrix4fv(u_mvp, 1, GL_FALSE, (float *)mvp);
            glUniform3f(u_col, 0.0f, 0.0f, 0.0f);
            glBindVertexArray(outline_vao);
            glDrawArrays(GL_LINES, 0, 24);
        }

        mat4 hud;                                           /* mira em coordenadas de tela (NDC) */
        glm_mat4_identity(hud);
        hud[0][0] = 1.0f / aspect;                          /* corrige para a mira ficar quadrada */
        glDisable(GL_DEPTH_TEST);
        glUniformMatrix4fv(u_mvp, 1, GL_FALSE, (float *)hud);
        glUniform3f(u_col, 1.0f, 1.0f, 1.0f);
        glBindVertexArray(crosshair_vao);
        glDrawArrays(GL_LINES, 0, 4);
        glEnable(GL_DEPTH_TEST);

        glfwSwapBuffers(win);

        /* ---------- FPS + bloco selecionado no titulo ---------- */
        frames++;
        fps_timer += dt;
        if (fps_timer >= 0.5) {
            char title[128];
            snprintf(title, sizeof title, "MiniCraft | %.0f FPS | bloco: %s | %s",
                     frames / fps_timer, HOTBAR_NAME[app.selected],
                     app.player.flying ? "voando" : "andando");
            glfwSetWindowTitle(win, title);
            frames = 0; fps_timer = 0.0;
        }
    }

    world_destroy(world);
    glDeleteTextures(1, &atlas);
    glDeleteProgram(block_prog);
    glDeleteProgram(line_prog);
    glfwDestroyWindow(win);
    glfwTerminate();
    return 0;
}
