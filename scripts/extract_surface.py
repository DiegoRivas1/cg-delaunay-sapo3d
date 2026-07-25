"""
Extrae la superficie (cascara de voxeles) de una mascara TIFF binaria,
la sub-muestrea a una nube de puntos manejable, y la exporta en formato
.xyz (texto plano, una fila "x y z" por punto) para usar en el pipeline
de Delaunay en C++.

Requisitos:
    pip install tifffile numpy scipy

Uso:
    python extract_surface.py <archivo.tiff> [target_points] [salida.xyz]

Ejemplo:
    python extract_surface.py data/spleenMasks.tiff 600 data/spleen_points.xyz
"""

import sys
import numpy as np
import tifffile as tiff
from scipy import ndimage
from scipy.spatial import cKDTree


def extract_shell(mask: np.ndarray) -> np.ndarray:
    """Devuelve solo los voxeles de la CASCARA (borde) de la mascara,
    no el volumen solido completo. Reduce muchisimo el conteo de puntos
    antes de submuestrear."""
    eroded = ndimage.binary_erosion(mask)
    shell = mask & ~eroded
    return shell


def voxel_grid_downsample(points: np.ndarray, target_count: int) -> np.ndarray:
    """Downsample uniforme: bins de una grilla 3D, un punto (el centroide)
    por celda ocupada. Da mejor cobertura espacial que un sub-muestreo
    puramente aleatorio. Ajusta el tamano de celda por busqueda binaria
    hasta acercarse a target_count."""
    if len(points) <= target_count:
        return points

    mins = points.min(axis=0)
    maxs = points.max(axis=0)
    diag = np.linalg.norm(maxs - mins)

    lo, hi = diag / 500.0, diag / 2.0
    best = points

    for _ in range(25):
        cell = (lo + hi) / 2.0
        idx = np.floor((points - mins) / cell).astype(np.int64)
        # hash de celda -> nos quedamos con un representante por celda
        keys = idx[:, 0] * 1_000_000 + idx[:, 1] * 1_000 + idx[:, 2]
        _, unique_indices = np.unique(keys, return_index=True)
        candidate = points[unique_indices]

        if len(candidate) > target_count * 1.05:
            lo = cell  # celda muy chica -> quedan muchos puntos, agrandar
        elif len(candidate) < target_count * 0.95:
            hi = cell  # celda muy grande -> quedan pocos, achicar
        else:
            return candidate

        best = candidate

    return best


def main():
    if len(sys.argv) < 2:
        print("Uso: python extract_surface.py <archivo.tiff> [target_points] [salida.xyz]")
        sys.exit(1)

    tiff_path = sys.argv[1]
    target_points = int(sys.argv[2]) if len(sys.argv) > 2 else 600
    out_path = sys.argv[3] if len(sys.argv) > 3 else tiff_path.rsplit(".", 1)[0] + "_points.xyz"

    print(f"Leyendo {tiff_path} ...")
    mask = tiff.imread(tiff_path).astype(bool)
    print(f"  Shape: {mask.shape}, voxeles activos: {mask.sum()}")

    print("Extrayendo cascara (superficie) ...")
    shell = extract_shell(mask)
    coords = np.argwhere(shell).astype(np.float64)  # (z, y, x)
    print(f"  Voxeles de cascara: {len(coords)}")

    if len(coords) == 0:
        print("ERROR: la mascara esta vacia, no hay nada que extraer.")
        sys.exit(1)

    # Reordenar a (x, y, z) y centrar en el origen (mas comodo para la camara).
    points = coords[:, [2, 1, 0]]
    centroid = points.mean(axis=0)
    points -= centroid

    print(f"Submuestreando a ~{target_points} puntos ...")
    points = voxel_grid_downsample(points, target_points)
    print(f"  Puntos finales: {len(points)}")

    # Estadisticas de vecino mas cercano -> sugerencia de alpha para el
    # alpha-shape (regla practica: alpha ~ 2.5x la distancia mediana al
    # vecino mas cercano; ajustar con el slider del visor si hace falta).
    tree = cKDTree(points)
    dists, _ = tree.query(points, k=2)  # k=2: el mas cercano es el mismo punto
    nn_dist = dists[:, 1]
    median_nn = float(np.median(nn_dist))
    mean_nn = float(np.mean(nn_dist))

    print(f"  Distancia al vecino mas cercano -> mediana: {median_nn:.4f}, "
          f"media: {mean_nn:.4f}, min: {nn_dist.min():.4f}, max: {nn_dist.max():.4f}")
    print(f"  Alpha sugerido (2.0x - 3.0x mediana): "
          f"{2.0 * median_nn:.3f} - {3.0 * median_nn:.3f}")

    with open(out_path, "w") as f:
        f.write(f"# {tiff_path} -> {len(points)} puntos, alpha sugerido "
                 f"{2.0 * median_nn:.3f}-{3.0 * median_nn:.3f}\n")
        for p in points:
            f.write(f"{p[0]:.4f} {p[1]:.4f} {p[2]:.4f}\n")

    print(f"-> Exportado a {out_path}")


if __name__ == "__main__":
    main()
