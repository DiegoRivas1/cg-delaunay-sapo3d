cmake_minimum_required(VERSION 4.3)
project(cg-delaunay-sapo3d LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ---------------------------------------------------------------------------
# Dependencias descargadas automaticamente (GLFW, GLM, ImGui)
# ---------------------------------------------------------------------------
include(FetchContent)

FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG        3.4
)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(glfw)

FetchContent_Declare(
    glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG        1.0.1
)
FetchContent_MakeAvailable(glm)

FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG        v1.90.9
)
FetchContent_MakeAvailable(imgui)

# ImGui no trae CMakeLists propio: se arma como libreria manualmente.
add_library(imgui STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/imgui_demo.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
)
target_include_directories(imgui PUBLIC
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
)
target_link_libraries(imgui PUBLIC glfw)

# ---------------------------------------------------------------------------
# GLAD (NO se descarga automaticamente: copialo de tu proyecto anterior,
# p.ej. cg-oceano-dinamico/external/glad, o genera uno nuevo en
# https://glad.dav1d.de con API "gl" version 3.3, profile "core",
# y con "Generate a loader" activado. Debe quedar asi:
#
#   external/glad/include/glad/glad.h
#   external/glad/include/KHR/khrplatform.h
#   external/glad/src/glad.c
# ---------------------------------------------------------------------------
add_library(glad STATIC external/glad/src/glad.c)
target_include_directories(glad PUBLIC external/glad/include)

# ---------------------------------------------------------------------------
# Nucleo del algoritmo de Delaunay (sin dependencias graficas, testeable
# de forma aislada).
# ---------------------------------------------------------------------------
add_library(delaunay_core STATIC
        src/delaunay/Delaunay3D.cpp
)
target_include_directories(delaunay_core PUBLIC src)

# ---------------------------------------------------------------------------
# Capa de aplicacion (ventana, shaders, camara) reutilizable por todos
# los ejecutables del proyecto (test sintetico, y luego el del sapo).
# ---------------------------------------------------------------------------
add_library(app_core STATIC
        src/core/Application.cpp
        src/core/Shader.cpp
)
target_include_directories(app_core PUBLIC src)
target_link_libraries(app_core PUBLIC glad glfw imgui glm::glm)

# ---------------------------------------------------------------------------
# Ejecutable de CONSOLA (sin OpenGL): valida el algoritmo puro y exporta
# .obj de la superficie reconstruida. Util para iterar rapido sin GPU/ventana.
# ---------------------------------------------------------------------------
add_executable(delaunay_core_test tests/test_main.cpp)
target_link_libraries(delaunay_core_test PRIVATE delaunay_core)

# ---------------------------------------------------------------------------
# Ejecutable: test sintetico (esfera / cubo aleatorio -> valida el algoritmo)
# ---------------------------------------------------------------------------
add_executable(delaunay_test apps/synthetic_test/main.cpp)
target_link_libraries(delaunay_test PRIVATE app_core delaunay_core)

# Copia los shaders junto al ejecutable para poder correrlo desde build/
add_custom_command(TARGET delaunay_test POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/shaders
        $<TARGET_FILE_DIR:delaunay_test>/shaders
)

# ---------------------------------------------------------------------------
# Placeholder: ejecutable del sapo (Unidad 4). Se agrega despues de validar
# el pipeline de extraccion de superficie desde los TIFF.
# ---------------------------------------------------------------------------
# add_executable(sapo3d apps/sapo3d/main.cpp)
# target_link_libraries(sapo3d PRIVATE app_core delaunay_core)
