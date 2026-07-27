#include "PointCloudIO.hpp"
#include <fstream>
#include <sstream>

namespace PointCloudIO {

std::vector<Vec3> loadXYZ(const std::string& path, std::string* outError) {
    std::vector<double> unused;
    return loadXYZWithScale(path, unused, outError);
}

std::vector<Vec3> loadXYZWithScale(const std::string& path,
                                    std::vector<double>& outLocalScale,
                                    std::string* outError) {
    std::vector<Vec3> points;
    outLocalScale.clear();

    std::ifstream file(path);
    if (!file.is_open()) {
        if (outError) *outError = "No se pudo abrir: " + path;
        return points;
    }

    std::string line;
    int badLines = 0;
    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#') continue;

        std::istringstream iss(line);
        double x, y, z;
        double localScale = 0.0;  // 0.0 = no disponible (archivo formato viejo)
        if (iss >> x >> y >> z) {
            points.emplace_back(x, y, z);
            iss >> localScale;  // opcional: si no hay 4ta columna, queda en 0.0
            outLocalScale.push_back(localScale);
        } else {
            ++badLines;
        }
    }

    if (points.empty()) {
        if (outError) *outError = "Archivo leido pero sin puntos validos: " + path;
    } else if (badLines > 0 && outError) {
        *outError = "Advertencia: " + std::to_string(badLines) + " lineas invalidas ignoradas";
    }

    return points;
}

}  // namespace PointCloudIO
