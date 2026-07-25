#pragma once
#include <string>
#include <vector>

struct OrganEntry {
    std::string name;
    std::string pointsFile;   // relativo a la carpeta del manifest
    int numPoints = 0;
    double alphaAuto = 0.0;
    double alphaLow = 0.0;
    double alphaHigh = 0.0;
};

namespace ManifestIO {

// Lee un manifest.tsv (formato generado por scripts/extract_all.py).
// Devuelve vector vacio si no se pudo abrir. outError opcional para UI.
std::vector<OrganEntry> loadManifest(const std::string& path, std::string* outError = nullptr);

}  // namespace ManifestIO
