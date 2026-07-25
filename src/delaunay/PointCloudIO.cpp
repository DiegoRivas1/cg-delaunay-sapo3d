#include "PointCloudIO.hpp"
#include <fstream>
#include <sstream>

namespace PointCloudIO {

std::vector<Vec3> loadXYZ(const std::string& path, std::string* outError) {
    std::vector<Vec3> points;

    std::ifstream file(path);
    if (!file.is_open()) {
        if (outError) *outError = "No se pudo abrir: " + path;
        return points;
    }

    std::string line;
    int lineNum = 0;
    int badLines = 0;
    while (std::getline(file, line)) {
        ++lineNum;
        if (line.empty() || line[0] == '#') continue;

        std::istringstream iss(line);
        double x, y, z;
        if (iss >> x >> y >> z) {
            points.emplace_back(x, y, z);
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
