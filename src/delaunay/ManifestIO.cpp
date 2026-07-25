#include "ManifestIO.hpp"
#include <fstream>
#include <sstream>

namespace ManifestIO {

std::vector<OrganEntry> loadManifest(const std::string& path, std::string* outError) {
    std::vector<OrganEntry> entries;

    std::ifstream file(path);
    if (!file.is_open()) {
        if (outError) *outError = "No se pudo abrir: " + path;
        return entries;
    }

    std::string line;
    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#') continue;

        std::istringstream iss(line);
        std::string name, pointsFile, numPointsStr, alphaAutoStr, alphaLowStr, alphaHighStr;

        if (!std::getline(iss, name, '\t')) continue;
        if (!std::getline(iss, pointsFile, '\t')) continue;
        if (!std::getline(iss, numPointsStr, '\t')) continue;
        if (!std::getline(iss, alphaAutoStr, '\t')) continue;
        if (!std::getline(iss, alphaLowStr, '\t')) continue;
        std::getline(iss, alphaHighStr, '\t');  // ultimo campo, puede faltar el \t final

        OrganEntry e;
        e.name = name;
        e.pointsFile = pointsFile;
        try {
            e.numPoints = std::stoi(numPointsStr);
            e.alphaAuto = std::stod(alphaAutoStr);
            e.alphaLow = std::stod(alphaLowStr);
            e.alphaHigh = alphaHighStr.empty() ? e.alphaAuto : std::stod(alphaHighStr);
        } catch (...) {
            continue;  // fila corrupta, se salta
        }
        entries.push_back(e);
    }

    if (entries.empty() && outError && outError->empty()) {
        *outError = "Manifest leido pero sin filas validas: " + path;
    }

    return entries;
}

}  // namespace ManifestIO
