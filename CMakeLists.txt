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
