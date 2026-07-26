"""
Reporta cuantos voxeles ACTIVOS y de CASCARA tiene cada TIFF original,
sin generar ni submuestrear nada -- solo para referencia (ej. para saber
el techo real de detalle disponible si mas adelante subis target_points).

Requisitos:
    pip install tifffile numpy scipy

Uso:
    python report_voxel_counts.py <carpeta_data>
"""

import sys
import os
import glob
import numpy as np
import tifffile as tiff

from tiff_utils import extract_shell


def main():
    if len(sys.argv) < 2:
        print("Uso: python report_voxel_counts.py <carpeta_data>")
        sys.exit(1)

    data_dir = sys.argv[1]
    files = sorted(glob.glob(os.path.join(data_dir, "*.tiff")) +
                    glob.glob(os.path.join(data_dir, "*.tif")))
    if not files:
        print(f"No se encontraron .tiff/.tif en {data_dir}")
        sys.exit(1)

    print(f"{'Organo':<15} {'Voxeles activos':>16} {'Voxeles cascara':>16}")
    print("-" * 49)

    total_active = 0
    total_shell = 0

    for f in files:
        name = os.path.basename(f)
        mask = tiff.imread(f).astype(bool)
        active = int(mask.sum())
        shell = int(extract_shell(mask).sum())

        total_active += active
        total_shell += shell
        print(f"{name:<15} {active:>16} {shell:>16}")

    print("-" * 49)
    print(f"{'TOTAL':<15} {total_active:>16} {total_shell:>16}")


if __name__ == "__main__":
    main()
