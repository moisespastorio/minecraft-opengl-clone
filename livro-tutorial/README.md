---
title: "Construindo um Mini Minecraft do Zero"
subtitle: "Um livro-tutorial de OpenGL 3.3 em C com GLFW, GLAD e cglm"
lang: pt-BR
toc: true
toc-depth: 2
---

\newpage

# Prefácio: como usar este livro

Este livro ensina a construir, passo a passo, um **mini Minecraft** em C: terreno gerado proceduralmente, texturas, câmera em primeira pessoa, física com colisão, e a mecânica clássica de **quebrar e colocar blocos**. O foco não é só *o que* digitar, mas **por que cada decisão foi tomada**, incluindo as alternativas que descartamos.

## Para quem é

Você já sabe o básico de OpenGL: VAOs, VBOs, EBOs e a ideia de *model / view / projection*. Por isso **não vamos ensinar o que é um VBO**. Em compensação, vamos dedicar um capítulo inteiro à **câmera**, que costuma ser o ponto onde "eu só brinco com a matriz de view" vira "agora eu entendo o que a matriz de view é".

Também assumimos C razoável: ponteiros, `struct`, `malloc`/`calloc`, `static`, header guards.

## Convenções do livro

Ao longo dos capítulos você vai encontrar quatro tipos de caixa:

> 💡 **Curiosidade.** Um desvio lateral, quase sempre útil mais tarde.

> 🧭 **Decisão de projeto.** Uma escolha arquitetural, com as alternativas e o preço de cada uma.

> ⚠️ **Cuidado.** Uma armadilha em que (quase) todo mundo cai.

> 🤔 **Para pensar.** Perguntas numeradas (P4.1, P7.2...). As respostas comentadas estão no Apêndice C. Tente responder *antes* de olhar: é aí que o aprendizado gruda.

Todo capítulo termina com um **checkpoint**: algo que você consegue compilar e ver na tela. Se o checkpoint não funcionar, não avance: o capítulo seguinte assume que o anterior está de pé.

## O que vamos construir

| Cap. | Tema | Checkpoint |
|---|---|---|
| 1 | Ambiente: CMake, GLFW, GLAD, cglm | projeto compila |
| 2 | Janela, contexto, game loop | céu azul |
| 3 | Shaders e o primeiro cubo | cubo em arame |
| 4 | Câmera em primeira pessoa (a fundo) | voar em volta do cubo |
| 5 | Modelo de dados: blocos e chunks | acessores testados |
| 6 | Terreno procedural | mapa de alturas em ASCII |
| 7 | Do voxel ao triângulo: mesh | terreno em escala de cinza |
| 8 | Texturas, atlas, iluminação falsa e neblina | mundo texturizado |
| 9 | Física e colisão | andar, pular, cair |
| 10 | Raycast, quebrar/colocar blocos, HUD | jogo jogável |
| 11 | Costurando tudo: a `main` | versão final |
| 12 | Depuração, desempenho e próximos passos | você, sozinho |

## Arquitetura em uma imagem

```
main.c  ---->  player.c  ---->  world.c  ---->  noise.c
  |                                |
  |                                +---------->  blocks.h
  +------>  shader.c
  |
  +------>  texture.c  ---->  blocks.h, noise.c
```

Leia as setas como "depende de". Duas propriedades importam:

1. **`world.c` não sabe nada sobre janela, teclado ou câmera.** Ele só conhece blocos e malhas.
2. **`player.c` não sabe nada sobre OpenGL.** Ele só pergunta ao mundo "essa célula é sólida?".

Essas separações parecem burocracia agora e viram salvação quando você quiser testar a física sem abrir janela (e nós vamos fazer isso).

## O código completo

Os trechos do livro são extraídos **diretamente** dos arquivos do projeto final (entregue junto), então o que você lê é o que compilou. Nos capítulos iniciais mostramos versões simplificadas, só quando o passo intermediário é didático; sempre avisamos.

\newpage

# Capítulo 1: Preparando o terreno

> **Objetivo:** ter um projeto que compila, com GLFW, GLAD e cglm enxergados pelo CMake.

## 1.1 Quem faz o quê

Antes de instalar qualquer coisa, vale entender o *papel* de cada biblioteca. Pense numa obra:

- **GLFW** é o **mestre de obras que consegue o alvará e abre o terreno**: cria a janela, pede ao sistema operacional um *contexto* OpenGL, e entrega teclado e mouse.
- **GLAD** é a **agenda de telefones**: OpenGL não é uma biblioteca que você linka com todas as funções; é uma *especificação* cujas funções vivem no driver da placa de vídeo. Alguém precisa descobrir, em tempo de execução, o endereço de cada função (`glDrawArrays`, `glBufferData`...). Esse alguém é o GLAD.
- **cglm** é a **calculadora científica**: vetores, matrizes, `lookAt`, `perspective`.

> 💡 **Curiosidade.** No Windows, a `opengl32.dll` só exporta as funções do OpenGL **1.1**. Tudo acima disso (VAOs, shaders, tudo que usamos) precisa ser buscado em tempo de execução. É exatamente por isso que loaders como o GLAD existem.

## 1.2 Estrutura de pastas

Esta é a estrutura final. Você vai criá-la aos poucos:

```
minicraft/
  CMakeLists.txt
  third_party/
    glad/
      include/glad/glad.h
      include/KHR/khrplatform.h
      src/glad.c
  src/
    main.c  world.c/.h  player.c/.h  noise.c/.h
    shader.c/.h  texture.c/.h  blocks.h
```

## 1.3 Gerando o GLAD

Usamos o **GLAD 1.x** (o que expõe `<glad/glad.h>` e `gladLoadGLLoader`), configurado para **OpenGL 3.3 Core**. Uma forma de gerar é via Python:

```
pip install glad==0.1.36
python -m glad --generator c --api gl=3.3 --profile core --out-path third_party/glad
```

(Também serve o gerador web do GLAD 1.x: escolha *gl 3.3*, perfil *Core*, linguagem *C*.)

> 🧭 **Decisão de projeto: por que OpenGL 3.3 Core?**
> É o menor denominador comum moderno: roda em Windows, Linux e macOS (que parou em 4.1 e exige *core profile*). O perfil *core* **remove o pipeline fixo** (`glBegin`/`glEnd`, matrizes embutidas). Isso nos obriga a escrever shaders, que é o que você quer aprender de qualquer jeito. O preço: nada de recursos de 4.5+ (como `glBindTextureUnit`), mas não precisamos de nenhum.

## 1.4 O `CMakeLists.txt`

```cmake
cmake_minimum_required(VERSION 3.16)
project(minicraft C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

include(FetchContent)

# --- GLFW: usa a do sistema se existir, senao baixa e compila ---------
find_package(glfw3 3.3 QUIET)
if(NOT glfw3_FOUND)
  message(STATUS "GLFW nao encontrada no sistema: baixando via FetchContent")
  set(GLFW_BUILD_DOCS     OFF CACHE BOOL "" FORCE)
  set(GLFW_BUILD_TESTS    OFF CACHE BOOL "" FORCE)
  set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
  FetchContent_Declare(glfw GIT_REPOSITORY https://github.com/glfw/glfw.git GIT_TAG 3.4)
  FetchContent_MakeAvailable(glfw)
endif()

# --- cglm: idem ---------------------------------------------------------
find_package(cglm QUIET)
if(NOT cglm_FOUND)
  message(STATUS "cglm nao encontrada no sistema: baixando via FetchContent")
  set(CGLM_SHARED OFF CACHE BOOL "" FORCE)
  set(CGLM_STATIC ON  CACHE BOOL "" FORCE)
  FetchContent_Declare(cglm GIT_REPOSITORY https://github.com/recp/cglm.git GIT_TAG v0.9.4)
  FetchContent_MakeAvailable(cglm)
endif()

add_executable(minicraft
  src/main.c
  src/world.c
  src/player.c
  src/noise.c
  src/shader.c
  src/texture.c
  third_party/glad/src/glad.c       # GLAD 1.x gerado para GL 3.3 core
)

target_include_directories(minicraft PRIVATE src third_party/glad/include)
target_link_libraries(minicraft PRIVATE glfw)

if(TARGET cglm::cglm)
  target_link_libraries(minicraft PRIVATE cglm::cglm)
else()
  target_link_libraries(minicraft PRIVATE cglm)
endif()

if(UNIX)
  target_link_libraries(minicraft PRIVATE m ${CMAKE_DL_LIBS})
endif()

if(MSVC)
  target_compile_options(minicraft PRIVATE /W3)
else()
  target_compile_options(minicraft PRIVATE -Wall -Wextra)
endif()
```

Pontos que merecem atenção:

- **`find_package` com *fallback* para `FetchContent`.** Se a biblioteca está instalada no sistema, usamos ela; senão, o CMake baixa e compila. Quem clona o projeto não precisa instalar nada além de compilador, CMake e git.
- **`PRIVATE`.** Nosso executável não é uma biblioteca, então nada precisa "vazar" para quem nos usa. É o hábito certo desde cedo.
- **`${CMAKE_DL_LIBS}` e `m`.** O arquivo `glad.c` usa `dlopen`/`dlsym` no Linux; `m` é a libm (`floorf`, `sinf`...).
- **`-Wall -Wextra`.** Warnings ligados desde o dia 1. Durante a escrita deste livro eles pegaram um bug real (veja o Capítulo 9).
- **cglm e o link.** As funções `glm_*` são `inline` em headers; o link com a biblioteca só é estritamente necessário para as variantes `glmc_*`. Mesmo assim ligamos o alvo, porque é ele que nos entrega o *include path* corretamente.

> ⚠️ **Cuidado.** Enquanto você está no Capítulo 1, os arquivos `src/*.c` ainda não existem. Vá adicionando cada um ao `add_executable` à medida que aparece, ou crie arquivos vazios agora.

## 1.5 Dependências e build

```
# Linux (Debian/Ubuntu)
sudo apt install build-essential cmake libglfw3-dev libcglm-dev
# macOS
brew install cmake glfw cglm

cmake -S . -B build
cmake --build build
```

Se não instalar nada, o `FetchContent` baixa GLFW e cglm. No Linux a GLFW precisa de headers de X11/Wayland para compilar (`libx11-dev`, `libxrandr-dev`, `libxinerama-dev`, `libxcursor-dev`, `libxi-dev`).

> 🤔 **Para pensar.**
> **P1.1.** Por que `gladLoadGLLoader` só funciona *depois* de `glfwMakeContextCurrent`?
> **P1.2.** Por que precisamos linkar `-ldl` no Linux se estamos usando `glfwGetProcAddress` para carregar as funções?

\newpage

# Capítulo 2: Janela, contexto e game loop

> **Checkpoint:** uma janela 1280x720 azul-céu que fecha com `Esc`.

## 2.1 Criando o contexto

Um **contexto OpenGL** é a "mesa de trabalho" onde ficam todos os seus objetos (texturas, buffers, programas) e o estado atual (qual VAO está ligado, qual shader...). Cada thread só enxerga o contexto que está *corrente* nela. Por isso a ordem é:

1. `glfwInit()`
2. *hints* (versão, perfil) **antes** de criar a janela
3. `glfwCreateWindow()`
4. `glfwMakeContextCurrent()`
5. `gladLoadGLLoader(glfwGetProcAddress)`

Se você inverter 4 e 5, o `glfwGetProcAddress` não tem de onde tirar endereços e o GLAD falha (ou, pior, devolve ponteiros nulos que só explodem na primeira chamada GL).

No macOS, o contexto 3.3 Core exige o hint `GLFW_OPENGL_FORWARD_COMPAT`. Em outros sistemas ele é inofensivo, mas protegemos com `#ifdef __APPLE__`.

## 2.2 O game loop

Todo jogo é um laço que repete quatro verbos:

```
while (jogo rodando):
    1. medir o tempo        (dt)
    2. ler entrada          (eventos + teclas seguradas)
    3. atualizar o mundo    (física, ações)
    4. desenhar e trocar buffers
```

### Por que medir `dt`?

Imagine mover o jogador com `pos += 0.1f` por frame. Num PC a 60 FPS ele anda 6 unidades/s; na máquina do seu amigo a 144 FPS, 14,4 unidades/s. A solução é trabalhar com **velocidade × tempo**: `pos += velocidade * dt`, com `dt` em segundos desde o frame anterior.

Nosso `dt` é **variável** (o tempo real do último frame). Alternativas existem (passo fixo com acumulador, a técnica do artigo "Fix Your Timestep", de Glenn Fiedler), e são melhores para física determinística. Para um jogo single-player simples, `dt` variável com *clamp* (veremos no Capítulo 9) é suficiente e muito mais curto de escrever.

### Tamanho da janela ≠ tamanho do framebuffer

Em telas de alta densidade (Retina, escalonamento 150% no Windows), a janela pode medir 1280x720 "pontos" mas ter 2560x1440 **pixels reais**. `glViewport` trabalha em pixels reais, então usamos `glfwGetFramebufferSize` e o callback `glfwSetFramebufferSizeCallback`, nunca o tamanho que pedimos ao criar a janela.

> 💡 **Curiosidade.** `glfwSwapInterval(1)` liga o *vsync*: o `SwapBuffers` espera o "intervalo de apagamento vertical" do monitor. O nome vem dos monitores CRT, em que o feixe de elétrons precisava voltar ao topo da tela entre um quadro e outro.

## 2.3 O primeiro programa

Esta é a versão **simplificada** de `main.c` deste capítulo (a final está no Capítulo 11):

```c
#include <glad/glad.h>      /* sempre ANTES do glfw3.h */
#include <GLFW/glfw3.h>
#include <stdio.h>

static void on_resize(GLFWwindow *win, int w, int h)
{
    (void)win;
    glViewport(0, 0, w, h);
}

int main(void)
{
    if (!glfwInit()) { fprintf(stderr, "falha ao iniciar GLFW\n"); return 1; }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
#endif

    GLFWwindow *win = glfwCreateWindow(1280, 720, "MiniCraft", NULL, NULL);
    if (!win) { fprintf(stderr, "falha ao criar janela\n"); glfwTerminate(); return 1; }
    glfwMakeContextCurrent(win);
    glfwSwapInterval(1);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        fprintf(stderr, "falha ao carregar GLAD\n");
        return 1;
    }

    glfwSetFramebufferSizeCallback(win, on_resize);
    int fbw, fbh;
    glfwGetFramebufferSize(win, &fbw, &fbh);
    glViewport(0, 0, fbw, fbh);
    glEnable(GL_DEPTH_TEST);

    double last = glfwGetTime();
    while (!glfwWindowShouldClose(win)) {
        double now = glfwGetTime();
        float dt = (float)(now - last);
        last = now;
        (void)dt;                          /* vamos usar a partir do Cap. 4 */

        glfwPollEvents();
        if (glfwGetKey(win, GLFW_KEY_ESCAPE) == GLFW_PRESS)
            glfwSetWindowShouldClose(win, GLFW_TRUE);

        glClearColor(0.53f, 0.74f, 0.92f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glfwSwapBuffers(win);
    }

    glfwDestroyWindow(win);
    glfwTerminate();
    return 0;
}
```

Detalhes que valem uma pausa:

- **`#include <glad/glad.h>` antes de `<GLFW/glfw3.h>`.** O `glfw3.h` inclui o header GL do sistema se ninguém o fez antes; o GLAD precisa chegar primeiro e declarar tudo por conta própria. Se inverter, você recebe o erro clássico "gl.h included before glad.h".
- **`glClear` com `GL_DEPTH_BUFFER_BIT`.** Mesmo ainda sem desenhar nada 3D, já criamos o hábito. Esquecer de limpar o depth buffer produz o bug "só vejo o primeiro frame".
- **A cor do céu `(0.53, 0.74, 0.92)`** vai voltar no Capítulo 8, como cor da neblina.

> 🤔 **Para pensar.**
> **P2.1.** Se você desligar o vsync e mover a câmera com `pos += 0.1f` por frame, o que muda de uma máquina para outra? E com `pos += 5.0f * dt`?
> **P2.2.** Qual a diferença entre *tamanho da janela* e *tamanho do framebuffer*? Qual deles o `glViewport` quer?

\newpage

# Capítulo 3: Shaders e o primeiro cubo

> **Checkpoint:** um cubo unitário em arame, preto, visto de longe.

Você já sabe criar VAOs e VBOs; aqui o objetivo é montar a **infraestrutura de shaders** que usaremos o livro inteiro e verificar a cadeia *model → view → projection* com o objeto mais simples possível.

## 3.1 Compilando e linkando

Escrevemos um único utilitário, `shader_create(vs, fs)`, que compila os dois estágios, linka e **imprime o log de erro do driver** se algo falhar.

```c
static GLuint compile(GLenum type, const char *src)
{
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, NULL);
    glCompileShader(s);

    GLint ok;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[1024];
        glGetShaderInfoLog(s, sizeof log, NULL, log);
        fprintf(stderr, "[shader] erro de compilacao (%s):\n%s\n",
                type == GL_VERTEX_SHADER ? "vertex" : "fragment", log);
        glDeleteShader(s);
        return 0;
    }
    return s;
}
```

```c
GLuint shader_create(const char *vs_src, const char *fs_src)
{
    GLuint vs = compile(GL_VERTEX_SHADER, vs_src);
    GLuint fs = compile(GL_FRAGMENT_SHADER, fs_src);
    if (!vs || !fs) return 0;

    GLuint prog = glCreateProgram();
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);
    glLinkProgram(prog);
    glDeleteShader(vs);   /* ja foram copiados para o programa */
    glDeleteShader(fs);

    GLint ok;
    glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[1024];
        glGetProgramInfoLog(prog, sizeof log, NULL, log);
        fprintf(stderr, "[shader] erro de link:\n%s\n", log);
        glDeleteProgram(prog);
        return 0;
    }
    return prog;
}
```

Duas decisões pequenas, mas que economizam horas:

1. **Sempre imprimir o log.** Um shader que falha em silêncio produz uma tela preta e nenhuma pista. O log do driver diz a linha exata do erro GLSL.
2. **`glDeleteShader` logo após o link.** Os *shader objects* só servem de matéria-prima; depois do link o *program* guarda seu próprio código. Mantê-los vazaria memória de GPU.

> 🧭 **Decisão de projeto: GLSL embutido em strings C.**
> Os shaders do projeto vivem em `shader.c`, como strings literais, e não em arquivos `.vert`/`.frag`. **Vantagem:** o executável funciona de qualquer diretório (nada de "arquivo não encontrado porque você rodou de outra pasta"). **Desvantagem:** sem *hot reload*, e escrever GLSL entre aspas é feio. Se você preferir arquivos, troque por uma função que leia o arquivo para um buffer; o resto do código não muda.

## 3.2 O shader de linhas

Este é o shader mais simples possível: posição transformada por uma MVP e cor sólida. (Aqui está em GLSL puro; no projeto, cada linha é uma string C.)

```glsl
// vertex
#version 330 core
layout(location = 0) in vec3 aPos;
uniform mat4 uMVP;
void main() { gl_Position = uMVP * vec4(aPos, 1.0); }
```

```glsl
// fragment
#version 330 core
uniform vec3 uColor;
out vec4 FragColor;
void main() { FragColor = vec4(uColor, 1.0); }
```

Ele será reaproveitado no Capítulo 10 para o **contorno do bloco mirado** e a **mira**.

## 3.3 Geometria: o cubo em arame

Primeiro, uma função utilitária que cria um VAO com um VBO de posições (3 floats por vértice). Você conhece o ritual, então só reparamos que ele aceita qualquer lista de vértices:

```c
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
```

Agora o cubo. Em vez de escrever 24 vértices à mão, geramos a partir de **8 cantos** e **12 arestas** (pares de cantos):

```c
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
```

Cada aresta contribui com 2 vértices, 12 × 2 = 24, e desenhamos com `GL_LINES`. Note o `* 1.004f - 0.002f`: o cubo fica **0,4% maior** que o bloco, para as linhas não "brigarem" com as faces do próprio bloco no depth buffer (explicamos no Capítulo 10).

## 3.4 A cadeia MVP

Para ver o cubo precisamos de três matrizes. Com a cglm:

```c
mat4 proj, view, vp, model, mvp;
float aspect = (float)fbw / (float)fbh;

glm_perspective(glm_rad(70.0f), aspect, 0.1f, 300.0f, proj);
glm_lookat((vec3){2.5f, 2.0f, 4.0f},     /* olho    */
           (vec3){0.5f, 0.5f, 0.5f},     /* alvo    */
           (vec3){0.0f, 1.0f, 0.0f},     /* "cima"  */
           view);
glm_mat4_mul(proj, view, vp);            /* vp  = P * V */
glm_mat4_identity(model);
glm_mat4_mul(vp, model, mvp);            /* mvp = P * V * M */

glUseProgram(line_prog);
glUniformMatrix4fv(u_mvp, 1, GL_FALSE, (float *)mvp);
glUniform3f(u_col, 0.0f, 0.0f, 0.0f);
glBindVertexArray(outline_vao);
glDrawArrays(GL_LINES, 0, 24);
```

Três pontos importantes sobre matrizes na cglm:

- **`mat4` é `float[4][4]` em ordem de colunas** (*column-major*): `m[coluna][linha]`. É a mesma convenção do OpenGL, então passamos `GL_FALSE` no parâmetro `transpose`. Se você vir o cubo todo deformado, desconfie de um `GL_TRUE` fora de lugar.
- **`glm_mat4_mul(a, b, dest)` calcula `a * b`.** Em `P * V * M`, o vértice é multiplicado à direita: **primeiro `M`, depois `V`, depois `P`**. Leia as matrizes *da direita para a esquerda*.
- **`vp` calculado uma vez por frame.** Como `P * V` é igual para todos os objetos, multiplicamos uma vez e reaproveitamos. No jogo final, o `vp` serve para todos os chunks.

> 💡 **Curiosidade.** Guardar matrizes em ordem de colunas não é capricho: facilita a vida da GPU, porque os eixos transformados (as "colunas") ficam contíguos na memória. Por isso, na cglm, `hud[0][0]` é a escala em X, `m[3][0..2]` é a translação, e `m[0][0..2]` é o primeiro eixo da base.

> 🤔 **Para pensar.**
> **P3.1.** Se você trocar para `glm_mat4_mul(view, proj, vp)`, o que acontece com a cena?
> **P3.2.** Por que não precisamos manter os *shader objects* vivos depois do link?

\newpage

# Capítulo 4: A câmera em primeira pessoa (a fundo)

> **Checkpoint:** voar livremente ao redor do cubo com mouse + WASD + Espaço/Shift.

Este é o capítulo para quem "só brinca com a matriz de view". Vamos construir a intuição inteira, da matemática ao código.

## 4.1 Spoiler: a câmera não existe

OpenGL não tem câmera. Não existe objeto "câmera" em nenhuma função. O que existe é uma sequência de transformações que leva um vértice do mundo até a tela:

```
espaço do MODELO  --(Model)-->  espaço do MUNDO
espaço do MUNDO   --(View)--->  espaço da CÂMERA (olho)
espaço da CÂMERA  --(Proj)--->  espaço de RECORTE (clip)
clip --(divisão por w)--> NDC --(viewport)--> pixels
```

A **matriz de view** faz a mágica: em vez de mover a câmera pelo mundo, ela **move o mundo inteiro em volta de uma câmera parada na origem, olhando para -Z**. Pense num carrinho de filmagem: você pode empurrar o carrinho para a frente, ou pode puxar o cenário para trás em direção ao carrinho. Para quem está olhando pela lente, é *idêntico*.

Formalmente: **a view é a inversa da transformação da câmera**. Se a câmera estivesse posicionada e rotacionada por uma matriz `C` no mundo, a view seria `V = C⁻¹`.

## 4.2 Dentro do `lookAt`

`glm_lookat(olho, alvo, cima, V)` constrói essa inversa a partir de três coisas que *você* entende intuitivamente: onde estou, para onde olho, e o que é "para cima". Passo a passo:

```
f = normaliza(alvo - olho)      // frente: para onde a câmera olha
s = normaliza(f x cima)         // lado direito da câmera ("side")
u = s x f                       // cima REAL da câmera (já ortogonal)

     |  s.x   s.y   s.z   -dot(s, olho) |
V =  |  u.x   u.y   u.z   -dot(u, olho) |
     | -f.x  -f.y  -f.z    dot(f, olho) |
     |   0     0     0          1       |
```

Como ler isso:

- `s`, `u` e `-f` são os três eixos da câmera (direita, cima, e "para trás", já que a câmera olha para -Z). Eles formam uma **base ortonormal**.
- Colocar esses vetores nas **linhas** da matriz equivale a **transpor** a rotação da câmera, e para rotações a transposta é a inversa. É assim que "desfazemos" a rotação.
- A última coluna aplica a translação *já rotacionada*: `-dot(eixo, olho)` desloca o mundo para que o olho caia na origem.

Dois produtos vetoriais (`x`) merecem comentário: o primeiro (`f x cima`) só dá um vetor útil se `f` **não for paralelo a `cima`**. Guarde essa frase; ela explica uma restrição que vem a seguir.

## 4.3 De onde vem a direção do olhar?

O `lookAt` pede um alvo, e nós temos um jogador que gira o mouse. Duas opções:

1. **Guardar uma matriz de rotação e multiplicar a cada movimento do mouse.** Erros numéricos se acumulam (a matriz "desorto­gonaliza") e fica difícil impor limites.
2. **Guardar dois ângulos, `yaw` e `pitch`, e recalcular a direção a cada frame.** Sem acúmulo de erro, sem drift, limites triviais.

Em um jogo em primeira pessoa **não existe rolagem (*roll*)**, então dois ângulos bastam. É a nossa escolha.

> 🧭 **Decisão de projeto: yaw/pitch (e não quaternions).**
> Quaternions são o padrão para câmeras de 6 graus de liberdade (naves espaciais, onde o roll existe). Para FPS, são complexidade desnecessária. Se um dia você fizer um simulador de voo, aí sim.

### A fórmula da direção

`yaw` gira em torno do eixo Y (olhar para os lados); `pitch` inclina para cima/baixo. A direção da frente vem de coordenadas esféricas:

```
frente.x = cos(yaw) * cos(pitch)
frente.y =            sin(pitch)
frente.z = sin(yaw) * cos(pitch)
```

Para entender: `sin(pitch)` é a *altura* do vetor. Sobra um "comprimento horizontal" de `cos(pitch)`, que distribuímos no plano XZ na direção `(cos yaw, sin yaw)`. E o resultado já é unitário, pois:

```
cos²(p)·(cos²(y) + sin²(y)) + sin²(p) = cos²(p) + sin²(p) = 1
```

Convenção usada: `yaw = -90°` aponta para `(0, 0, -1)`, a direção natural "para frente" do OpenGL. Por isso o jogador começa com `yaw = -90`.

Já temos o código dessa fórmula pronto no projeto final:

```c
void player_front(const Player *p, vec3 out)
{
    float yaw = glm_rad(p->yaw), pitch = glm_rad(p->pitch);
    out[0] = cosf(yaw) * cosf(pitch);
    out[1] = sinf(pitch);
    out[2] = sinf(yaw) * cosf(pitch);
}
```

E a posição do olho, que é a posição dos *pés* mais a altura dos olhos:

```c
void player_eye(const Player *p, vec3 out)
{
    glm_vec3_copy((vec3){p->pos[0], p->pos[1] + EYE_HEIGHT, p->pos[2]}, out);
}
```

> 🧭 **Decisão de projeto: a "câmera" é só um jeito de olhar para o `Player`.**
> Não criamos uma `struct Camera`. O olho e a direção saem de funções do jogador. Em primeira pessoa, câmera e jogador são a mesma coisa; separar criaria sincronização sem benefício. Se um dia quiser câmera em terceira pessoa, é só calcular `olho = pos_olho - frente * distância`.

### Por que limitar o pitch a ±89°

Lembra do aviso do `f x cima`? Com `pitch = 90°`, `frente = (0, 1, 0)`, paralela ao `cima` do mundo. O produto vetorial dá o vetor nulo, a normalização divide por zero e a matriz vira **NaN**: tela preta (ou pior, tela piscando). Limitamos o pitch a ±89° para nunca chegar lá.

> 💡 **Curiosidade.** Esse problema, em que dois eixos de rotação se alinham e perdemos um grau de liberdade, é parente do famoso **gimbal lock**. Em FPS ele só aparece nos polos (olhar exatamente para cima/baixo), e o *clamp* resolve. Quaternions evitam o problema por construção, mas cobram o preço da complexidade.

## 4.4 Lendo o mouse

O GLFW oferece o modo `GLFW_CURSOR_DISABLED`: o cursor some e a posição reportada deixa de ser limitada pela tela (sem ele, ao chegar na borda você para de girar). Calculamos o *delta* em relação à posição anterior:

```c
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
```

Três detalhes:

- **`first_mouse`.** No primeiro evento, a "posição anterior" é lixo; sem tratar isso, a câmera dá um tranco enorme no primeiro frame.
- **`last_y - y` (invertido).** Em coordenadas de tela o Y cresce para baixo, mas queremos que mover o mouse para cima olhe para cima.
- **`MOUSE_SENS` em graus por pixel (0,1).** Em 1000 pixels de movimento, 100° de giro. Ajuste ao gosto.

Repare que **não multiplicamos o delta do mouse por `dt`**. O delta já é um *deslocamento* acumulado no intervalo entre eventos, não uma *velocidade*. Multiplicar por `dt` faria a sensibilidade depender do FPS (o oposto do que queremos).

## 4.5 Movendo-se

O WASD move o corpo no plano horizontal, **ignorando o pitch**. Se usássemos a direção 3D completa, olhar para o chão faria você andar mais devagar (ou enterrar-se no chão). As duas direções horizontais:

```
frente_horizontal = ( cos(yaw), 0,  sin(yaw))
direita           = (-sin(yaw), 0,  cos(yaw))
```

A `direita` é a `frente_horizontal x cima`, calculada à mão: `(cy,0,sy) x (0,1,0) = (-sy, 0, cy)`. Somamos as contribuições de cada tecla e, se o vetor resultante passar de comprimento 1, **normalizamos**: sem isso, andar na diagonal (W+D) seria √2 ≈ 41% mais rápido.

## 4.6 Projeção: FOV, aspect, near e far

A outra metade da "câmera" é a matriz de **projeção** (`glm_perspective(fovy, aspect, near, far)`):

- **`fovy` (70°).** Campo de visão **vertical**. É também o valor padrão do Minecraft. Quanto maior, mais "olho de peixe".
- **`aspect`.** Largura ÷ altura, para que círculos não virem elipses ao redimensionar. Protegemos contra altura zero (janela minimizada).
- **`near` e `far`.** Tudo fora desse intervalo é recortado.

O ponto traiçoeiro é o **depth buffer não ser linear**. A profundidade guardada é proporcional a `1/z`: a precisão é *enorme* perto da câmera e *minúscula* longe. Consequência prática: **empurre o `near` o mais longe que o jogo tolerar**. Com `near = 0.001` você teria "z-fighting" (superfícies cintilando) já a poucas dezenas de blocos; com `0.1` e `far = 300` a precisão é bem confortável para nosso mundo de 128 blocos.

(A matriz de perspectiva completa está no Apêndice B.)

## 4.7 Checkpoint: câmera livre

Para validar a câmera antes de existir jogador, usamos uma struct provisória. Ela será **descartada** no Capítulo 9.

```c
#include <math.h>
#include <cglm/cglm.h>

typedef struct {
    vec3   pos;
    float  yaw, pitch;          /* graus */
    double last_x, last_y;
    int    first_mouse;
} FreeCam;

static void on_mouse_move(GLFWwindow *win, double x, double y)
{
    FreeCam *c = glfwGetWindowUserPointer(win);
    if (c->first_mouse) { c->last_x = x; c->last_y = y; c->first_mouse = 0; }
    float dx = (float)(x - c->last_x);
    float dy = (float)(c->last_y - y);
    c->last_x = x; c->last_y = y;

    c->yaw   += dx * 0.10f;
    c->pitch += dy * 0.10f;
    if (c->pitch >  89.0f) c->pitch =  89.0f;
    if (c->pitch < -89.0f) c->pitch = -89.0f;
}

/* +1 se 'pos' pressionada, -1 se 'neg', 0 se nenhuma ou as duas */
static float axis(GLFWwindow *w, int key_pos, int key_neg)
{
    return (glfwGetKey(w, key_pos) == GLFW_PRESS ? 1.0f : 0.0f)
         - (glfwGetKey(w, key_neg) == GLFW_PRESS ? 1.0f : 0.0f);
}
```

Na `main`, antes do loop: `static FreeCam cam = { .pos = {0.5f, 2.0f, 4.0f}, .yaw = -90.0f, .first_mouse = 1 };`, mais `glfwSetWindowUserPointer(win, &cam)`, `glfwSetCursorPosCallback(win, on_mouse_move)` e `glfwSetInputMode(win, GLFW_CURSOR, GLFW_CURSOR_DISABLED)`. Dentro do loop, no lugar do `lookAt` fixo do Capítulo 3:

```c
float yaw = glm_rad(cam.yaw), pitch = glm_rad(cam.pitch);
vec3 front = { cosf(yaw) * cosf(pitch), sinf(pitch), sinf(yaw) * cosf(pitch) };
vec3 fwd   = { cosf(yaw), 0.0f, sinf(yaw) };
vec3 right = { -sinf(yaw), 0.0f, cosf(yaw) };

float f = axis(win, GLFW_KEY_W, GLFW_KEY_S);
float r = axis(win, GLFW_KEY_D, GLFW_KEY_A);
float v = axis(win, GLFW_KEY_SPACE, GLFW_KEY_LEFT_SHIFT);
vec3 move = { fwd[0] * f + right[0] * r, v, fwd[2] * f + right[2] * r };
if (glm_vec3_norm(move) > 1e-4f) glm_vec3_normalize(move);
glm_vec3_muladds(move, 8.0f * dt, cam.pos);          /* pos += move * (8 * dt) */

vec3 center;
glm_vec3_add(cam.pos, front, center);                /* alvo = olho + frente */
glm_lookat(cam.pos, center, (vec3){0, 1, 0}, view);
```

Se tudo estiver certo, você voa em volta do cubo, e **olhar para cima/baixo nunca "vira de cabeça para baixo"**. Experimente: o que acontece se você tirar o `clamp` do pitch e olhar direto para cima?

> 🤔 **Para pensar.**
> **P4.1.** Por que limitar o pitch em ±89° e não em ±90°?
> **P4.2.** Por que não multiplicamos o delta do mouse por `dt`, mas multiplicamos a velocidade do WASD?
> **P4.3.** Por que o movimento horizontal ignora o pitch? Em que situação você *gostaria* de usá-lo?

\newpage

# Capítulo 5: Pensando em voxels (o modelo de dados)

> **Checkpoint:** ler e escrever blocos por coordenada, validado por um pequeno teste.

Até aqui só desenhamos um cubo. Um mundo de Minecraft é, no fundo, uma **grade 3D de inteiros**: cada célula guarda "que tipo de bloco há aqui". Quase tudo no jogo (desenho, colisão, quebrar/colocar) é uma pergunta ou uma escrita nessa grade. Por isso o modelo de dados merece cuidado.

## 5.1 Tipos de bloco

```c
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
```

`BLOCK_AIR = 0` é uma escolha deliberada: um mundo recém-zerado (com `calloc` ou `memset(0)`) já nasce **vazio e válido**. Você ganha a inicialização de graça.

Repare em `block_tile(bloco, face)`: o *tipo* de bloco não conhece cores nem pixels, só **qual tile do atlas** usar em cada face (a grama usa uma textura no topo, outra nas laterais e terra embaixo). Texturas só aparecem no Capítulo 8; a função fica pronta aqui para o mesh já nascer certo.

## 5.2 Por que chunks?

Nosso mundo terá 128 × 64 × 128 blocos, ou seja, **1 MiB** com um byte por bloco. Caberia em um único array. Então por que dividir?

Porque o **custo está na malha (*mesh*), não na memória**. Cada vez que você quebra um bloco, a malha precisa ser reconstruída:

- Com um array único, reconstruímos **1.048.576 células**.
- Com chunks de 16 × 64 × 16, reconstruímos **16.384** (e, se o bloco estiver na borda, mais um vizinho).

Chunks também são a unidade natural de **carregamento/descarregamento** (mundos infinitos), de **recorte por visibilidade** (frustum culling) e de **multithreading**.

> 💡 **Curiosidade.** O Minecraft também divide o mundo em colunas de 16 × 16 blocos. Internamente, cada coluna é fatiada em *seções* de 16 × 16 × 16.

Nossas dimensões ficam em `world.h`:

```c
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
```

> 🧭 **Decisão de projeto: dimensões potências de dois.**
> 16 e 64 permitem trocar `x / 16` por `x >> 4` e `x % 16` por `x & 15` (para valores não negativos). Hoje o compilador faz isso sozinho; o ganho real é mental: o limite entre chunks cai sempre em múltiplos "redondos".

## 5.3 O layout do array

```c
uint8_t blocks[CHUNK_W][CHUNK_H][CHUNK_W];   /* [x][y][z] */
```

- **`uint8_t`:** até 256 tipos de bloco em 1 byte. Um chunk ocupa `16 × 64 × 16 = 16.384 bytes = 16 KiB`, o que cabe no cache L1 de dados da maioria das CPUs (tipicamente 32 KiB ou mais), então varrer um chunk é muito rápido.
- **Ordem `[x][y][z]`:** em C, o **último índice varia mais rápido** na memória. Se nossos laços aninhados têm `z` por dentro, percorremos memória **sequencialmente**, que é o padrão amigável ao cache.

## 5.4 Coordenadas de mundo × coordenadas locais

O resto do jogo fala em **coordenadas de mundo** (`x = 70, z = 33`). O array vive em **coordenadas locais** (`0..15`). A tradução:

```
cx = x / 16      lx = x % 16          (qual chunk, e onde dentro dele)
```

Todas as consultas passam por `world_get_block` e `world_set_block`:

```c
static Chunk *chunk_at(const World *w, int cx, int cz)
{
    return &w->chunks[cx * WORLD_CZ + cz];
}
```

```c
BlockType world_get_block(const World *w, int x, int y, int z)
{
    if (x < 0 || z < 0 || y < 0 || y >= CHUNK_H) return BLOCK_AIR;
    int cx = x / CHUNK_W, cz = z / CHUNK_W;
    if (cx >= WORLD_CX || cz >= WORLD_CZ) return BLOCK_AIR;
    return (BlockType)chunk_at(w, cx, cz)->blocks[x % CHUNK_W][y][z % CHUNK_W];
}
```

> ⚠️ **Cuidado: divisão de inteiros negativos em C.**
> Em C, `-1 / 16` é `0` (truncamento em direção a zero) e `-1 % 16` é `-1`. Um `world_get_block(-1, ...)` ingênuo acabaria indexando o chunk 0 com `lx = -1`: leitura fora do array, comportamento indefinido. Por isso tratamos `x < 0` *antes* de dividir. A alternativa "correta" seria usar **divisão com piso** (`floor_div`), que só vale a pena quando houver mundo infinito com coordenadas negativas.

### Duas respostas para "o que há ali fora?"

Repare que `world_get_block` devolve **AIR** fora do mundo, mas a física precisa de outra resposta:

```c
int world_is_solid(const World *w, int x, int y, int z)
{
    if (x < 0 || z < 0 || x >= WORLD_SIZE_X || z >= WORLD_SIZE_Z || y < 0) return 1;
    if (y >= CHUNK_H) return 0;
    return world_get_block(w, x, y, z) != BLOCK_AIR;
}
```

Para o **mesh**, "fora do mundo é ar" significa que desenhamos as faces da borda. Para a **física**, "fora do mundo é sólido" cria paredes invisíveis que impedem o jogador de cair no vazio. Mesma pergunta, duas semânticas, e por isso **duas funções**, em vez de uma função cheia de `if`s e parâmetros.

## 5.5 Escrever: o `dirty` e os vizinhos

```c
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
```

Duas ideias aqui:

1. **A flag `dirty`.** Alterar um bloco *não* reconstrói o mesh na hora; apenas marca. A reconstrução acontece depois, em um ponto controlado do frame (Capítulo 7). Se você quebrar 10 blocos no mesmo frame, o chunk é remontado **uma vez**.
2. **Vizinhos na borda.** As faces entre dois chunks dependem dos blocos *de ambos*. Se você quebra o bloco `lx = 0`, a parede do chunk ao lado, que antes estava escondida, agora precisa aparecer. Por isso marcamos o vizinho como sujo também.

## 5.6 Onde guardamos tudo

```c
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
```

`World` e `Chunk` vão para o **heap** (`calloc`), não para a pilha. Um `World` inteiro tem ~1 MiB; o tamanho padrão da pilha da thread principal no Windows é justamente 1 MiB. Declarar isso como variável local seria um *stack overflow* garantido.

> 🧭 **Decisão de projeto: `Chunk` contém dados *e* handles de GPU.**
> `Chunk` guarda os blocos (dados) junto de `vao`, `vbo` e `vertex_count` (estado de renderização). Isso é pragmático: um só lugar para olhar. Engines grandes separam as duas coisas (`ChunkData` × `ChunkMesh`) para poder gerar o mesh em outra thread sem tocar em objetos GL. Para nosso tamanho, juntos é mais legível.

> 🧭 **Decisão de projeto: o mundo é uma caixa-preta com API pequena.**
> O resto do código **nunca** acessa `chunk->blocks` diretamente; só `get/set/is_solid/surface_height`. Isso permite trocar a representação interna (paletas, seções esparsas, compressão) sem tocar em física, raycast ou main.

## 5.7 Checkpoint: um teste sem janela

Cole isto no começo da `main` (antes do `glfwInit`). `world_create` não usa OpenGL, só o mesh usa:

```c
#include "world.h"
...
World *w = world_create(1337);
world_set_block(w, 5, 10, 5, BLOCK_STONE);
printf("get(5,10,5)  = %d (esperado %d)\n", world_get_block(w, 5, 10, 5), BLOCK_STONE);
printf("get(-1,10,5) = %d (esperado 0)\n",  world_get_block(w, -1, 10, 5));
printf("solid(-1,10,5) = %d (esperado 1)\n", world_is_solid(w, -1, 10, 5));
```

Neste ponto `world.c` só tem os acessores; `generate_chunk` (próximo capítulo) ainda não existe, então o mundo está vazio.

> 🤔 **Para pensar.**
> **P5.1.** Quanto valem `-1 / 16` e `-1 % 16` em C? Que bug isso causaria sem o teste `x < 0`?
> **P5.2.** Se `CHUNK_H` virasse 256, quanta memória cada chunk usaria? Que estrutura você consideraria usar no lugar do array único?

\newpage

# Capítulo 6: Terreno procedural

> **Checkpoint:** um mapa de alturas impresso no terminal.

## 6.1 O problema da aleatoriedade "com memória"

Queremos um terreno **natural** (colinas suaves, não ruído de TV) e **reproduzível** (mesma `seed`, mesmo mundo). `rand()` não serve: tem estado global, então o resultado depende da *ordem* em que você pede números.

A solução clássica é um **ruído baseado em hash**: uma função pura `valor = f(x, z, seed)`. Para o mesmo ponto, sempre o mesmo valor, em qualquer ordem, em qualquer thread. É como uma biblioteca onde cada livro tem posição fixa: você pode abrir a página 3.000 sem ler as 2.999 anteriores.

> 💡 **Curiosidade.** Ken Perlin criou o ruído que leva seu nome nos anos 1980 para o filme *Tron*, por estar cansado do visual "plástico" da computação gráfica da época. Ele recebeu um Oscar técnico por isso, em 1997.

## 6.2 O código do ruído

```c
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
```

Vamos por partes.

### O hash (`hash2`)

Misturamos `x`, `y` e `seed` multiplicando por constantes primas grandes e embaralhando os bits com `xor` e *shifts*. O objetivo é o **efeito avalanche**: mudar 1 bit da entrada deve mudar ~metade dos bits da saída, para que coordenadas vizinhas pareçam sem relação. Convertemos para `uint32_t` antes de multiplicar porque overflow de inteiro *com sinal* é comportamento indefinido em C, enquanto o de *sem sinal* é bem definido (módulo 2³²).

### Value noise (`value_noise`)

1. Cada ponto inteiro da grade ganha um valor aleatório fixo em `[0, 1)` (`lattice`).
2. Para um ponto `(x, y)` qualquer, olhamos os **4 cantos** da célula que o contém e fazemos **interpolação bilinear**.
3. Em vez de usar `t` direto, passamos por `smooth(t) = t²(3 - 2t)`.

Por que `smooth`? Com interpolação linear pura, a inclinação muda bruscamente ao cruzar cada linha da grade, criando "vincos" visíveis que denunciam o reticulado. A curva *smoothstep* tem derivada zero nas pontas, então as células se encaixam sem quinas. (O "improved noise" de Perlin vai além, com a curva quíntica `6t⁵ - 15t⁴ + 10t³`, mas aqui o ganho não compensa.)

### fBm: somar oitavas

Um único ruído tem uma só "escala" de detalhe. O **fBm** (*fractional Brownian motion*) soma várias cópias com **o dobro da frequência e metade da amplitude** a cada passo:

```
oitava 0:  colinas grandes     (amplitude 1)
oitava 1:  colinas médias      (amplitude 1/2)
oitava 2:  morrinhos           (amplitude 1/4)
oitava 3:  rugosidade          (amplitude 1/8)
```

Dividimos pela soma das amplitudes (`norm`), então o resultado continua em `[0, 1]`. Cada oitava usa `seed + i` para que não sejam cópias idênticas em escalas diferentes.

## 6.3 Do ruído à altura

```c
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
```

**A altura de cada coluna:**

```c
float n = fbm(wx * 0.015f, wz * 0.015f, w->seed, 4);
int   h = 30 + (int)((n - 0.5f) * 80.0f);
```

- **`0.015`** é a frequência: a oitava mais grossa tem "comprimento de onda" de `1 / 0.015 ≈ 67` blocos. A quarta oitava, ~8 blocos.
- **`(n - 0.5) * 80`:** o fBm tende a se concentrar perto de 0,5 (somar valores independentes "puxa para a média"), então usamos uma amplitude generosa. Em um teste com a seed 1337, as alturas foram de 3 a 58.
- **`30 +`:** a altura média. Depois limitamos com `clamp` para `[1, CHUNK_H - 12]`: o `-12` reserva espaço para as copas das árvores.

**As camadas** (de cima para baixo): 1 bloco de grama, 3 de terra, pedra o resto. Onde `h <= SEA_LEVEL + 1` viram areia. Ainda não temos água, mas a areia já sugere onde ela estaria.

## 6.4 Árvores e a regra da margem

O laço das árvores só percorre `x, z` de `2` a `CHUNK_W - 3`. Por quê?

Uma copa tem raio 2. Se a árvore nascer a menos de 2 blocos da borda, a copa **invadiria o chunk vizinho**, que talvez ainda não tenha sido gerado (ou já esteja pronto e precise ser corrigido). Com a margem, cada chunk é **autossuficiente**: depende só de `(seed, cx, cz)`.

Essa independência é uma propriedade valiosa: você pode gerar chunks em qualquer ordem, sob demanda, ou em paralelo, sem se preocupar com dependências.

> 🧭 **Decisão de projeto: margem em vez de "passo de decoração".**
> Minecraft gera o terreno primeiro e decora depois, deixando decorações atravessarem fronteiras (o que exige regras de ordem entre chunks). Nós preferimos a margem: custa um pouco de naturalidade (nenhuma árvore perto da borda do chunk) e economiza um sistema inteiro.

**A forma da árvore:** o laço da copa percorre camadas (`ly`) de `topo - 2` a `topo + 1`; as duas de baixo têm raio 2 (com os cantos removidos para arredondar), as duas de cima raio 1. O tronco é escrito **depois** da copa para sobrescrevê-la. A aleatoriedade (`hash2(...) % 90 == 0`) é determinística: a mesma árvore aparece no mesmo lugar sempre.

E, para fechar o ciclo de vida:

```c
void world_destroy(World *w)
{
    for (int i = 0; i < WORLD_CX * WORLD_CZ; i++) {
        if (w->chunks[i].vbo) glDeleteBuffers(1, &w->chunks[i].vbo);
        if (w->chunks[i].vao) glDeleteVertexArrays(1, &w->chunks[i].vao);
    }
    free(w->chunks);
    free(w);
}
```

## 6.5 Checkpoint: o mapa de alturas

Cole na `main`, logo após criar o mundo, e rode:

```c
World *w = world_create(1337);
for (int z = 0; z < WORLD_SIZE_Z; z += 4) {
    for (int x = 0; x < WORLD_SIZE_X; x++) {
        int h = world_surface_height(w, x, z);          /* 0..63 */
        putchar(" .:-=+*#%@"[h * 9 / 63]);              /* 10 "tons" */
    }
    putchar('\n');
}
```

Você deve ver "manchas" suaves de `.:-=+*#%@`. Troque a seed, mude `0.015f` para `0.15f` e `0.0015f` e veja o que o parâmetro de frequência faz com a paisagem.

> 🤔 **Para pensar.**
> **P6.1.** O que acontece com a paisagem se a frequência subir de `0.015` para `0.15`? E se descer para `0.0015`?
> **P6.2.** Sem a margem de 2 blocos, que problemas surgiriam ao gerar árvores perto da borda do chunk? Como você resolveria?

\newpage

# Capítulo 7: Do voxel ao triângulo (o mesh)

> **Checkpoint:** o terreno inteiro desenhado em tons de cinza, voando com a câmera do Capítulo 4.

Este é o capítulo central do livro. É aqui que uma grade de números vira geometria.

## 7.1 A ideia: desenhar só o que se vê

A abordagem ingênua é desenhar um cubo (12 triângulos) por bloco. Um único chunk cheio teria 16.384 × 12 ≈ 196 mil triângulos, e o mundo todo passaria de 12 milhões. Mas quase todos esses triângulos são **invisíveis**: ficam entre dois blocos sólidos, enterrados.

A regra do ***face culling***:

> Uma face de um bloco sólido só existe se o vizinho naquela direção for **ar**.

Isso reduz a geometria a **uma casca fina** em volta do terreno e das cavernas. É a técnica que torna viável qualquer jogo de voxels.

## 7.2 O formato do vértice

Cada vértice tem **6 floats** (24 bytes):

```
x y z   u v   shade
posição textura brilho
```

> 🧭 **Decisão de projeto: coordenadas de mundo direto no vértice.**
> Ao montar o mesh, já somamos o deslocamento do chunk (`wx = cx * 16 + x`). Assim todos os chunks são desenhados com **a mesma matriz** `vp`, sem uma matriz de modelo por chunk. **Vantagem:** shader mais simples, um uniform só. **Desvantagem:** os floats em coordenadas de mundo perdem precisão em mundos gigantes (a milhares de blocos de distância); engines grandes usam coordenadas locais ao chunk + um `uniform vec3 uChunkOffset`. Para um mundo de 128 blocos, é irrelevante.

Também poderíamos **empacotar** o vértice: posição local cabe em 3 bytes, o tile em 1, o brilho em 1. Daria ~6 bytes em vez de 24. Deixamos como exercício.

## 7.3 A tabela de faces

Em vez de um `switch` gigante, descrevemos as 6 faces **como dados**:

```c
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
```

Cada linha define: o deslocamento do **vizinho** a consultar, os **4 cantos** (em ordem), as **4 coordenadas de textura** e o **brilho**. Os `QUAD_IDX` transformam o quadrilátero em dois triângulos: `(0,1,2)` e `(0,2,3)`.

> 🧭 **Decisão de projeto: código orientado a dados.**
> Um laço `for f in 0..5` percorrendo uma tabela substitui seis blocos de código quase idênticos. Corrigir um canto de uma face é editar *uma linha de dados*, não caçar um bug em seis lugares.

### Por que essa ordem dos cantos? (o winding)

Habilitamos `GL_CULL_FACE` com `glFrontFace(GL_CCW)`: a GPU descarta triângulos cujos vértices aparecem em **sentido horário** na tela. Logo, vistos **de fora**, os cantos de cada face precisam estar em sentido **anti-horário** (CCW).

Para checar, use o produto vetorial. A normal de um triângulo `(v0, v1, v2)` é `(v1 - v0) x (v2 - v0)`, e ela deve apontar *para fora* do bloco. Para a face `+X`:

```
v0 = (1,0,0)   v1 = (1,1,0)   v2 = (1,1,1)
v1 - v0 = (0,1,0)       v2 - v0 = (0,1,1)

(0,1,0) x (0,1,1) = (1·1 - 0·1,  0·0 - 0·1,  0·1 - 1·0) = (1, 0, 0)    ✓ aponta para +X
```

Se uma face "sumir" quando você olha para ela de fora (mas aparecer de dentro do bloco), a ordem dos cantos está invertida.

### E as coordenadas de textura?

Queremos a textura **legível**, sem espelhar. Olhando a face de fora, `u` deve crescer para a **direita** e `v` para **cima**. O vetor "direita" de quem olha é `frente x cima`. Para a face `+X`, quem a vê está em `+X` olhando para `-X`:

```
direita = (-1,0,0) x (0,1,0) = (0·0 - 0·1,  0·0 - (-1)·0,  (-1)·1 - 0·0) = (0, 0, -1)
```

Ou seja, a direita é `-Z`: `u` cresce quando `z` **diminui**. Confira na tabela: o canto `(1,0,0)` (z = 0) recebe `u = 1`; o canto `(1,0,1)` (z = 1) recebe `u = 0`. 

## 7.4 Construindo o mesh

Primeiro, um vetor dinâmico de floats, com crescimento por dobra (custo amortizado O(1) por `push`):

```c
#define FLOATS_PER_VERTEX 6   /* x y z u v shade */
#define UV_EPS 0.0005f        /* evita "sangrar" pixel do tile vizinho   */

typedef struct { float *d; size_t len, cap; } Vec;
static Vec g_vec;             /* buffer reutilizado a cada remontagem */
```

```c
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
```

O buffer `g_vec` é `static` e **reaproveitado** a cada reconstrução: zeramos o `len` sem liberar a memória, então depois das primeiras vezes não há mais `malloc`. (Nota para o futuro: ele não é *thread-safe*. Mesh em várias threads exigiria um buffer por thread.)

Agora a função principal:

```c
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
```

Percorrendo o laço:

1. Para cada bloco não-ar, para cada uma das 6 faces...
2. ...consultamos o vizinho com **`world_get_block` em coordenadas de mundo**. Isso funciona através de fronteiras de chunk de graça, e é a razão pela qual o `set_block` marca vizinhos como sujos.
3. Se o vizinho é sólido, **pulamos** a face. Se é ar, emitimos 6 vértices.
4. `y + dy < 0` pula a face de baixo do fundo do mundo (nunca visível).

### Os UVs e o `UV_EPS`

```
u = (tile + (EPS + uu * (1 - 2*EPS))) / ATLAS_COLS
v =          EPS + vv * (1 - 2*EPS)
```

`uu` e `vv` (0 ou 1) vêm da tabela; o primeiro termo seleciona o tile no atlas. O `EPS` (0,0005) **encolhe levemente** cada tile para evitar que erros de arredondamento façam a GPU amostrar o pixel do tile vizinho. Veremos o sintoma no Capítulo 8.

### Enviando para a GPU

A parte final da função cria o VAO/VBO **sob demanda** (chunks totalmente vazios nem alocam) e envia os dados. Dois detalhes que você, que já conhece VAOs, vai apreciar:

- Os `glVertexAttribPointer` são configurados **uma vez**, na criação do VAO. O VAO memoriza o VBO ligado naquele momento; nas reconstruções, só chamamos `glBufferData` de novo, e o VAO continua válido.
- Usamos `GL_STATIC_DRAW`: o mesh é reescrito *raramente* (edições) e desenhado *milhares de vezes* (frames).

## 7.5 Reconstruindo com orçamento

```c
void world_update_meshes(World *w, int budget)
{
    for (int cx = 0; cx < WORLD_CX && budget > 0; cx++)
    for (int cz = 0; cz < WORLD_CZ && budget > 0; cz++)
        if (chunk_at(w, cx, cz)->dirty) {
            chunk_build_mesh(w, cx, cz);
            budget--;
        }
}
```

O parâmetro `budget` limita quantos chunks remontamos **por frame**. No arranque remontamos todos de uma vez (64); durante o jogo, 4 por frame. Se você quebrar vários blocos de uma vez, ou uma explosão tocar muitos chunks, o custo é diluído em vários frames, em vez de um engasgo.

E o desenho, que é deliberadamente trivial:

```c
void world_draw(const World *w)
{
    for (int i = 0; i < WORLD_CX * WORLD_CZ; i++) {
        const Chunk *c = &w->chunks[i];
        if (c->vertex_count == 0) continue;
        glBindVertexArray(c->vao);
        glDrawArrays(GL_TRIANGLES, 0, c->vertex_count);
    }
}
```

64 chunks = 64 *draw calls*. É uma quantidade confortável; em mundos grandes você agruparia ou usaria *multi-draw*.

## 7.6 E o EBO? (a pergunta que você já deve estar fazendo)

Usamos **6 vértices por face** (dois triângulos independentes), sem índices. Com um EBO, seriam **4 vértices + 6 índices**:

| Formato | Bytes por face |
|---|---|
| 6 vértices (o nosso) | 6 × 24 = **144** |
| 4 vértices + 6 índices `uint32` | 4 × 24 + 6 × 4 = **120** (−17%) |
| 4 vértices + 6 índices `uint16` | 4 × 24 + 6 × 2 = **108** (−25%) |

> 🧭 **Decisão de projeto: sem EBO.**
> A economia é real, mas o código fica mais longo (gerenciar índices, base de vértices por face, e cuidado com o limite de 65.535 vértices do `uint16`). Como o objetivo é *entender*, ficamos com o caminho curto. Trocar para EBO é o primeiro exercício do Capítulo 12.

## 7.7 Checkpoint: terreno em cinza

Para ver o mesh antes de existir textura, troque temporariamente o fragment shader do bloco para mostrar só o brilho:

```glsl
#version 330 core
in float vShade;
out vec4 FragColor;
void main() { FragColor = vec4(vec3(vShade), 1.0); }
```

O vertex shader de bloco é o final, e vale o mesmo comentário de antes (strings C no projeto):

```glsl
#version 330 core
layout(location = 0) in vec3  aPos;     // ja em coordenadas de MUNDO
layout(location = 1) in vec2  aUV;
layout(location = 2) in float aShade;   // brilho fixo por direcao da face
uniform mat4 uVP;
out vec2  vUV;
out float vShade;
out vec3  vWorld;
void main() {
    vUV = aUV; vShade = aShade; vWorld = aPos;
    gl_Position = uVP * vec4(aPos, 1.0);
}
```

(O fragment shader precisa declarar apenas os `in` que usa; os outros são ignorados.)

Na `main`: criar o mundo, `world_update_meshes(world, WORLD_CX * WORLD_CZ)`, habilitar `GL_CULL_FACE` (`glCullFace(GL_BACK)`, `glFrontFace(GL_CCW)`) e, a cada frame, `glUseProgram(block_prog)`, enviar `uVP` e chamar `world_draw(world)`. Posicione a `FreeCam` perto de `(64, 50, 64)`.

Se tudo deu certo você vê colinas em cinza, com topos claros e laterais mais escuras. Se faltarem faces, volte à seção do *winding*.

> 🤔 **Para pensar.**
> **P7.1.** O que você veria se invertesse a ordem dos cantos de *uma* face na tabela?
> **P7.2.** Por que o brilho do topo é `1.0` e o da face de baixo `0.5`? De onde vem essa ideia de iluminação?

\newpage

# Capítulo 8: Texturas, atlas, iluminação falsa e neblina

> **Checkpoint:** o mundo com grama, terra, pedra, areia e árvores, em direção à névoa.

## 8.1 Um atlas em vez de várias texturas

Temos 9 tiles de 16 × 16 pixels. Podíamos criar 9 texturas, mas aí cada chunk precisaria **trocar de textura** no meio do desenho (e um chunk mistura vários tipos de bloco). A solução é um **atlas**: todos os tiles lado a lado em **uma** textura. O bloco apenas escolhe *onde* amostrar.

Nosso atlas tem 16 colunas × 1 linha de tiles (256 × 16 pixels). Só 9 colunas são usadas; as outras 7 são espaço para crescer. Largura 256 é potência de dois, o que agrada hardware e ferramentas.

> 💡 **Curiosidade.** O Minecraft clássico usava exatamente isso: um `terrain.png` de 256 × 256 com tiles de 16 × 16. Nosso atlas tem o mesmo tamanho de tile.

> 🧭 **Decisão de projeto: atlas procedural.**
> Em vez de carregar PNGs (o que exigiria `stb_image` e arquivos de recurso), **desenhamos as texturas por código**, com ruído por pixel. **Vantagem:** zero dependências, zero caminhos de arquivo. **Desvantagem:** só serve para artes simples. Quando você quiser arte de verdade, troque `atlas_create` por um loader e mantenha o mesmo contrato (um atlas com tiles 16 × 16).

## 8.2 Gerando o atlas

```c
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
```

Pontos-chave:

- **Cada tile é uma pequena receita**: uma cor-base mais `n`, um ruído em `[-16, 16]` por pixel (`hash2(x, y, tile)`), o que dá aquele aspecto "granulado".
- **Detalhes que dão personalidade:** a borda da grama lateral é irregular (`drip`), o tronco tem listras, o topo do tronco tem anéis concêntricos, as tábuas têm emendas.
- **Magenta de erro** `(255, 0, 255)`: se algum `tile` não tiver `case`, o pixel fica magenta, o "pink-and-black" universal que diz "faltou textura aqui".
- **Orientação:** `y = 15` é o **topo** visual do tile, porque a primeira linha enviada ao OpenGL é `v = 0`, e no mesh `v = 1` é "para cima". Se suas texturas ficarem de cabeça para baixo, é aqui que você olha.

### Parâmetros da textura

```c
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
```

- **`GL_NEAREST`:** pega o texel mais próximo, sem borrar. É o que dá o visual "pixelado" de Minecraft. Com `GL_LINEAR`, as texturas ficariam embaçadas ao serem ampliadas.
- **`GL_CLAMP_TO_EDGE`:** fora de `[0,1]` repete a borda em vez de dar a volta, outro seguro contra "sangramento".
- **Sem mipmaps**, de propósito. Num atlas, os níveis menores de mipmap *misturam tiles vizinhos* (o pixel pequeno é média de uma região que cruza fronteiras). Soluções profissionais usam **texture arrays** (`GL_TEXTURE_2D_ARRAY`), onde cada tile é uma camada separada e os mipmaps não vazam.

## 8.3 O shader de bloco completo

O vertex shader já é o do Capítulo 7. O fragment shader final:

```glsl
#version 330 core
in vec2  vUV;
in float vShade;
in vec3  vWorld;
uniform sampler2D uAtlas;
uniform vec3 uCamPos;
uniform vec3 uFogColor;
uniform vec2 uFogRange;   // x = inicio, y = fim da neblina
out vec4 FragColor;
void main() {
    vec4 t = texture(uAtlas, vUV);
    if (t.a < 0.5) discard;           // pronto p/ texturas com buracos
    vec3 c = t.rgb * vShade;
    float d = length(vWorld - uCamPos);
    float f = clamp((d - uFogRange.x) / (uFogRange.y - uFogRange.x), 0.0, 1.0);
    FragColor = vec4(mix(c, uFogColor, f), 1.0);
}
```

Três efeitos em poucas linhas:

**Textura × brilho.** `t.rgb * vShade` multiplica a cor do texel pelo brilho da face. Topo = 1,0; Z = 0,65; X = 0,8; fundo = 0,5. Não há luz alguma: é uma **iluminação falsa** que dá volume (as faces "viradas para o sol" ficam claras). É a mesma ideia de ter "luz vindo de cima" que o Minecraft usa em seus blocos.

**Neblina.** Calculamos a distância do fragmento à câmera e misturamos linearmente (`mix`) a cor do bloco com a cor da névoa entre 50 e 110 blocos. Usamos como cor da névoa **exatamente a cor do céu** (`glClearColor`): os blocos distantes se dissolvem no fundo, e a borda do mundo (a 128 blocos) fica escondida.

**`discard`.** Se o texel for transparente, o fragmento é descartado. Nenhum tile atual é transparente, mas o código está pronto para folhas "vazadas" ou grades. (Cuidado: `discard` desliga alguns otimizações de *early-z* na GPU. Para este projeto é irrelevante.)

> 🧭 **Decisão de projeto: neblina por fragmento, com distância real.**
> Poderíamos calcular a névoa no vertex shader (mais barato), mas com faces grandes a interpolação linear da névoa ficaria visivelmente errada. Calcular por fragmento custa um `length` por pixel, uma ninharia moderna.

## 8.4 Uniforms: localizar uma vez só

Na `main`, os `glGetUniformLocation` são feitos **uma vez**, antes do loop, e só os valores são enviados a cada frame. Consultar a *location* por nome toda hora é uma busca de string no driver.

## 8.5 Os sintomas clássicos

| Sintoma | Causa provável |
|---|---|
| Linhas finas de outra cor entre blocos | `UV_EPS` ausente/pequeno, ou filtro `LINEAR` |
| Texturas de cabeça para baixo | orientação de `y` no atlas ou `v` na tabela |
| Texturas espelhadas em algumas faces | UVs inconsistentes na tabela de faces |
| Tudo magenta | falta um `case` do tile |
| Tela preta, sem erro | `uAtlas` não setado, `glActiveTexture` fora de lugar |

## 8.6 Checkpoint

Reative o fragment shader completo, crie o atlas (`atlas_create`), ligue-o na unidade 0 (`glActiveTexture`, `glBindTexture`, `glUniform1i(u_atlas, 0)`) e envie `uCamPos`, `uFogColor` e `uFogRange`. Você deve ver um mundo com árvores, areia nas áreas baixas, e uma névoa azulada ao fundo.

> 🤔 **Para pensar.**
> **P8.1.** Por que `GL_NEAREST` em vez de `GL_LINEAR`? E por que o atlas fica sem mipmaps?
> **P8.2.** Por que a cor da névoa é igual à cor de fundo (`glClearColor`)? O que aconteceria se fossem diferentes?

\newpage

# Capítulo 9: Física e colisão

> **Checkpoint:** um jogador que cai, pousa, pula e desliza em paredes, testado primeiro sem janela.

Até aqui voávamos pelo mundo como fantasmas. Agora vamos ter **corpo**: gravidade, chão, paredes. Todo o código de `player.c` é puro C: **não inclui OpenGL**. Ele só pergunta ao mundo "essa célula é sólida?".

## 9.1 O corpo: uma caixa (AABB)

Representamos o jogador como uma **AABB** (*axis-aligned bounding box*): uma caixa alinhada aos eixos, sem rotação. Por quê? Porque testar caixa contra uma grade de voxels é trivial: basta listar as células que a caixa toca.

```c
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
```

- **`pos` é a posição dos pés** (centro da base da caixa), não do centro do corpo. Assim "estar no chão" é simplesmente `pos.y` igual ao topo de um bloco.
- Medidas `0,6 × 1,8` e olhos a `1,62` são as do próprio Minecraft: o jogador passa por vãos de 2 blocos de altura, mas não por vãos de 1.

```c
#define GRAVITY      28.0f
#define JUMP_SPEED    8.5f
#define MAX_FALL     50.0f
#define WALK_SPEED    4.5f
#define FLY_SPEED    12.0f
```

> 💡 **Curiosidade.** A velocidade de caminhada do Minecraft é cerca de 4,3 blocos por segundo; usamos 4,5 por ser um número redondo. A gravidade e o pulo foram ajustados "no olho", porque o que importa é a *sensação*.

## 9.2 Detectando colisão

```c
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
```

A caixa ocupa o intervalo contínuo `[min, max)`. As células tocadas são de `floor(min)` até `ceil(max) - 1`. Um exemplo com `pos.x = 3.0` e meia largura `0.3`: a caixa vai de `2.7` a `3.3`, tocando as células `2` e `3`. Se estivesse exatamente em `x ∈ [3.0, 3.6)`, `ceil(3.6) - 1 = 3`: só a célula 3, **sem** vazar para a célula 2 nem para a 4.

Se `max` for um inteiro exato (por exemplo, o topo da cabeça em `y = 23.0`), `ceil(23.0) - 1 = 22`: não incluímos a célula 23 só porque encostamos nela. É o que queremos: encostar não é bater.

## 9.3 Resolvendo por eixo

```c
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
```

Em vez de mover o jogador em 3D e tentar descobrir de onde veio a colisão, movemos **um eixo por vez**: primeiro X, depois Z, depois Y. Se o novo lugar colide, **desfazemos só aquele eixo**.

A consequência é a sensação certa de jogo: se você anda na diagonal contra uma parede, o componente perpendicular à parede é cancelado, mas o paralelo continua, e você **desliza**. Se resolvêssemos o deslocamento 3D inteiro, bater em uma parede travaria você no lugar.

Quando o eixo Y colide *descendo* (`delta < 0`), sabemos que há chão sob os pés: `on_ground = 1`. Em qualquer colisão zeramos a velocidade naquele eixo (bater a cabeça no teto também interrompe a subida).

> 🧭 **Decisão de projeto: "desfazer" em vez de "encostar exatamente".**
> A abordagem exata calcularia a distância até a face do bloco e colaria o jogador nela. A nossa devolve o jogador para a posição *anterior ao passo*, o que pode deixá-lo a alguns milímetros do chão (em um teste, ele pousou em `y = 21.006` sobre uma superfície em `y = 21`). Visualmente imperceptível e muito mais curto; e `on_ground` continua correto, pois a gravidade o empurra novamente a cada frame.

## 9.4 Atualizando o jogador

```c
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
```

Passo a passo:

1. **`dt` limitado a 0,05 s.** Se o jogo engasgar por meio segundo (carregar algo, arrastar a janela), um `dt` de 0,5 faria o jogador "teleportar" vários blocos de uma vez, atravessando o chão. O clamp transforma o engasgo em câmera lenta, que é muito mais saudável.
2. **A intenção de movimento** vem somando `frente × entrada_f + direita × entrada_d`, normalizada se passar de 1 (para a diagonal não ser mais rápida; ver Capítulo 4).
3. **Velocidade horizontal é atribuída diretamente** (`vel.x = wish.x * speed`), sem aceleração: o jogador para na hora que você solta a tecla. É o feel "arcade" do Minecraft; um jogo de corrida ou plataforma sofisticado usaria aceleração e atrito.
4. **Vertical:** no modo normal, `vel.y -= GRAVITY * dt` (limitado em `MAX_FALL`), e pular é **atribuir** uma velocidade para cima, mas só se `on_ground`. No modo voo, a velocidade vertical vem direto das teclas.
5. **`on_ground = 0` antes de mover.** A flag é reconstruída a cada frame pela colisão do eixo Y.

### Quanto pula o jogador?

Para um lançamento vertical, a altura máxima teórica é `h = v² / (2g)`:

```
v = 8.5      g = 28      h = 8.5² / (2 × 28) = 72.25 / 56 ≈ 1.29 blocos
```

Precisamos de `h` **maior que 1** (para subir um degrau) e **menor que 2** (para não pular paredes de 2 blocos). Com 1,29 teórico (e cerca de 1,4 no teste a 60 FPS, por causa da discretização do passo), sobra folga dos dois lados.

> ⚠️ **Limitação conhecida: tunelamento.**
> Só testamos colisão na **posição final** de cada passo. Com `dt = 0.05` e queda a `MAX_FALL = 50`, o jogador anda **2,5 blocos num único passo**, podendo atravessar uma plataforma de 1 bloco de espessura. A 60 FPS (`dt ≈ 0.0167`) o passo é de 0,83 bloco, e nada acontece. A correção profissional é dividir o `dt` em *sub-passos* de no máximo, digamos, meio bloco de movimento. É o exercício de física do Capítulo 12.

## 9.5 Spawn, voo e o bug do include guard

```c
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
```

O jogador nasce no centro do mundo, 1 bloco acima do solo (`world_surface_height + 1`). A tecla `F` alterna `flying`: sem gravidade, com velocidade maior, mas **ainda com colisão**: você voa, mas não atravessa paredes (`noclip` seria só desligar `move_axis`).

> ⚠️ **Cuidado: o bug que o `-Wall` pegou.**
> Durante a escrita deste projeto, o header do jogador tinha o include guard `PLAYER_H` e, mais abaixo, uma constante também chamada `PLAYER_H` (a altura). O compilador avisou "`PLAYER_H` redefined". Ignorar o aviso teria sido um bug fantasma: o guard e a constante se atropelando. A correção foi renomear o guard para `PLAYER_HEADER_H` e as constantes para `PLAYER_WIDTH`/`PLAYER_HEIGHT`. Moral: nomes de guard bem específicos, e **nunca** desligue warnings.

## 9.6 Checkpoint: testar sem janela

Como `player.c` não toca em OpenGL, dá para testar a física no terminal. Cole na `main` (antes do `glfwInit`):

```c
World *w = world_create(1337);
Player p;
player_init(&p, w);
p.pos[1] += 10.0f;                         /* solta de 10 blocos acima */

PlayerInput in = {0};
for (int i = 0; i < 200; i++)              /* ~3.3 s a 60 FPS */
    player_update(&p, w, &in, 1.0f / 60.0f);

printf("y = %.3f  no_chao = %d  superficie = %d\n",
       p.pos[1], p.on_ground,
       world_surface_height(w, (int)p.pos[0], (int)p.pos[2]));
```

Esperado: `no_chao = 1` e `y` ligeiramente acima de `superficie + 1`. (Os pés ficam sobre o topo do bloco, que está em `superficie + 1`.)

Depois, substitua a `FreeCam` do Capítulo 4 pelo `Player`: no callback do mouse, escreva em `player.yaw/pitch`; no loop, monte um `PlayerInput` com `axis()` e chame `player_update`; use `player_eye` e `player_front` para a view. Você agora **anda**, **pula** e **escorrega** em paredes. Note que **não** sobe degraus de 1 bloco sem pular: não implementamos *step-up*.

> 🤔 **Para pensar.**
> **P9.1.** Por que movemos cada eixo separadamente, em vez de aplicar o deslocamento 3D inteiro e testar uma vez?
> **P9.2.** Em que situação exata o `dt` máximo de 0,05 e a queda máxima de 50 permitem que o jogador atravesse algo? Como você corrigiria?

\newpage

# Capítulo 10: Interação (raycast, quebrar e colocar blocos, HUD)

> **Checkpoint:** quebrar com o botão esquerdo, colocar com o direito, ver o contorno do bloco mirado e a mira.

## 10.1 O problema: "qual bloco estou olhando?"

O jogador mira para um ponto da tela; o jogo precisa descobrir **qual bloco** está nessa linha de visão, em até ~6 blocos. Lançamos um **raio** a partir do olho, na direção do olhar, e andamos por ele até bater em um bloco sólido.

A primeira ideia que vem à cabeça é *marchar* o raio em passos pequenos (de 0,1 em 0,1 bloco, digamos). Funciona, mas tem dois problemas: passos grandes **pulam cantos finos** de blocos; passos pequenos custam caro. Existe um algoritmo exato e barato.

## 10.2 O DDA voxel (Amanatides & Woo)

> 💡 **Curiosidade.** O algoritmo vem de um artigo de 1987, "A Fast Voxel Traversal Algorithm for Ray Tracing", de John Amanatides e Andrew Woo. Quase todo jogo de voxels o usa até hoje.

A ideia: um raio `p(t) = origem + t · direção` atravessa a grade **uma célula por vez**, sempre cruzando a fronteira de um dos três eixos. Basta saber, para cada eixo, **em que `t` a próxima fronteira será cruzada**, e avançar sempre no eixo cujo `t` é menor.

Para cada eixo `i`, com `celula[i] = floor(origem[i])`:

```
direcao[i] > 0:  proxima fronteira em celula+1 -->  tMax = (celula + 1 - origem) / direcao
direcao[i] < 0:  proxima fronteira em celula   -->  tMax = (origem - celula) / (-direcao)
direcao[i] = 0:  nunca cruza                   -->  tMax = infinito

tDelta[i] = 1 / |direcao[i]|    // quanto t avança para atravessar UMA célula inteira nesse eixo
```

O laço fica assim: olhe a célula atual; se for sólida, acabou. Senão, escolha o eixo de menor `tMax`, dê um passo naquela direção, some `tDelta` ao `tMax` daquele eixo. Como a direção é unitária, `t` é a distância percorrida, então `tMax > alcance` encerra a busca.

```c
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
```

Dois detalhes: guardamos **`prev`**, a última célula *vazia* antes de bater, porque é onde um bloco novo será colocado (na face que você está olhando). E o `tMax = INFINITY` para eixos paralelos ao raio evita divisão por zero e faz o eixo nunca ser escolhido.

## 10.3 Quebrar e colocar

Os cliques chegam por **eventos** (callbacks), então só ligam flags que o loop consome uma vez por frame:

```c
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
```

- **Quebrar:** escreve `AIR` na célula `hit.block`.
- **Colocar:** escreve o bloco do hotbar na célula `hit.prev`... **mas só se o jogador não estiver ocupando a célula**. Sem essa checagem, você poderia emparedar a si mesmo (ou enterrar-se dentro de um bloco), e a colisão deixaria o jogador preso. A checagem é um teste de sobreposição de AABBs:

```c
int player_overlaps_block(const Player *p, int x, int y, int z)
{
    float hw = PLAYER_WIDTH * 0.5f;
    return p->pos[0] + hw > x && p->pos[0] - hw < x + 1 &&
           p->pos[1] + PLAYER_HEIGHT > y && p->pos[1] < y + 1 &&
           p->pos[2] + hw > z && p->pos[2] - hw < z + 1;
}
```

Note os `>` e `<` **estritos**: encostar numa face não conta como sobrepor, então você pode colocar um bloco bem rente aos seus pés.

## 10.4 Contorno do bloco mirado

Já construímos o cubo em arame no Capítulo 3. Agora o usamos com um `model` que é só uma translação até o bloco, e desenhamos **depois** do mundo, com teste de profundidade ligado (linhas atrás de outros blocos não aparecem). O `1.004f` escala o cubo 0,4% além do bloco: sem isso, as linhas ficariam exatamente no mesmo plano das faces do bloco e o depth buffer, com precisão finita, alternaria entre as duas a cada pixel (***z-fighting***, aquela cintilação).

> ⚠️ **Cuidado: `glLineWidth`.**
> No perfil *core*, larguras de linha maiores que 1 **não são garantidas** (e em contextos *forward-compatible*, como o do macOS, geram erro). Por isso não chamamos `glLineWidth` e aceitamos linhas de 1 pixel. Para linhas grossas de verdade, você desenharia *quads* finos.

## 10.5 A mira

```c
static GLuint make_crosshair_vao(void)
{
    const float s = 0.03f;
    const float v[] = { -s,0,0,  s,0,0,   0,-s,0,  0,s,0 };
    return make_vao(v, 12);
}
```

A mira é um "+" feito de 4 vértices em **coordenadas normalizadas de tela (NDC)**, de -1 a 1, sem matriz de view nem de projeção. Como a tela é mais larga que alta, `0.03` em X e `0.03` em Y teriam tamanhos diferentes em pixels; corrigimos escalando X por `1 / aspect` na matriz do HUD (`hud[0][0] = 1.0f / aspect`), o que deixa a cruz com braços do mesmo tamanho em pixels.

Desenhamos tudo isto junto, no fim do frame:

```c
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
```

A mira é desenhada **por último** e com `glDisable(GL_DEPTH_TEST)`: ela é uma sobreposição de tela, deve aparecer por cima de qualquer coisa, sem importar o que está a que profundidade.

> 🤔 **Para pensar.**
> **P10.1.** Por que o raycast devolve `prev` além de `block`? Que mecânica quebraria sem ele?
> **P10.2.** Por que o contorno é escalado em `1.004` e não `1.0`?

\newpage

# Capítulo 11: Costurando tudo (a `main`)

> **Checkpoint:** a versão final do jogo.

Todos os módulos existem; falta o maestro. Vamos ler a `main.c` final na ordem em que as coisas acontecem.

## 11.1 Constantes e tabelas

@@src/main.c::#define START_W::<                /* Estado compartilhado@@

`SKY` é a cor do céu **e** da névoa. Os dois arrays do hotbar são paralelos: `HOTBAR[i]` é o bloco e `HOTBAR_NAME[i]` seu nome para o título da janela. (Dois arrays paralelos são frágeis; uma `struct { BlockType type; const char *name; }` seria mais robusta. Deixamos paralelos pela leitura rápida.)

## 11.2 O estado compartilhado

```c
/* Estado compartilhado com os callbacks (via glfwSetWindowUserPointer) */
typedef struct {
    Player player;
    int    selected;
    int    fb_w, fb_h;
    double last_x, last_y;
    int    first_mouse;
    int    click_break, click_place;   /* viram 1 no clique, consumidos no loop */
} App;
```

Callbacks do GLFW são funções C puras: não existe *closure* nem `this`. Como dar a elas acesso ao jogador? Duas opções: variáveis globais, ou o **ponteiro de usuário da janela** (`glfwSetWindowUserPointer`). Preferimos a segunda: o estado fica agrupado numa `struct`, os callbacks o recuperam com `glfwGetWindowUserPointer`, e nada fica "solto" no escopo global.

## 11.3 Eventos × polling

```c
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
```

Há duas formas de ler entrada, e usamos **as duas**, cada uma onde faz sentido:

- **Eventos (callbacks):** para coisas *discretas* que acontecem uma vez: um clique, apertar `F`, trocar de bloco, mover o mouse. Se você usasse *polling* para o clique, um clique rápido entre dois frames seria perdido.
- **Polling (`glfwGetKey` no loop):** para estados *contínuos*: WASD segurado. Um callback só dispararia no instante em que a tecla é apertada, e a repetição de teclado do sistema é irregular.

## 11.4 A função principal

```c
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
```

### A ordem de inicialização

`glfwInit` → *hints* → janela → contexto corrente → GLAD → callbacks → estado do GL → shaders e texturas → mundo → jogador → localização dos uniforms. Cada passo depende do anterior; chamar `glGenTextures` antes do GLAD carregar é um crash garantido.

Algumas linhas pedem atenção:

- **`static App app;`:** `static` zera a struct (e faz o `first_mouse = 1` ser a única exceção que precisamos atribuir), vive durante todo o programa, e tem endereço estável, requisito do *user pointer*.
- **`glfwGetFramebufferSize`:** como vimos no Capítulo 2, lemos o tamanho **real em pixels**.
- **`world_update_meshes(world, WORLD_CX * WORLD_CZ)`:** no arranque, remontamos tudo de uma vez, sem orçamento, para o primeiro frame já ter o mundo inteiro.

### A ordem dentro de cada frame

```
1. tempo (dt)                                 <- sem isso, nada anda direito
2. glfwPollEvents                             <- dispara os callbacks (mouse, cliques)
3. ler teclas seguradas -> PlayerInput
4. player_update (física)                     <- move o jogador
5. raycast a partir do olho NOVO              <- qual bloco estou olhando AGORA?
6. aplicar cliques (quebrar / colocar)
7. world_update_meshes (4 por frame)          <- as edições aparecem NO MESMO frame
8. matrizes (proj, view, vp)
9. desenhar: céu -> mundo -> contorno -> mira
10. swap buffers, FPS no título
```

Cada ordem tem um motivo:

- O **raycast vem depois da física** para usar a posição de olho atualizada; senão a mira ficaria um frame atrasada.
- O **update dos meshes vem depois das edições e antes do desenho**, então quebrar um bloco aparece no mesmo frame, sem latência visível.
- Desenhamos **opacos primeiro**, depois o contorno (usa o depth buffer do mundo), por fim o HUD sem teste de profundidade.

### Os uniforms e as `locations`

Os `glGetUniformLocation` ficam **antes do loop**. Já mencionamos no Capítulo 8: buscas por string dentro do loop são desperdício. Dentro do loop só enviamos valores.

### O título como mini-HUD

A cada 0,5 s calculamos `frames / fps_timer` e escrevemos FPS, bloco selecionado e modo (andando/voando) no título da janela. É um HUD gratuito: renderizar texto de verdade exigiria fontes e outro shader.

### Limpeza

Destruímos o mundo (que libera VAOs/VBOs), a textura, os programas e a janela. Em um programa pequeno o sistema operacional faria isso por nós ao fechar, mas **vazamentos de GPU** só somem quando o processo morre, e o hábito de limpar na ordem inversa da criação evita dor de cabeça em programas maiores.

> 🤔 **Para pensar.**
> **P11.1.** Por que `static App app` e um *user pointer*, em vez de variáveis globais soltas?
> **P11.2.** Por que desenhamos a mira por último, e sem teste de profundidade?

\newpage

# Capítulo 12: Depuração, desempenho e próximos passos

## 12.1 Guia de sobrevivência: "tela preta" e amigos

Programação gráfica falha em silêncio. Quando nada aparece, siga esta lista **em ordem**:

| Sintoma | Verifique |
|---|---|
| Tela preta, nada desenhado | O `shader_create` retornou 0? O log do GLSL apareceu no terminal? |
| Tela preta, shaders OK | `uVP`/`uMVP` com `location == -1` (nome com erro de digitação, ou uniform "otimizado" para fora porque não é usado) |
| Objeto some ao girar a câmera | `near`/`far`, ou `NaN` na view (pitch em ±90°) |
| Faces faltando | *Winding* da face, ou `GL_CULL_FACE` ligado sem `glFrontFace(GL_CCW)` |
| Tudo "dentro para fora" | *Winding* invertido em todas as faces |
| Cena distorcida | `GL_TRUE` no `transpose`, ou ordem errada de `P * V * M` |
| Mouse "pula" no início | `first_mouse` não tratado |
| Mouse não gira | Cursor não está em `GLFW_CURSOR_DISABLED`, ou callback não registrado |
| Linhas entre blocos | `UV_EPS`, filtro `LINEAR`, mipmaps no atlas |
| Cintilação em superfícies | *z-fighting* (coplanares ou `near` pequeno demais) |
| Jogador treme no chão | `dt` descontrolado, ou `on_ground` mal calculado |
| Crash ao iniciar no Windows | Estruturas grandes na pilha (`World` por valor) |

Três ferramentas valem ouro:

1. **`glGetError()`** logo depois de uma chamada suspeita (ou, no 4.3+, `glDebugMessageCallback`; no nosso 3.3 vale uma função `check_gl()` que você salpica).
2. **Imprimir valores**: matrizes, `eye`, `front`, `hit.block`. Quase todo bug de câmera some quando você vê o número.
3. **Um depurador gráfico**: RenderDoc captura um frame e deixa inspecionar buffers, texturas e uniforms. Para um bug de renderização, é imbatível.

## 12.2 Um modelo mental de desempenho

Antes de otimizar, meça (o FPS no título já ajuda). O custo se divide em três regiões:

- **CPU, atualização:** física e raycast são desprezíveis neste projeto.
- **CPU, reconstrução de meshes:** acontece só quando se edita, e limitada por `budget`. O pior caso é o arranque.
- **GPU, desenho:** proporcional ao número de **vértices** e de **fragmentos**. As faces escondidas já foram eliminadas; o que sobra é a casca visível.

Os gargalos prováveis, em ordem: (1) desenhar chunks atrás de você; (2) uso de 24 bytes por vértice; (3) 64 *draw calls*. Nenhum é problema agora; todos se tornam problema em mundos maiores.

## 12.3 Próximos passos, em ordem de dificuldade

1. **EBO (fácil).** Troque 6 vértices por face por 4 + 6 índices (Capítulo 7). Dica: o padrão de índices é `{0,1,2, 0,2,3}` somado a `4 * número_da_face`. Use `uint32_t` primeiro; depois estude se `uint16_t` basta.

2. **Frustum culling (fácil/médio).** Não desenhe chunks fora da pirâmide de visão. A ideia é extrair os 6 planos do frustum de `vp` (técnica de Gribb e Hartmann, 2001) e testar a AABB do chunk contra eles. A cglm tem funções prontas para isso (`glm_frustum_planes`, `glm_aabb_frustum`); confira a documentação da sua versão.

3. **Sub-passos de física (fácil).** Divida o `dt` em passos de no máximo, digamos, 1/120 s, e rode `player_update` em cada. Corrige o tunelamento do Capítulo 9.

4. **Step-up (médio).** Ao bater horizontalmente, teste se subir 1 bloco libera o caminho; se sim, suba. Dá fluidez ao andar em terreno irregular.

5. **Oclusão ambiente por vértice (médio).** Escureça cantos onde blocos se encontram. Para cada vértice, olhe os 2 blocos laterais e o diagonal que o tocam: se os dois laterais forem sólidos, `ao = 0`; senão `ao = 3 - (lateral1 + lateral2 + diagonal)` (cada termo é 0 ou 1). Multiplique o brilho por `ao / 3`. Detalhe: alterne a diagonal que divide o quad em triângulos, para evitar artefatos de interpolação.

6. **Água (médio).** Um segundo desenho, depois dos opacos, com *blending* (`GL_BLEND`) e `glDepthMask(GL_FALSE)`. Exige separar o mesh da água do mesh sólido.

7. **Salvar e carregar (médio).** Como o terreno é determinístico pela `seed`, **salve só as diferenças** (blocos que o jogador modificou), não o mundo inteiro. Para guardar chunks completos, compressão por repetição (RLE) funciona muito bem em grades cheias de ar e pedra.

8. **Mesh em outra thread (difícil).** Gere os vértices em uma thread de trabalho e envie para a GPU na thread principal, a única com o contexto GL. Exige um buffer por thread e fila de resultados.

9. **Mundo infinito (difícil).** Troque o array fixo por um mapa de chunks indexado por `(cx, cz)`, carregue/descarregue ao redor do jogador, e use divisão com piso para coordenadas negativas (o aviso do Capítulo 5).

10. **Greedy meshing (difícil).** Funda faces vizinhas coplanares do mesmo tipo em retângulos maiores, reduzindo drasticamente os triângulos. A referência clássica é o artigo de Mikola Lysenko no blog 0fps. Atenção: com atlas, textura repetida em retângulos grandes exige cuidado (ou uma *texture array*).

11. **Iluminação de verdade (difícil).** Luz por *flood fill* no estilo Minecraft: cada bloco guarda um nível de luz (0-15), propagado vizinho a vizinho, e o mesh usa esse valor no lugar do brilho fixo por face.

## 12.4 Uma última palavra

O que você construiu não é só "um clone": cada módulo é um **padrão que aparece em quase todo jogo 3D**: separar dados de apresentação (chunks e meshes), marcar como sujo e reconstruir sob orçamento, mover eixo por eixo, resolver visibilidade com um algoritmo exato. Os nomes mudam de engine para engine; as ideias não.

Se um dia você voltar a olhar para a matriz de view e achar que ela é só "aquela coisa que eu mexo para a câmera funcionar", lembre do Capítulo 4: **ela é o mundo inteiro sendo puxado para a frente dos seus olhos**.

Boa construção. 🧱

\newpage

# Apêndice A: Glossário

**AABB.** *Axis-Aligned Bounding Box*: caixa alinhada aos eixos, definida por um mínimo e um máximo.

**Atlas.** Uma textura que contém vários tiles lado a lado.

**Backface culling.** Descarte de triângulos virados "de costas" para a câmera, decidido pela ordem (*winding*) dos vértices na tela.

**Chunk.** Bloco de terreno (aqui 16 × 64 × 16), unidade de geração e de mesh.

**Clip space.** Espaço depois da projeção, antes da divisão por `w`. A GPU recorta aqui.

**DDA.** *Digital Differential Analyzer*: algoritmo que percorre uma grade célula a célula ao longo de uma reta.

**Dirty flag.** Marca "mudou; precisa recalcular". Evita trabalho repetido.

**fBm.** *Fractional Brownian Motion*: soma de oitavas de ruído com frequência dobrando e amplitude caindo pela metade.

**Frustum.** Tronco de pirâmide que representa o volume visível da câmera.

**NDC.** *Normalized Device Coordinates*: cubo `[-1, 1]³` depois da divisão por `w`.

**Pitch / Yaw / Roll.** Rotação em torno dos eixos lateral (cima/baixo), vertical (esquerda/direita) e frontal (inclinar a cabeça). Não usamos o roll.

**Voxel.** "Pixel 3D": uma célula de uma grade 3D.

**Winding.** Sentido (horário ou anti-horário) em que os vértices de um triângulo aparecem na tela.

**Z-fighting.** Cintilação causada por duas superfícies quase na mesma profundidade.

\newpage

# Apêndice B: Matrizes e cglm

## B.1 Matriz de perspectiva

Com `f = 1 / tan(fovy / 2)`:

```
| f/aspect   0        0                      0                    |
|    0       f        0                      0                    |
|    0       0   (far+near)/(near-far)   2·far·near/(near-far)    |
|    0       0       -1                      0                    |
```

A linha `-1` copia `-z` para `w`: depois da multiplicação, `w = -z_olho`. A GPU então divide `x`, `y` e `z` por `w`, e é essa divisão que faz objetos distantes ficarem menores (**perspectiva**) e que torna o depth buffer não linear em `z`.

## B.2 Convenção de índices na cglm

`mat4` é `float[4][4]` com a convenção `m[coluna][linha]`:

```
m[0][0]  m[1][0]  m[2][0]  m[3][0]        eixo X   eixo Y   eixo Z   translação
m[0][1]  m[1][1]  m[2][1]  m[3][1]   =      (colunas 0..2 = base)    (coluna 3)
m[0][2]  m[1][2]  m[2][2]  m[3][2]
m[0][3]  m[1][3]  m[2][3]  m[3][3]
```

Exemplo do projeto: `hud[0][0] = 1.0f / aspect` altera a escala em X.

## B.3 Funções da cglm usadas neste livro

| Função | O que faz |
|---|---|
| `glm_rad(g)` | graus para radianos |
| `glm_perspective(fovy, aspect, near, far, dest)` | matriz de projeção |
| `glm_lookat(olho, alvo, cima, dest)` | matriz de view |
| `glm_mat4_mul(a, b, dest)` | `dest = a * b` |
| `glm_mat4_identity(m)` | identidade |
| `glm_translate_make(m, v)` | matriz de translação |
| `glm_vec3_add(a, b, dest)` | soma |
| `glm_vec3_copy(a, dest)` / `glm_vec3_zero(v)` | copiar / zerar |
| `glm_vec3_muladds(a, s, dest)` | `dest += a * s` |
| `glm_vec3_norm(v)` / `glm_vec3_normalize(v)` | comprimento / normalizar no lugar |
| `glm_ivec3_copy(a, dest)` | copiar vetor de inteiros |

\newpage

# Apêndice C: Respostas comentadas

Tente responder antes de ler. Aqui estão as respostas, de forma curta.

**P1.1.** `glfwGetProcAddress` consulta o driver do contexto corrente; sem contexto corrente não há de onde obter os endereços das funções.

**P1.2.** O `glad.c` contém um carregador próprio (usado por `gladLoadGL`) que chama `dlopen`/`dlsym` para achar a `libGL`. Mesmo sem usá-lo, o arquivo é compilado inteiro e os símbolos precisam ser resolvidos. Em versões recentes da glibc as funções `dl*` foram movidas para a libc, então o `-ldl` pode virar um no-op, mas mantê-lo é portátil.

**P2.1.** Com `pos += 0.1f` por frame, a velocidade em unidades/segundo é proporcional ao FPS: cada máquina anda numa velocidade diferente. Com `5.0f * dt`, a velocidade é sempre 5 unidades por segundo.

**P2.2.** A janela usa unidades do sistema (pontos); o framebuffer, pixels reais. Em telas HiDPI diferem. O `glViewport` trabalha com os pixels do framebuffer.

**P3.1.** `V * P` aplicaria a projeção primeiro e depois a view sobre coordenadas já projetadas, o que não faz sentido geométrico: a cena aparece distorcida ou some. A ordem dos produtos é a ordem (da direita para a esquerda) em que as transformações são aplicadas.

**P3.2.** O *program* guarda o código linkado; os *shader objects* são só matéria-prima intermediária.

**P4.1.** Em ±90° a `frente` fica paralela ao `cima` do mundo; o produto vetorial do `lookAt` zera e a normalização produz `NaN`.

**P4.2.** O delta do mouse é um **deslocamento** (já embute o tempo entre eventos); o WASD define uma **velocidade**, que precisa ser multiplicada pelo tempo decorrido.

**P4.3.** Porque olhar para o chão não deve frear nem enterrar o personagem. A direção 3D completa faz sentido em câmeras de voo livre (*spectator*), natação e naves.

**P5.1.** Valem `0` e `-1`. Sem o teste, `blocks[-1]` lê/escreve fora do array: memória corrompida ou lixo.

**P5.2.** 16 × 256 × 16 = 65.536 bytes = 64 KiB por chunk. Uma alternativa é fatiar em seções de 16 × 16 × 16 (4 KiB), alocando só as que não estão vazias.

**P6.1.** Com `0.15`, a oitava mais grossa tem comprimento de onda de ~7 blocos: terreno serrilhado, sem colinas largas. Com `0.0015`, ~667 blocos, mais que o mundo inteiro: relevo quase plano ou uma única rampa suave.

**P6.2.** A copa cruzaria a fronteira, exigindo escrever em um chunk vizinho que pode não existir ainda ou já ter sido meshado. Soluções: margem (a nossa), uma passada de decoração depois da geração, ou cada chunk calcular determinísticamente as árvores dos vizinhos e escrever só a parte que lhe cabe.

**P7.1.** A face sumiria vista de fora (descartada pelo culling) e apareceria vista de dentro: um "buraco" no bloco.

**P7.2.** É iluminação falsa: assume um sol alto, então o topo é claro e o fundo escuro. Os valores são parecidos com os que o próprio Minecraft usa para dar volume aos blocos.

**P8.1.** `GL_NEAREST` dá o visual pixelado e evita borrar e misturar tiles vizinhos. Mipmaps em atlas misturam tiles adjacentes nos níveis menores; *texture arrays* resolvem isso.

**P8.2.** Para os blocos distantes se dissolverem sem emenda no fundo. Se fossem diferentes, os blocos distantes ficariam com uma cor diferente do céu e a borda do mundo ficaria visível como uma silhueta.

**P9.1.** Movendo por eixo, o componente que bate é cancelado e o paralelo à parede continua, então você desliza. Aplicando o deslocamento 3D inteiro, uma colisão cancelaria o movimento todo e você grudaria nas paredes.

**P9.2.** Com `dt = 0.05` e queda a 50, o jogador desloca 2,5 blocos em um passo; só testamos a posição final, então uma plataforma de 1 bloco de espessura pode ser atravessada. Correção: dividir o passo em sub-passos menores.

**P10.1.** `prev` é a célula vazia imediatamente antes do bloco atingido, ou seja, onde o novo bloco deve ser colocado (na face mirada). Sem ela, não dá para colocar blocos.

**P10.2.** Para evitar *z-fighting*: linhas exatamente no mesmo plano das faces do bloco competem no depth buffer e cintilam. Empurrá-las 0,002 para fora resolve.

**P11.1.** `static` zera a struct, dá vida ao programa inteiro e um endereço estável para o *user pointer*. Callbacks em C não têm *closure*; o ponteiro de usuário agrupa o estado sem poluir o escopo global.

**P11.2.** A mira é uma sobreposição de tela: precisa ficar por cima de tudo, independentemente de profundidade, então vai por último e sem depth test.
