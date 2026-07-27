# cg-delaunay-sapo3d

Reconstruccion 3D de organos de rana a partir de mascaras TIFF, usando
triangulacion de Delaunay (algoritmo Bowyer-Watson en 3D / tetraedros)
mas un filtrado alpha-shape para reconstruir superficies no convexas.

## Indice

- [Que se implemento](#que-se-implemento)
- [Arquitectura](#arquitectura)
- [Setup y compilacion](#setup-y-compilacion)
- [Ejecutables](#ejecutables)
- [Pipeline de datos (TIFF -> nube de puntos)](#pipeline-de-datos-tiff---nube-de-puntos)
- [Resultados de validacion](#resultados-de-validacion)
- [Complejidad y optimizaciones](#complejidad-y-optimizaciones)
- [Los 17 organos](#los-17-organos)
- [Limitaciones y dificultades](#limitaciones-y-dificultades)
- [Capturas](#capturas)

## Que se implemento

- **Algoritmo de Bowyer-Watson en 3D** (tetraedros), desde cero: insercion
  incremental, super-tetraedro inicial, deteccion de tetraedros invalidos
  via circunesfera, reconstruccion de la malla por cada punto insertado.
- **Alpha-shape**: filtrado post-triangulacion por radio de circunesfera,
  necesario para reconstruir superficies con concavidades (los organos
  reales no son convexos; un casco convexo directo tapa huecos y hunde
  detalles).
- **Pipeline completo de datos**: extraccion de la cascara de voxeles
  desde TIFF binarios, sub-muestreo uniforme por grilla 3D, calculo de
  alpha sugerido por estadistica de vecino mas cercano.
- **Visualizacion interactiva**: OpenGL 3.3 core + ImGui, camara orbital,
  selector de organos con color distinto por cada uno, ajuste de alpha
  en vivo.
- **Patron Strategy** (`IDelaunayStrategy`) para poder intercambiar la
  implementacion de triangulacion sin tocar el resto del codigo.

## Arquitectura

```
src/delaunay/       Nucleo del algoritmo, SIN dependencias graficas:
  Vec3.hpp            vector 3D minimo (double, para precision numerica)
  Delaunay3D.hpp/cpp   Bowyer-Watson 3D + alpha-shape + extraccion de superficie
  IDelaunayStrategy.hpp Strategy pattern sobre el algoritmo
  PointCloudIO.hpp/cpp  lector de nubes de puntos .xyz
  ManifestIO.hpp/cpp    lector de data/manifest.tsv
  TetraCache.hpp/cpp    cache binario de triangulaciones ya calculadas

src/core/           Capa grafica reutilizable:
  Application.h/cpp    ventana GLFW + contexto ImGui + loop principal
  Shader.h/cpp         carga/compila/linkea GLSL
  Camera.h             camara orbital (arrastrar + scroll)
  MeshBuilder.h/cpp     construccion de buffers GPU desde la triangulacion

apps/
  synthetic_test/main.cpp   validacion con esfera / cubo / toro / archivo .xyz
  sapo3d/main.cpp            reconstruccion completa, re-triangula cada corrida
  sapo3d_hd/main.cpp         igual, pero con cache binario (instantaneo la 2da vez)

tests/
  test_main.cpp        validacion de consola (sin OpenGL), exporta .obj

scripts/
  tiff_utils.py          funciones compartidas de extraccion (cascara, downsample, NN stats)
  manifest_utils.py      lectura/escritura del manifest, compartido entre los dos scripts de abajo
  extract_surface.py     extrae UN organo (target_points manual u automatico), actualiza el manifest
  extract_all.py         extrae los 17 de una, reescribe el manifest completo
  report_voxel_counts.py conteo de voxeles originales, solo referencia (no genera nada)

shaders/                GLSL (basic.vert/frag = superficie con Phong, line.vert/frag = aristas)
external/glad/           [no versionado tal cual, ver Setup]
external/imgui-1.92.8/  [no versionado tal cual, ver Setup]
data/                    TIFFs + .xyz generados + manifest (no versionado, pesa mucho)
```

## Setup y compilacion

Toolchain: Windows + MSYS2 UCRT64 + CLion.

1. Copiamos `external/glad` y `external/imgui-1.92.8`, nosostors ya lo t enemos en el repositorio 
2. Instala dependencias via pacman si no las tenes:
   ```
   pacman -S mingw-w64-ucrt-x86_64-glfw mingw-w64-ucrt-x86_64-glm
   ```
3. Reconfigura CMake en CLion y compila. Genera 6 targets:
   `delaunay_core_test`, `delaunay_test`, `sapo3d`, `sapo3d_hd`, `sapo3d_adaptive` (y la libreria interna
   `app_gfx_core`).
4. Para `sapo3d`, el *working directory* del run config debe apuntar a la
   raiz del repo (no a `cmake-build-debug/`), para que encuentre
   `data/manifest.tsv` y `shaders/` con rutas relativas.
5. Para `sapo3d_hd`, el *working directory* debe ser la raiz del repo y
   `data/cache/` debe existir (aunque este vacio) para que el programa
   pueda crear los archivos binarios de cache.
6. Para `sapo3d_adaptive` , el *working directory* debe ser la raiz del repo y
   `data/cache/` debe existir (aunque este vacio) para que el programa
   pueda crear los archivos binarios de cache.
7. Para `delaunay_test`, el *working directory* debe ser la raiz del repo para que encuentre `data/` y `shaders/`.

Ejemplo de configuracion de working directory en CLion (Windows):
![run_configuration](docs/run_configuration.png)
![working_directory](docs/directory_ejemplo.png)
## Ejecutables

### `delaunay_core_test`
Consola, sin ventana. Corre 3 casos (esfera, cubo aleatorio, toro) y
valida la propiedad de Delaunay + exporta `.obj` de cada superficie.
Sirve para iterar sobre el algoritmo sin esperar compilar OpenGL/ImGui.

### `delaunay_benchmark`
Consola, sin ventana. Triangula nubes sinteticas de 500/1500/3000/5000
puntos y mide `triangulate()`/`boundaryFaces()`. Imprime si se compilo
con OpenMP y cuantos nucleos detecta. Correrlo una vez normal y otra
con `$env:OMP_NUM_THREADS=1` (PowerShell) da los dos numeros para la
tabla de rendimiento del informe.

### `delaunay_test`
Visor grafico de validacion. Nube de puntos sintetica (esfera / cubo /
toro) o cargada desde un `.xyz` real, con controles de alpha-shape,
wireframe de tetraedros, y boton de validacion en vivo.

### `sapo3d`
Reconstruccion completa. Lee `data/manifest.tsv`, muestra un checklist
de los 17 organos con color editable por cada uno (clic en el swatch
abre el picker), carga perezosa en RAM (cada organo se triangula recien
la primera vez que lo activas dentro de esa corrida), slider de alpha
global con boton "Aplicar" que re-filtra sin re-triangular. Vuelve a
triangular todo desde cero cada vez que abris el programa.

### `sapo3d_hd`
Igual a `sapo3d`, pero con **cache binario en disco**
(`data/cache/<organo>_<numPuntos>.bin`): la primera vez que un organo se
triangula, el resultado (puntos + tetraedros, con su circunesfera ya
calculada) se guarda en ese archivo. En corridas siguientes del
programa -- aunque hayas cerrado y vuelto a abrir -- se carga directo
del cache en vez de re-correr Bowyer-Watson. Boton "Precompilar todos"
para generar el cache de los 17 sin necesidad de activarlos uno por uno
en pantalla. El checklist marca `[cache]` en los organos que se
cargaron del cache en vez de triangularse en esa corrida.

Si cambias `target_points` de un organo (re-corriendo
`extract_surface.py` con otro valor), el nombre de archivo del cache
cambia solo (incluye la cantidad de puntos), asi que no hay que borrar
nada a mano, simplemente no va a existir cache para el nuevo conteo
todavia, y se genera la primera vez que lo actives.

### `sapo3d_adaptive`
Igual a `sapo3d_hd`, pero con **alpha-shape adaptativo**: el radio de circunesfera se calcula localmente para cada tetraedro
segun la densidad de puntos vecinos, en vez de usar un unico alpha global. Esto permite reconstruir mejor organos con zonas anchas y finas a la vez, preservando huecos menores en protuberancias finas sin tener que sacrificar detalle en el cuerpo principal. El slider de alpha ahora ajusta un factor de escala global sobre los radios locales, en vez de un alpha unico.

## Pipeline de datos (TIFF -> nube de puntos)

1. `extract_shell`: de la mascara binaria solida, se queda solo con los
   voxeles de borde (erosion binaria + resta). Reduce el conteo de
   puntos entre 5x y 20x segun el organo antes de tocar nada mas.
2. `voxel_grid_downsample`: sub-muestreo uniforme por grilla 3D (un
   punto por celda ocupada), mas parejo espacialmente que un muestreo
   puramente aleatorio.
3. **Centrado compartido**: todos los organos se centran respecto al
   mismo punto de referencia (el centro del volumen completo del TIFF,
   igual para los 17 archivos), no cada uno por su cuenta, asi se
   preserva la posicion relativa entre organos al combinarlos en
   `sapo3d`, `sapo3d_hd` o `sapo3d_adaptive`.
4. Estadistica de vecino mas cercano -> alpha sugerido (`2.0x`-`3.0x` la
   distancia mediana al vecino mas cercano).

`extract_all.py` hace esto para los 17 de una y calcula un target de
puntos por organo escalado segun el tamano de su cascara (calibrado con
el caso del bazo). Si en algun organo puntual queres mas detalle,
`extract_surface.py <tiff> <target_points> <salida.xyz>` deja
especificarlo a mano y actualiza solo la entrada de ESE organo en
`manifest.json`/`manifest.tsv` (via `manifest_utils.py`), sin pisar los
otros 16. Importante: si le subis los puntos a un organo despues de
haber generado su cache en `sapo3d_hd`, el nombre del `.bin` cambia
automaticamente (incluye la cantidad de puntos), asi que no hace falta
borrar nada -- el cache viejo simplemente queda sin usarse.

### Ejecucion de los scripts

Todos se corren desde la raiz del repo, con Python 3 + `pip install
tifffile numpy scipy`.

```bash
# Los 17 organos de una, con el target de puntos automatico por organo
python scripts/extract_all.py data data

# Un organo puntual, con un target de puntos manual (ej. mas detalle
# para un organo grande/complejo)
python scripts/extract_surface.py data/liverMasks.tiff 5000 data/liver_points.xyz

# Solo referencia: conteo de voxeles ORIGINALES (activos y de cascara)
# por organo, sin generar nada util, soloo para saber el techo real de
# detalle disponible antes de subir target_points
python scripts/report_voxel_counts.py data
```

Para el informe, se corrio `extract_surface.py` una vez por organo
usando como `target_points` el maximo razonable (el conteo de voxeles
de su propia cascara, sin comprimir nada) para los organos chicos y
medianos, por ejemplo:

```bash
python scripts/extract_surface.py data/spleenMasks.tiff 1385 data/spleen_points_max.xyz
python scripts/extract_surface.py data/liverMasks.tiff 42951 data/liver_points_max.xyz
```

**Cuidado con `muscle` (822,268) y `skeleton` (198,340):** el algoritmo
es `O(n^2)` (ver [Complejidad y optimizaciones](#complejidad-y-optimizaciones)),
asi que a esos tamanos NO es cuestion de minutos. Calibrando con el
benchmark real (`delaunay_benchmark`, 3000 puntos -> 0.24s en 1
nucleo), la proyeccion para `muscle` a su maximo real son
**~5 horas**. Para esos dos organos se uso un tope practico en vez del
maximo real:

```bash
python scripts/extract_surface.py data/skeletonMasks.tiff 25000 data/skeleton_points_max.xyz
python scripts/extract_surface.py data/muscleMasks.tiff 25000 data/muscle_points_max.xyz
```

Estos `_max.xyz` (y sus caches `.bin` correspondientes, que en el caso
de `muscle`/`skeleton` pesan varios cientos de MB) **no se versionan**
,son solo para comparar detalle/tiempos de forma puntual, no para el
uso normal de `sapo3d`/`sapo3d_hd`/ `sapo3d_adaptive` (que ya vienen calibrados con un
tope de ~3000 puntos que son extract_all.py).

Si quisieramos usar los maximos reales, 
habria que correr `extract_surface.py` 
con esos conteos y esperar los unicos que demoran para latraingulacion son muscle y skeleton,
para que se regenere el cache de  puntos la primera vez que se active cada organo en `sapo3d_hd`. `sapo3d_adaptive` tambien soporta esos conteos, pero el tiempo de triangulacion sigue siendo el mismo (Bowyer-Watson incremental es O(n^2) sin optimizaciones de busqueda).

Automaticamente, los caches `.bin` se nombran con la cantidad de puntos, asi que no hay que borrar nada a mano: si cambias `target_points`, el cache viejo queda sin usarse y se genera uno nuevo la primera vez que actives ese organo. Si quieiseramos usar caches viejos solo se especifica con el extract_surface.py el mismo target_points que se uso para generarlos.

```bash
python scripts/extract_surface.py data/bloodMasks.tiff 26035 data/blood_points_max.xyz
python scripts/extract_surface.py data/brainMasks.tiff 5374 data/brain_points_max.xyz
python scripts/extract_surface.py data/duodenumMasks.tiff 12182 data/duodenum_points_max.xyz
python scripts/extract_surface.py data/eyeMasks.tiff 9024 data/eye_points_max.xyz
python scripts/extract_surface.py data/eyeRetnaMasks.tiff 13317 data/eyeRetna_points_max.xyz
python scripts/extract_surface.py data/eyeWhiteMasks.tiff 5165 data/eyeWhite_points_max.xyz
python scripts/extract_surface.py data/heartMasks.tiff 8648 data/heart_points_max.xyz
python scripts/extract_surface.py data/ileumMasks.tiff 9874 data/ileum_points_max.xyz
python scripts/extract_surface.py data/kidneyMasks.tiff 12199 data/kidney_points_max.xyz
python scripts/extract_surface.py data/lIntestineMasks.tiff 9634 data/lIntestine_points_max.xyz
python scripts/extract_surface.py data/liverMasks.tiff 42951 data/liver_points_max.xyz
python scripts/extract_surface.py data/lungMasks.tiff 13210 data/lung_points_max.xyz
python scripts/extract_surface.py data/muscleMasks.tiff 822268 data/muscle_points_max.xyz
python scripts/extract_surface.py data/nerveMasks.tiff 16649 data/nerve_points_max.xyz
python scripts/extract_surface.py data/skeletonMasks.tiff 198340 data/skeleton_points_max.xyz
python scripts/extract_surface.py data/spleenMasks.tiff 1385 data/spleen_points_max.xyz
python scripts/extract_surface.py data/stomachMasks.tiff 67602 data/stomach_points_max.xyz
```

Puede ser tambien, el nombre de los archivos de salida`.xyz` no tiene que ser `_max`, puede ser cualquier nombre, lo importante es que el `target_points` sea el mismo que se uso para generar el cache si queremos usarlo despues en `sapo3d_hd`, `sapo3d_adaptive`, `sapo3d`.

Esto modifica data/manifest.tsv para que el target_points de cada organo coincida con el que se uso en extract_surface.py, asi que si se cambia el target_points de un organo, el cache viejo queda sin usarse y se genera uno nuevo la primera vez que se active ese organo.
```bash
python scripts/extract_surface.py data/bloodMasks.tiff 5000 data/blood_points.xyz
python scripts/extract_surface.py data/brainMasks.tiff 2000 data/brain_points.xyz
python scripts/extract_surface.py data/duodenumMasks.tiff 3000 data/duodenum_points.xyz
python scripts/extract_surface.py data/eyeMasks.tiff 3000 data/eye_points.xyz
python scripts/extract_surface.py data/eyeRetnaMasks.tiff 3000 data/eyeRetna_points.xyz
python scripts/extract_surface.py data/eyeWhiteMasks.tiff 2000 data/eyeWhite_points.xyz
python scripts/extract_surface.py data/heartMasks.tiff 3000 data/heart_points.xyz
python scripts/extract_surface.py data/ileumMasks.tiff 3000 data/ileum_points.xyz
python scripts/extract_surface.py data/kidneyMasks.tiff 3000 data/kidney_points.xyz
python scripts/extract_surface.py data/lIntestineMasks.tiff 3000 data/lIntestine_points.xyz
python scripts/extract_surface.py data/liverMasks.tiff 10000 data/liver_points.xyz
python scripts/extract_surface.py data/lungMasks.tiff 3000 data/lung_points.xyz
python scripts/extract_surface.py data/muscleMasks.tiff 50000 data/muscle_points.xyz
python scripts/extract_surface.py data/nerveMasks.tiff 5000 data/nerve_points.xyz
python scripts/extract_surface.py data/skeletonMasks.tiff 20000 data/skeleton_points.xyz
python scripts/extract_surface.py data/spleenMasks.tiff 500 data/spleen_points.xyz
python scripts/extract_surface.py data/stomachMasks.tiff 15000 data/stomach_points.xyz
```
## Resultados de validacion

Corridas reales de `delaunay_core_test` (ver `tests/test_main.cpp`):

```
[Caso 1] Esfera con 40 puntos
  Tetraedros generados: 123
  Violaciones de la propiedad Delaunay: 0
  Caras de frontera (superficie): 76
  Caras esperadas (Euler, V=40): 76          <- coincide exacto

[Caso 2] Nube aleatoria, 60 puntos en un cubo
  Tetraedros generados: 285
  Violaciones de la propiedad Delaunay: 0
  Caras de frontera (envolvente convexa): 46

[Caso 3] Toro (R=5, r=2), forma NO convexa
  Puntos: 288
  Tetraedros (triangulacion completa): 1524
  Violaciones de la propiedad Delaunay: 0
  Caras de frontera SIN alpha (casco convexo, tapa el agujero): 332
  alpha=1.5 -> tetraedros: 0,    caras: 0      (demasiado agresivo)
  alpha=2.5 -> tetraedros: 1128, caras: 576    (coincide con Euler genero 1: F=2V=576)
  alpha=4.0 -> tetraedros: 1260, caras: 524    (ya empieza a tapar el agujero)
```

El caso del toro es la prueba clave: para una superficie de genero 1
(con un agujero), la caracteristica de Euler predice `F = 2V` caras en
una triangulacion cerrada. Con 288 puntos eso son 576 caras esperadas,
y `alpha=2.5` las reproduce exactas, confirma que el alpha-shape
reconstruye la topologia real (agujero incluido), no solo el casco
convexo.

Validado tambien con datos reales: bazo (574 puntos, alpha~2.5) e
higado (3039 puntos, alpha~9.6) reconstruyen formas reconocibles con
huecos menores en zonas de baja densidad de puntos.

## Complejidad y optimizaciones

**Bowyer-Watson (insercion incremental):** por cada uno de los `n`
puntos, se recorren los tetraedros existentes (hasta `O(n)` en el peor
caso) para clasificarlos como validos/invalidos via su circunesfera
eso da una cota de **`O(n^2)`** en el peor caso para el algoritmo
completo (sin estructuras aceleradoras tipo *walk* o *kd-tree*, que no
se implementaron por alcance del laboratorio). En la practica, para las
nubes de unos pocos miles de puntos que usamos aca, esto corre en
fracciones de segundo (ver tabla de tiempos abajo).

**`boundaryFaces()` (extraccion de superficie):** cada tetraedro aporta
4 caras; una cara es de frontera si aparece en un solo tetraedro. La
primera implementacion comparaba cada cara contra todas las demas
(`O(caras^2)`); se optimizo a un `unordered_map` con hash sobre los 3
indices ordenados de cada cara, bajando a **`O(caras)`** amortizado.
Impacto medido (3000 puntos, 19511 tetraedros, 78044 caras):

| Version | Tiempo `boundaryFaces()` |
|---|---|
| `O(caras^2)`, comparacion por pares | 2.20 s |
| `O(caras)`, hash map | 0.005 s |

**OpenMP:** la clasificacion de tetraedros dentro de cada insercion
(el paso `O(n)` que domina el costo total) es paralelizable -- cada
tetraedro se evalua de forma independiente contra el punto que se esta
insertando, sin dependencias entre ellos. Se agrego un
`#pragma omp parallel for` sobre ese loop (activado solo cuando hay
mas de 500 tetraedros, para no pagar el overhead de crear hilos en
nubes chicas). Compila y corre igual sin OpenMP instalado, si no esta
disponible, CMake avisa y el codigo sigue siendo 100% funcional,
simplemente secuencial.

Medido en la maquina de desarrollo (16 nucleos logicos, build Release,
`delaunay_benchmark`, forzando `$env:OMP_NUM_THREADS=1` vs. sin limite):

| Puntos | Tetraedros | 1 nucleo | 16 nucleos | Razon (16n/1n) |
|---|---:|---:|---:|---|
| 500    | 3,002   | 0.0271 s | 0.0598 s   | 2.21x MAS LENTO |
| 1500   | 9,647   | 0.0959 s | 0.1974 s   | 2.06x MAS LENTO |
| 3000   | 19,511  | 0.4158 s | 0.6481 s   | 1.56x MAS LENTO |
| 5000   | 32,735  | 1.6680 s | 2.1833 s   | 1.31x MAS LENTO |
| 10000  | 66,117  | 8.7580 s | 10.1975 s  | 1.16x MAS LENTO |
| 20000  | 133,239 | 42.6774 s| 35.9358 s  | **1.19x mas rapido** |
| 30000  | 200,213 | 96.2677 s| 84.4323 s  | **1.14x mas rapido** |
| 60000  | 402,060 | 399.3201 s| 366.9374 s| **1.09x mas rapido** |

**Hallazgo (medido, no hipotetico):** con la granularidad actual
(una region paralela nueva por cada punto insertado), OpenMP es
**mas lento** que la version secuencial hasta unos 10.000-20.000
puntos, el overhead de coordinar 16 hilos miles de veces por corrida
supera el trabajo real de cada llamada. Recien alrededor de 20.000
puntos el paralelismo empieza a compensar. Pero el beneficio tampoco
crece indefinidamente: el speedup **llega a un pico (~1.19x en 20.000)
y despues se reduce** (1.14x en 30.000, 1.09x en 60.000). Esto es
consistente con un cuello de botella de **ancho de banda de memoria**:
con nubes grandes, el arreglo de tetraedros ya no entra en cache, y los
16 nucleos compiten por el mismo bus de memoria al leerlo en paralelo
el trabajo por tetraedro (comparar contra un centro+radio) es
demasiado poco computo por byte leido como para que mas hilos seccione
la ganancia proporcionalmente.

**Conclusion practica:** los 17 organos de este proyecto usan como
mucho ~3000 puntos (Seccion [Los 17 organos](#los-17-organos)), es
decir, quedan del lado donde OpenMP **perjudica**. Por eso `sapo3d` y
`sapo3d_hd` no dependen de que OpenMP ayude, el tamano tipico de
nube usado en este proyecto no llega al punto de cruce.

**CUDA:** se evaluo y se descarto para este laboratorio. Bowyer-Watson
incremental es inherentemente secuencial entre inserciones (el estado
de la malla tras insertar el punto `i` es precondicion para insertar el
punto `i+1`), asi que no se presta a paralelizarse en GPU sin
reemplazar el algoritmo completo por un enfoque distinto (ej. Delaunay
paralelo por *divide and conquer*, o triangulacion por lotes con
re-flipping), que excede el alcance de este trabajo. Ademas, la GPU ya
se usa activamente para el render (shaders GLSL, buffers de vertices),
que es la parte que efectivamente se beneficia de paralelismo masivo en
este proyecto.

## Los 17 organos

Conteos de voxeles **originales** (antes de cualquier sub-muestreo),
via `report_voxel_counts.py`:

| Organo | Voxeles activos | Voxeles cascara | Puntos usados | Alpha auto |
|---|---:|---:|---:|---:|
| blood | 35,492 | 26,035 | 2,492 | 5.00 |
| brain | 19,172 | 5,374 | 1,200 | 3.54 |
| duodenum | 38,224 | 12,182 | 1,741 | 3.54 |
| eye | 53,231 | 9,024 | 1,525 | 3.54 |
| eyeRetna | 37,146 | 13,317 | 1,832 | 3.54 |
| eyeWhite | 16,085 | 5,165 | 1,158 | 3.54 |
| heart | 37,196 | 8,648 | 1,561 | 3.54 |
| ileum | 27,916 | 9,874 | 1,516 | 3.54 |
| kidney | 39,245 | 12,199 | 1,753 | 3.54 |
| lIntestine | 51,660 | 9,634 | 1,625 | 3.54 |
| liver | 205,245 | 42,951 | 3,039 | 5.00 |
| lung | 47,454 | 13,210 | 1,782 | 3.54 |
| muscle | 4,097,993 | 822,268 | 2,933 | 17.32 |
| nerve | 24,314 | 16,649 | 1,987 | 3.54 |
| skeleton | 440,640 | 198,340 | 3,015 | 8.29 |
| spleen | 3,935 | 1,385 | 574 | 2.50 |
| stomach | 134,323 | 67,602 | 2,975 | 5.59 |
| **TOTAL** | **5,309,271** | **1,273,857** | **32,708** | -- |

`muscle` y `skeleton` son los que mas comprimen (822K y 198K voxeles de
cascara, bajados al mismo tope de 3000 puntos que todos) -- ver
limitaciones.

## Limitaciones y dificultades

- **Alpha-shape de radio unico por organo**: un solo valor de alpha no
  siempre reconstruye bien zonas anchas y finas del mismo organo a la
  vez. En el bazo y el higado aparecen huecos menores en las
  extremidades/protuberancias mas finas, donde la distancia entre
  puntos vecinos crece y el mismo alpha que conecta bien el cuerpo
  principal ya no alcanza ahi. Una alpha-shape adaptativa (radio
  variable segun densidad local) resolveria esto, pero no se
  implemento por alcance del laboratorio.
- **Sub-muestreo con tope fijo de puntos**: `muscle` (822K voxeles de
  cascara) y `skeleton` (198K) se comprimen al mismo techo de ~3000
  puntos que organos mucho mas chicos como el bazo (1385). Esto
  significa menos detalle relativo en las estructuras mas grandes y
  complejas del sapo. Se puede mitigar corriendo `extract_surface.py`
  a mano con un `target_points` mayor para esos casos puntuales, a
  costa de un tiempo de triangulacion mayor (Bowyer-Watson incremental
  escala peor que lineal con la cantidad de puntos).
- **Complejidad del algoritmo**: la implementacion de Bowyer-Watson usa
  busqueda lineal sobre los tetraedros existentes en cada insercion
  (sin estructura acelerada tipo *walk* o *kd-tree*), lo que la hace
  adecuada para las nubes de unos pocos miles de puntos que usamos aca,
  pero no escalaria bien a nubes de cientos de miles de puntos sin
  optimizar esa busqueda.
- **Formato TIFF asumido**: se asume que todas las mascaras comparten
  el mismo shape de volumen (voxeles isotropicos, sin espaciado
  distinto por eje) valido para este dataset puntual, pero no es un
  supuesto general para cualquier dataset medico.

## Capturas

*(agregar en `docs/`, formato GIF corto para las demos interactivas y
PNG para las estaticas)*

- `docs/demo.gif` -- captura general de `sapo3d` con varios organos
  activos, para mostrar el uso general de la app.
  ![demo](docs/demo.gif)
- `docs/torus_alpha.gif` -- corto mostrando el toro en `delaunay_test`
  con el checkbox de alpha-shape activandose/desactivandose (se ve
  aparecer y desaparecer el agujero).
  ![torus_alpha](docs/torus_alpha.gif)
- `docs/liver_reconstruction.gif` -- carga del higado desde archivo,
  ajuste del slider de alpha en vivo.
  ![liver_reconstruction](docs/liver_reconstruction.gif)
- `docs/organ_selector.gif` -- checklist de organos en `sapo3d`
  prendiendose uno por uno.
  ![organ_selector](docs/organ_selector.gif)
- `docs/sapo3d_hd_cache.gif` -- corto mostrando `sapo3d_hd`: primera
  activacion de un organo (tarda, triangula), cerrar y volver a abrir
  el programa, activar el mismo organo de nuevo (instantaneo, se ve el
  tag `[cache]`), y edicion de color en vivo con el picker.
  ![sapo3d_hd_cache](docs/sapo3d_hd_cache.gif)
- `docs/sapo3d_adaptive.gif` -- corto mostrando `sapo3d_adaptive`: ajuste del factor de escala global sobre los radios locales de alpha-shape, preservando huecos menores en protuberancias finas sin sacrificar detalle en el cuerpo principal.
  ![sapo3d_adaptive](docs/sapo3d_adaptive.gif)
