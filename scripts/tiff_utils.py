"""Funciones compartidas de extraccion de superficie desde mascaras TIFF."""

import numpy as np
from scipy import ndimage
from scipy.spatial import cKDTree


def extract_shell(mask: np.ndarray) -> np.ndarray:
    """Solo los voxeles de la CASCARA (borde) de la mascara."""
    eroded = ndimage.binary_erosion(mask)
    return mask & ~eroded


def voxel_grid_downsample(points: np.ndarray, target_count: int) -> np.ndarray:
    """Downsample uniforme por grilla 3D (un punto por celda ocupada)."""
    if len(points) <= target_count:
        return points

    mins = points.min(axis=0)
    maxs = points.max(axis=0)
    diag = np.linalg.norm(maxs - mins)
    if diag < 1e-9:
        return points

    lo, hi = diag / 500.0, diag / 2.0
    best = points

    for _ in range(25):
        cell = (lo + hi) / 2.0
        idx = np.floor((points - mins) / cell).astype(np.int64)
        keys = idx[:, 0] * 1_000_000 + idx[:, 1] * 1_000 + idx[:, 2]
        _, unique_indices = np.unique(keys, return_index=True)
        candidate = points[unique_indices]

        if len(candidate) > target_count * 1.05:
            lo = cell
        elif len(candidate) < target_count * 0.95:
            hi = cell
        else:
            return candidate

        best = candidate

    return best


def nearest_neighbor_stats(points: np.ndarray):
    """Devuelve (mediana, media, min, max) de la distancia al vecino mas cercano."""
    tree = cKDTree(points)
    dists, _ = tree.query(points, k=2)
    nn = dists[:, 1]
    return float(np.median(nn)), float(np.mean(nn)), float(nn.min()), float(nn.max())


def suggested_target_points(shell_voxel_count: int, k: float = 16.0,
                             min_points: int = 300, max_points: int = 3000) -> int:
    """Escala el numero de puntos objetivo segun el tamano de la cascara,
    calibrado con el caso del bazo (cascara=1385 -> 600 puntos anduvo bien).
    Organos grandes/complejos (higado, musculo) piden mas puntos; organos
    chicos y compactos (bazo) necesitan menos."""
    raw = k * np.sqrt(shell_voxel_count)
    return int(np.clip(round(raw), min_points, max_points))
