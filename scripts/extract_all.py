"""
Corre extract_surface sobre TODOS los TIFF de una carpeta y genera un
manifest.json/.tsv con metadata de cada organo (archivo de puntos,
cantidad, alpha sugerido) para que el ejecutable C++ (sapo3d) sepa que
cargar. Reescribe el manifest completo (a diferencia de
extract_surface.py, que solo actualiza la entrada de un organo).

Requisitos:
    pip install tifffile numpy scipy

Uso:
    python extract_all.py <carpeta_data> [carpeta_salida]
"""

import sys
import os
import glob

from manifest_utils import extract_organ, write_manifest


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
        name = os.path.basename(f)
        print(f"\n--- {name} ---")
        entry = extract_organ(f, out_dir)
        if entry.get("skipped"):
            print(f"  ADVERTENCIA: {entry.get('reason')}, se omite.")
        else:
            print(f"  Puntos finales: {entry['num_points']}  alpha_auto={entry['alpha_auto']:.3f}")
        manifest["organs"].append(entry)

    write_manifest(out_dir, manifest)

    ok = sum(1 for e in manifest["organs"] if not e.get("skipped"))
    print(f"\n=== Listo: {ok}/{len(files)} organos procesados. "
          f"Manifest: {out_dir}/manifest.json y {out_dir}/manifest.tsv ===")


if __name__ == "__main__":
    main()
