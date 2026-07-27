"""Manejo del manifest (leer/escribir/actualizar) + extraccion de un
organo individual, compartido entre extract_surface.py y extract_all.py
para que ambos mantengan el mismo manifest.json / manifest.tsv."""

import os
import json
import numpy as np
import tifffile as tiff

from tiff_utils import extract_shell, voxel_grid_downsample, nearest_neighbor_stats, per_point_local_scale, suggested_target_points


def organ_name_from_filename(filename: str) -> str:
    base = os.path.basename(filename)
    for suffix in ("Masks.tiff", "Masks.tif", ".tiff", ".tif"):
        if base.endswith(suffix):
            return base[: -len(suffix)]
    return base


def extract_organ(tiff_path: str, out_dir: str, target_points: int = None) -> dict:
    """Extrae UN organo: cascara -> submuestreo -> centrado compartido ->
    exporta .xyz -> devuelve el dict de entrada para el manifest."""
    name = organ_name_from_filename(tiff_path)

    mask = tiff.imread(tiff_path).astype(bool)
    active = int(mask.sum())
    if active == 0:
        return {"organ": name, "skipped": True, "reason": "mascara vacia"}

    shell = extract_shell(mask)
    coords = np.argwhere(shell).astype(np.float64)

    if target_points is None:
        target_points = suggested_target_points(len(coords))

    points = coords[:, [2, 1, 0]]
    # Centrado COMPARTIDO (centro del volumen, no el centroide de este
    # organo) -- preserva la posicion relativa entre organos.
    volume_center = np.array([mask.shape[2], mask.shape[1], mask.shape[0]], dtype=np.float64) / 2.0
    points -= volume_center

    points = voxel_grid_downsample(points, target_points)
    median_nn, mean_nn, min_nn, max_nn = nearest_neighbor_stats(points)
    alpha_auto = 2.5 * median_nn
    local_scale = per_point_local_scale(points)

    out_file = os.path.join(out_dir, f"{name}_points.xyz")
    with open(out_file, "w") as f:
        f.write(f"# {tiff_path} -> {len(points)} puntos, alpha_auto={alpha_auto:.3f}\n")
        f.write("# columnas: x y z local_scale (4ta col = densidad local, "
                "para alpha adaptativo; version vieja sin 4ta col sigue "
                "siendo compatible)\n")
        for p, ls in zip(points, local_scale):
            f.write(f"{p[0]:.4f} {p[1]:.4f} {p[2]:.4f} {ls:.4f}\n")

    return {
        "organ": name,
        "skipped": False,
        "source_tiff": os.path.basename(tiff_path),
        "points_file": os.path.basename(out_file),
        "num_points": int(len(points)),
        "volume_center_offset": volume_center.tolist(),
        "nn_median": median_nn,
        "nn_mean": mean_nn,
        "nn_min": min_nn,
        "nn_max": max_nn,
        "alpha_auto": alpha_auto,
        "alpha_range": [2.0 * median_nn, 3.0 * median_nn],
    }


def load_manifest(out_dir: str) -> dict:
    path = os.path.join(out_dir, "manifest.json")
    if not os.path.exists(path):
        return {"organs": []}
    with open(path) as f:
        return json.load(f)


def upsert_entry(manifest: dict, entry: dict) -> None:
    """Reemplaza la entrada existente del mismo organo, o la agrega."""
    organs = manifest.setdefault("organs", [])
    for i, e in enumerate(organs):
        if e.get("organ") == entry.get("organ"):
            organs[i] = entry
            return
    organs.append(entry)


def write_manifest(out_dir: str, manifest: dict) -> None:
    manifest_path = os.path.join(out_dir, "manifest.json")
    with open(manifest_path, "w") as f:
        json.dump(manifest, f, indent=2)

    tsv_path = os.path.join(out_dir, "manifest.tsv")
    with open(tsv_path, "w") as f:
        f.write("# organ\tpoints_file\tnum_points\talpha_auto\talpha_low\talpha_high\n")
        for e in manifest.get("organs", []):
            if e.get("skipped"):
                continue
            f.write(f"{e['organ']}\t{e['points_file']}\t{e['num_points']}\t"
                     f"{e['alpha_auto']:.4f}\t{e['alpha_range'][0]:.4f}\t{e['alpha_range'][1]:.4f}\n")
