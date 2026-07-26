"""
Extrae la superficie (cascara de voxeles) de UNA mascara TIFF binaria,
la sub-muestrea, exporta un .xyz, y ACTUALIZA la entrada de ese organo
en manifest.json/.tsv (sin tocar las entradas de los demas organos).

Requisitos:
    pip install tifffile numpy scipy

Uso:
    python extract_surface.py <archivo.tiff> [target_points] [salida.xyz]

Ejemplo:
    python extract_surface.py data/muscleMasks.tiff 5000 data/muscle_points.xyz

Si no se pasa target_points, se calcula automaticamente segun el tamano
de la cascara del organo.
"""

import sys
import os

from manifest_utils import extract_organ, load_manifest, upsert_entry, write_manifest


def main():
    if len(sys.argv) < 2:
        print("Uso: python extract_surface.py <archivo.tiff> [target_points] [salida.xyz]")
        sys.exit(1)

    tiff_path = sys.argv[1]
    target_points = int(sys.argv[2]) if len(sys.argv) > 2 else None
    out_path = sys.argv[3] if len(sys.argv) > 3 else tiff_path.rsplit(".", 1)[0] + "_points.xyz"
    out_dir = os.path.dirname(out_path) or "."

    print(f"Leyendo {tiff_path} ...")
    entry = extract_organ(tiff_path, out_dir, target_points)

    if entry.get("skipped"):
        print(f"ERROR: {entry.get('reason')}")
        sys.exit(1)

    # Si el nombre de salida pedido es distinto del default ("<organ>_points.xyz"),
    # respetamos el nombre pedido pero seguimos registrando el organo por su
    # nombre real (derivado del tiff), asi el manifest no se duplica.
    generated_path = os.path.join(out_dir, entry["points_file"])
    if os.path.abspath(generated_path) != os.path.abspath(out_path):
        os.replace(generated_path, out_path)
        entry["points_file"] = os.path.basename(out_path)

    print(f"  Puntos finales: {entry['num_points']}")
    print(f"  Distancia al vecino mas cercano -> mediana: {entry['nn_median']:.4f}, "
          f"media: {entry['nn_mean']:.4f}, min: {entry['nn_min']:.4f}, max: {entry['nn_max']:.4f}")
    print(f"  Alpha sugerido (2.0x - 3.0x mediana): "
          f"{entry['alpha_range'][0]:.3f} - {entry['alpha_range'][1]:.3f}")
    print(f"-> Exportado a {out_path}")

    manifest = load_manifest(out_dir)
    upsert_entry(manifest, entry)
    write_manifest(out_dir, manifest)
    print(f"-> manifest.json / manifest.tsv actualizados en {out_dir}")


if __name__ == "__main__":
    main()
