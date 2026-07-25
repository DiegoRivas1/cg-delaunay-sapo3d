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

Si no se pasa target_points, se calcula automaticamente segun el tamano
de la cascara del organo (ver tiff_utils.suggested_target_points).
"""

import sys
import numpy as np
import tifffile as tiff

from tiff_utils import extract_shell, voxel_grid_downsample, nearest_neighbor_stats, suggested_target_points


def main():
    if len(sys.argv) < 2:
        print("Uso: python extract_surface.py <archivo.tiff> [target_points] [salida.xyz]")
        sys.exit(1)

    tiff_path = sys.argv[1]
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

    target_points = int(sys.argv[2]) if len(sys.argv) > 2 else suggested_target_points(len(coords))
    if len(sys.argv) <= 2:
        print(f"  (target_points no especificado, se calculo automaticamente: {target_points})")

    # Reordenar a (x, y, z) y centrar en el origen.
    points = coords[:, [2, 1, 0]]
    # IMPORTANTE: centrar respecto al centro del VOLUMEN completo, no al
    # centroide de este organo -- todos los organos comparten el mismo
    # shape de TIFF, asi que este offset es igual para los 17 y preserva
    # la posicion relativa entre ellos (necesario para combinarlos despues
    # en el sapo completo).
    volume_center = np.array([mask.shape[2], mask.shape[1], mask.shape[0]], dtype=np.float64) / 2.0
    points -= volume_center

    print(f"Submuestreando a ~{target_points} puntos ...")
    points = voxel_grid_downsample(points, target_points)
    print(f"  Puntos finales: {len(points)}")

    median_nn, mean_nn, min_nn, max_nn = nearest_neighbor_stats(points)
    print(f"  Distancia al vecino mas cercano -> mediana: {median_nn:.4f}, "
          f"media: {mean_nn:.4f}, min: {min_nn:.4f}, max: {max_nn:.4f}")
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
