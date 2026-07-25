# cg-delaunay-sapo3d

Reconstrucción 3D de órganos de rana a partir de máscaras TIFF, usando
triangulación de Delaunay (algoritmo Bowyer-Watson en 3D / tetraedros).
Trabajo de laboratorio — Computación Gráfica, UNSA-EPCC.

## Estado actual

- [x] Núcleo del algoritmo Bowyer-Watson 3D, validado con puntos sintéticos
      (esfera y cubo aleatorio) — 0 violaciones de la propiedad Delaunay,
      conteo de caras de frontera coincide con la fórmula de Euler.
- [x] Ejecutable de consola (`delaunay_core_test`) para iterar rápido sin GPU.
- [x] Ejecutable gráfico (`delaunay_test`) con render OpenGL + ImGui:
      cámara orbital, selector de nube de puntos, wireframe de tetraedros,
      validación en vivo.
- [ ] Extracción de superficie desde los TIFF reales (`data/`).
- [ ] Ejecutable `sapo3d` con selector de órganos vía ImGui.

## Estructura

```
src/delaunay/   -> algoritmo puro (Vec3, Tetrahedron, Face, Bowyer-Watson), sin dependencias graficas
src/core/       -> Application (ventana+ImGui+loop), Shader, Camera
apps/synthetic_test/ -> ejecutable de validacion con puntos sinteticos
tests/          -> test de consola (sin OpenGL)
shaders/        -> GLSL (basic.vert/frag para superficie, line.vert/frag para aristas)
external/glad/  -> [DEBES COPIARLO TU, ver abajo]
data/           -> TIFFs del sapo (no versionados, van en .gitignore aparte)
```

## Setup (Windows, MSYS2 UCRT64 / CLion)

### 1. GLAD (paso manual obligatorio)

CMake no puede descargar GLAD automáticamente porque necesita generarse
con un loader específico. Dos opciones:

**A) Reusar el que ya tienes** en `cg-oceano-dinamico` o `cg-labs`:
copia esas carpetas tal cual a `external/glad/` aquí (debe quedar
`external/glad/include/glad/glad.h`, `external/glad/include/KHR/khrplatform.h`,
`external/glad/src/glad.c`).

**B) Generar uno nuevo** en https://glad.dav1d.de:
- Language: C/C++
- API: gl = Version 3.3
- Profile: Core
- Generate a loader: sí
- Descargar el zip, copiar `include/` y `src/glad.c` a `external/glad/`.

### 2. Resto de dependencias (GLFW, GLM, ImGui)

Se descargan solas vía `FetchContent` la primera vez que corres `cmake`.
Necesitas conexión a internet en ese paso (una sola vez, CMake las cachea
en `build/_deps`).

### 3. Compilar

```powershell
cmake -B build -G "Ninja"        # o "MinGW Makefiles" segun tu setup
cmake --build build
```

### 4. Correr

```powershell
./build/delaunay_core_test.exe   # validacion pura, sin ventana
./build/delaunay_test.exe        # version grafica con ImGui
```

Los shaders se copian automáticamente junto al ejecutable en cada build.

## Notas del algoritmo

- `Vec3` usa `double` para precisión numérica en el cálculo de
  circunesferas (evita falsos positivos/negativos cerca del borde).
- El súper-tetraedro se arma con 4 vértices en configuración de
  "cubo alternado" (tetraedro regular), escalado a 5x la diagonal del
  bounding box de los puntos de entrada.
- `boundaryFaces()` extrae las caras que pertenecen a un solo tetraedro:
  para nubes de puntos convexas (como una esfera) esto ya da la
  superficie completa. Para los órganos reales (formas no convexas,
  con huecos y concavidades) esto **no alcanza** — habrá que filtrar
  tetraedros por algún criterio adicional (p.ej. tamaño máximo de arista,
  o partir de una superficie extraída previamente con marching cubes /
  contornos por slice en vez de triangular el volumen sólido completo).
  Esto se decide en la siguiente etapa, con los TIFF reales.
