"""
Corre extract_surface sobre TODOS los TIFF de una carpeta y genera un
manifest.json con metadata de cada organo (archivo de puntos, cantidad,
alpha sugerido) para que el ejecutable C++ (sapo3d) sepa que cargar.

Requisitos:
    pip install tifffile numpy scipy

Uso:
    python extract_all.py <carpeta_data> [carpeta_salida]

Ejemplo:
    python extract_all.py data data
"""

import sys
import os
import glob
import json
import numpy as np
import tifffile as tiff

from tiff_utils import extract_shell, voxel_grid_downsample, nearest_neighbor_stats, suggested_target_points


def organ_name_from_filename(filename: str) -> str:
    base = os.path.basename(filename)
    for suffix in ("Masks.tiff", "Masks.tif", ".tiff", ".tif"):
        if base.endswith(suffix):
            return base[: -len(suffix)]
    return base


def process_one(tiff_path: str, out_dir: str) -> dict:
    name = organ_name_from_filename(tiff_path)
    print(f"\n--- {name} ({os.path.basename(tiff_path)}) ---")

    mask = tiff.imread(tiff_path).astype(bool)
    active = int(mask.sum())
    print(f"  Shape: {mask.shape}, voxeles activos: {active}")

    if active == 0:
        print("  ADVERTENCIA: mascara vacia, se omite.")
        return {"organ": name, "skipped": True, "reason": "mascara vacia"}

    shell = extract_shell(mask)
    coords = np.argwhere(shell).astype(np.float64)
    print(f"  Voxeles de cascara: {len(coords)}")

    target_points = suggested_target_points(len(coords))
    points = coords[:, [2, 1, 0]]
    # Mismo offset compartido para los 17 organos (ver comentario en
    # extract_surface.py) -- preserva la posicion relativa entre organos.
    volume_center = np.array([mask.shape[2], mask.shape[1], mask.shape[0]], dtype=np.float64) / 2.0
    points -= volume_center
    points = voxel_grid_downsample(points, target_points)
    print(f"  Puntos finales: {len(points)} (target: {target_points})")

    median_nn, mean_nn, min_nn, max_nn = nearest_neighbor_stats(points)
    alpha_auto = 2.5 * median_nn
    print(f"  vecino-mas-cercano mediana={median_nn:.3f}  alpha_auto={alpha_auto:.3f}")

    out_file = os.path.join(out_dir, f"{name}_points.xyz")
    with open(out_file, "w") as f:
        f.write(f"# {tiff_path} -> {len(points)} puntos, alpha_auto={alpha_auto:.3f}\n")
        for p in points:
            f.write(f"{p[0]:.4f} {p[1]:.4f} {p[2]:.4f}\n")

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


def main():
    if len(sys.argv) < 2:
        print("Uso: python extract_all.py <carpeta_data> [carpeta_salida]")
        sys.exit(1)

    data_dir = sys.argv[1]
    out_dir = sys.argv[2] if len(sys.argv) > 2 else data_dir

    files = sorted(glob.glob(os.path.join(data_dir, "*.tiff")) +
                    glob.glob(os.path.join(data_dir, "*.tif")))
    if not files:
        print(f"No se encontraron .tiff/.tif en {data_dir}")
        sys.exit(1)

    print(f"Encontrados {len(files)} organos en {data_dir}")

    manifest = {"organs": []}
    for f in files:
        entry = process_one(f, out_dir)
        manifest["organs"].append(entry)

    manifest_path = os.path.join(out_dir, "manifest.json")
    with open(manifest_path, "w") as f:
        json.dump(manifest, f, indent=2)

    # Version .tsv (texto plano) para consumir desde C++ sin libreria JSON.
    tsv_path = os.path.join(out_dir, "manifest.tsv")
    with open(tsv_path, "w") as f:
        f.write("# organ\tpoints_file\tnum_points\talpha_auto\talpha_low\talpha_high\n")
        for e in manifest["organs"]:
            if e.get("skipped"):
                continue
            f.write(f"{e['organ']}\t{e['points_file']}\t{e['num_points']}\t"
                     f"{e['alpha_auto']:.4f}\t{e['alpha_range'][0]:.4f}\t{e['alpha_range'][1]:.4f}\n")

    ok = sum(1 for e in manifest["organs"] if not e.get("skipped"))
    print(f"\n=== Listo: {ok}/{len(files)} organos procesados. Manifest: {manifest_path} y {tsv_path} ===")


if __name__ == "__main__":
    main()
