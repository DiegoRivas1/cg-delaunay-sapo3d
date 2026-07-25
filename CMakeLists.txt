cmake_minimum_required(VERSION 3.20)
project(cg_delaunay_sapo3d LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ------------------------------------------------------------------
# Rutas compartidas (mismo patron que cg_oceano / cg_labs)
# ------------------------------------------------------------------
set(EXTERNAL_DIR "${CMAKE_CURRENT_LIST_DIR}/external" CACHE PATH "Ruta a external/")

# Copia aqui (o symlink) tu carpeta imgui-1.92.8 y glad ya usados en cg_oceano
set(IMGUI_DIR "${EXTERNAL_DIR}/imgui-1.92.8")
set(GLAD_DIR  "${EXTERNAL_DIR}/glad")

# GLM viene instalado via MSYS2 (mingw-w64-ucrt-x86_64-glm), se resuelve
# via find_package normal, sin FetchContent ni vendorizar nada.
find_package(glm CONFIG REQUIRED)

# ------------------------------------------------------------------
# Nucleo del algoritmo de Delaunay (sin dependencias graficas,
# testeable de forma aislada, no toca GLAD/ImGui/GLFW para nada).
# ------------------------------------------------------------------
add_library(delaunay_core STATIC
    src/delaunay/Delaunay3D.cpp
    src/delaunay/PointCloudIO.cpp
)
target_include_directories(delaunay_core PUBLIC src)

# ------------------------------------------------------------------
# Ejecutable de CONSOLA (sin OpenGL): valida el algoritmo puro y
# exporta .obj de la superficie reconstruida. Util para iterar rapido
# sin GPU/ventana.
# ------------------------------------------------------------------
add_executable(delaunay_core_test tests/test_main.cpp)
target_link_libraries(delaunay_core_test PRIVATE delaunay_core)

# ------------------------------------------------------------------
# Ejecutable: test sintetico (esfera / cubo aleatorio -> valida el
# algoritmo con render OpenGL + ImGui)
# ------------------------------------------------------------------
add_executable(delaunay_test
    apps/synthetic_test/main.cpp
    src/core/Application.cpp
    src/core/Shader.cpp
    ${GLAD_DIR}/src/glad.c
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/imgui_demo.cpp
    ${IMGUI_DIR}/backends/imgui_impl_glfw.cpp
    ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
)
target_include_directories(delaunay_test PRIVATE
    ${GLAD_DIR}/include
    ${IMGUI_DIR}
    ${IMGUI_DIR}/backends
    src
)
target_link_libraries(delaunay_test PRIVATE
    delaunay_core
    glm::glm
    glfw3
    opengl32
    gdi32
    glu32
)

# Copia los shaders junto al ejecutable en cada build.
# OJO: 'data/' NO se copia aca a proposito -- va a tener los 17 TIFFs
# completos (~500MB en total) y copiarlos en cada build seria muy lento.
# En vez de eso, configura el "Working directory" del run config en
# CLion para que apunte a la raiz del proyecto (${CMAKE_SOURCE_DIR}),
# asi las rutas relativas como "data/spleen_points.xyz" funcionan sin
# copiar nada pesado.
add_custom_command(TARGET delaunay_test POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_LIST_DIR}/shaders" "$<TARGET_FILE_DIR:delaunay_test>/shaders"
)

# ------------------------------------------------------------------
# Placeholder: ejecutable del sapo (Unidad 4). Mismo patron que
# delaunay_test, se agrega despues de validar la extraccion de
# superficie desde los TIFF.
# ------------------------------------------------------------------
# add_executable(sapo3d
#     apps/sapo3d/main.cpp
#     src/core/Application.cpp
#     src/core/Shader.cpp
#     ${GLAD_DIR}/src/glad.c
#     ${IMGUI_DIR}/imgui.cpp
#     ${IMGUI_DIR}/imgui_draw.cpp
#     ${IMGUI_DIR}/imgui_tables.cpp
#     ${IMGUI_DIR}/imgui_widgets.cpp
#     ${IMGUI_DIR}/imgui_demo.cpp
#     ${IMGUI_DIR}/backends/imgui_impl_glfw.cpp
#     ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
# )
# target_include_directories(sapo3d PRIVATE ${GLAD_DIR}/include ${IMGUI_DIR} ${IMGUI_DIR}/backends src)
# target_link_libraries(sapo3d PRIVATE delaunay_core glm::glm glfw3 opengl32 gdi32 glu32)
