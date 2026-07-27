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

# OpenMP: paraleliza la clasificacion de tetraedros dentro de
# Bowyer-Watson (el paso mas caro del algoritmo). Opcional -- si no esta
# disponible, el codigo compila igual y corre secuencial (los #pragma
# omp se ignoran).
find_package(OpenMP)

# ------------------------------------------------------------------
# Nucleo del algoritmo de Delaunay (sin dependencias graficas,
# testeable de forma aislada, no toca GLAD/ImGui/GLFW para nada).
# ------------------------------------------------------------------
add_library(delaunay_core STATIC
    src/delaunay/Delaunay3D.cpp
    src/delaunay/PointCloudIO.cpp
    src/delaunay/ManifestIO.cpp
    src/delaunay/TetraCache.cpp
)
target_include_directories(delaunay_core PUBLIC src)
if(OpenMP_CXX_FOUND)
    target_link_libraries(delaunay_core PUBLIC OpenMP::OpenMP_CXX)
    message(STATUS "OpenMP encontrado: Bowyer-Watson corre en paralelo")
else()
    message(STATUS "OpenMP NO encontrado: Bowyer-Watson corre secuencial (sigue funcionando igual)")
endif()

# ------------------------------------------------------------------
# Ejecutable de CONSOLA (sin OpenGL): valida el algoritmo puro y
# exporta .obj de la superficie reconstruida. Util para iterar rapido
# sin GPU/ventana.
# ------------------------------------------------------------------
add_executable(delaunay_core_test tests/test_main.cpp)
target_link_libraries(delaunay_core_test PRIVATE delaunay_core)

# Benchmark de consola: mide triangulate()/boundaryFaces() para varios
# tamanos de nube de puntos, con o sin OpenMP. Sirve para llenar la
# tabla de tiempos del informe.
add_executable(delaunay_benchmark tests/bench_main.cpp)
target_link_libraries(delaunay_benchmark PRIVATE delaunay_core)

# ------------------------------------------------------------------
# Capa grafica compartida (ventana, shaders, camara, construccion de
# mallas GPU, GLAD, ImGui). Se compila UNA sola vez y la reusan todos
# los ejecutables graficos (delaunay_test, sapo3d) -- evita recompilar
# ImGui/GLAD por cada uno.
# ------------------------------------------------------------------
add_library(app_gfx_core STATIC
    src/core/Application.cpp
    src/core/Shader.cpp
    src/core/MeshBuilder.cpp
    ${GLAD_DIR}/src/glad.c
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/imgui_demo.cpp
    ${IMGUI_DIR}/backends/imgui_impl_glfw.cpp
    ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
)
target_include_directories(app_gfx_core PUBLIC
    ${GLAD_DIR}/include
    ${IMGUI_DIR}
    ${IMGUI_DIR}/backends
    src
)
target_link_libraries(app_gfx_core PUBLIC
    delaunay_core
    glm::glm
    glfw3
    opengl32
    gdi32
    glu32
)

# ------------------------------------------------------------------
# Ejecutable: test sintetico (esfera / cubo aleatorio / toro / .xyz
# cargado a mano -> valida el algoritmo con render OpenGL + ImGui)
# ------------------------------------------------------------------
add_executable(delaunay_test apps/synthetic_test/main.cpp)
target_link_libraries(delaunay_test PRIVATE app_gfx_core)

add_custom_command(TARGET delaunay_test POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_LIST_DIR}/shaders" "$<TARGET_FILE_DIR:delaunay_test>/shaders"
)

# ------------------------------------------------------------------
# Ejecutable: reconstruccion del sapo completo. Lee data/manifest.tsv
# (generado por scripts/extract_all.py) y muestra un selector de
# organos en ImGui.
# ------------------------------------------------------------------
add_executable(sapo3d apps/sapo3d/main.cpp)
target_link_libraries(sapo3d PRIVATE app_gfx_core)

add_custom_command(TARGET sapo3d POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_LIST_DIR}/shaders" "$<TARGET_FILE_DIR:sapo3d>/shaders"
)

# ------------------------------------------------------------------
# Ejecutable: reconstruccion del sapo con CACHE binario. Primera vez
# que se activa un organo, triangula y guarda el resultado en
# data/cache/<organo>_<numPuntos>.bin; las siguientes veces (incluso
# en otra corrida del programa) lo carga directo del cache, sin volver
# a correr Bowyer-Watson. Incluye edicion de colores por organo.
# ------------------------------------------------------------------
add_executable(sapo3d_hd apps/sapo3d_hd/main.cpp)
target_link_libraries(sapo3d_hd PRIVATE app_gfx_core)

add_custom_command(TARGET sapo3d_hd POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_LIST_DIR}/shaders" "$<TARGET_FILE_DIR:sapo3d_hd>/shaders"
)

# ------------------------------------------------------------------
# Ejecutable: reconstruccion con alpha ADAPTATIVO (por tetraedro, segun
# densidad local de sus 4 vertices) en vez de un unico alpha global por
# organo. Reusa el MISMO cache binario que sapo3d_hd -- no re-triangula
# nada que ya este calculado, solo cambia el criterio de filtrado final.
# ------------------------------------------------------------------
add_executable(sapo3d_adaptive apps/sapo3d_adaptive/main.cpp)
target_link_libraries(sapo3d_adaptive PRIVATE app_gfx_core)

add_custom_command(TARGET sapo3d_adaptive POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_LIST_DIR}/shaders" "$<TARGET_FILE_DIR:sapo3d_adaptive>/shaders"
)
