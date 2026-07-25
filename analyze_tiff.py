"""
Analiza los TIFF de mascaras (stacks 3D) para entender la data
antes de implementar Bowyer-Watson / Delaunay.

Requisitos:
    pip install tifffile numpy

Uso:
    python analyze_tiff.py "data"
    python analyze_tiff.py "data/liverMasks.tiff"
"""

import sys
import os
import glob
import numpy as np
import tifffile as tiff


def analyze_file(path):
    print(f"\n{'='*70}")
    print(f"Archivo: {os.path.basename(path)}")
    print(f"{'='*70}")

    try:
        arr = tiff.imread(path)
    except Exception as e:
        print(f"  ERROR leyendo el archivo: {e}")
        return

    print(f"  Shape       : {arr.shape}  (slices, alto, ancho) o (alto, ancho) si es 2D")
    print(f"  Dtype       : {arr.dtype}")
    print(f"  Dimensiones : {arr.ndim}D")

    tam_mb = arr.nbytes / (1024 * 1024)
    print(f"  Tamano en RAM: {tam_mb:.2f} MB")

    valores_unicos = np.unique(arr)
    n_unicos = len(valores_unicos)
    if n_unicos <= 20:
        print(f"  Valores unicos ({n_unicos}): {valores_unicos.tolist()}")
    else:
        print(f"  Valores unicos: {n_unicos} (min={valores_unicos.min()}, max={valores_unicos.max()})")

    # Si parece mascara binaria o de labels, calculamos bounding box de los voxeles != 0
    nonzero = np.argwhere(arr != 0)
    if nonzero.size == 0:
        print("  ADVERTENCIA: el volumen esta completamente vacio (todo ceros)")
        return

    mins = nonzero.min(axis=0)
    maxs = nonzero.max(axis=0)
    print(f"  Bounding box (voxeles != 0): min={mins.tolist()} max={maxs.tolist()}")
    print(f"  Cantidad de voxeles activos: {nonzero.shape[0]} "
          f"({100.0 * nonzero.shape[0] / arr.size:.4f}% del volumen total)")

    # Si tiene mas de 2 valores unicos, probablemente es un label map (varios sub-organos/estructuras)
    if 2 < n_unicos <= 20:
        print("  -> Parece un LABEL MAP (varias estructuras/IDs en el mismo archivo), no mascara binaria")
    elif n_unicos == 2:
        print("  -> Parece MASCARA BINARIA (0 = fondo, valor != 0 = organo)")


def main():
    if len(sys.argv) < 2:
        print("Uso: python analyze_tiff.py <carpeta_o_archivo_tiff>")
        sys.exit(1)

    target = sys.argv[1]

    if os.path.isdir(target):
        files = sorted(glob.glob(os.path.join(target, "*.tiff")) +
                        glob.glob(os.path.join(target, "*.tif")))
        if not files:
            print(f"No se encontraron .tiff/.tif en {target}")
            sys.exit(1)
        print(f"Encontrados {len(files)} archivos TIFF")
        for f in files:
            analyze_file(f)
    elif os.path.isfile(target):
        analyze_file(target)
    else:
        print(f"No existe: {target}")
        sys.exit(1)


if __name__ == "__main__":
    main()
