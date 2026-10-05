# MiniCraft — referência de mini Minecraft em C (OpenGL 3.3 core)

Libs: **GLFW** (janela/input), **GLAD 1.x** (loader GL, já gerado em `third_party/glad`), **cglm** (matemática).
Sem arquivos de imagem: o atlas de texturas é gerado por código.

## Compilar
    cmake -S . -B build
    cmake --build build
    ./build/minicraft            # (build/Release/minicraft.exe no Windows/MSVC)

Se GLFW/cglm não estiverem instaladas, o CMake baixa e compila (precisa de git + internet).
Linux: `sudo apt install build-essential cmake libglfw3-dev libcglm-dev`
macOS: `brew install cmake glfw cglm`

## Controles
WASD andar · Espaço pular/subir · Shift descer (voando) · F alterna voo
Mouse olhar · Botão esquerdo quebra · Direito coloca · 1–7 ou scroll troca de bloco · Esc sai

## Mapa do código
| Arquivo | O que ensina |
|---|---|
| `main.c` | janela, callbacks, loop principal, matrizes, ordem de desenho, HUD |
| `world.c` | chunks, geração de terreno, **mesh com face culling**, **raycast DDA** |
| `player.c` | física: gravidade, pulo, **colisão AABB eixo a eixo** |
| `noise.c` | hash + value noise + fBm (relevo) |
| `texture.c` | atlas procedural 16x16 por tile, filtro NEAREST |
| `shader.c` | compilação/link de shaders (GLSL embutido) |
| `blocks.h` | tipos de bloco e qual tile cada face usa |

## Decisões de projeto (e por quê)
- **Vértice = 6 floats** `x y z u v shade`, em coordenadas de mundo → um único `uVP` para todos os chunks.
- **Sem index buffer** (6 vértices por face): mais simples de ler. Trocar por 4 + índices é um bom exercício.
- **Face culling**: só gera a face se o vizinho for ar; ao mudar bloco na borda, o chunk vizinho é remontado.
- **Winding CCW** + `GL_CULL_FACE`: a ordem dos cantos em `FACES[]` (world.c) foi verificada com produto vetorial.
- **Iluminação fake**: brilho fixo por direção da face (topo 1.0, baixo 0.5, lados 0.8/0.65).
- **Neblina** no fragment shader esconde o limite do mundo.

## Limitações conhecidas (de propósito, para manter o código curto)
Mundo fixo 128x128 (8x8 chunks), sem água, sem salvar/carregar, sem frustum culling, sem step-up de degrau
(para subir 1 bloco é preciso pular), remontagem de mesh na thread principal, folhas opacas, árvores não cruzam chunks.

## Exercícios sugeridos
1. Frustum culling por chunk · 2. Index buffer · 3. Ambient occlusion por vértice
4. Água (segundo pass com blending) · 5. Chunks infinitos com carga/descarga e threads · 6. Salvar o mundo em disco
