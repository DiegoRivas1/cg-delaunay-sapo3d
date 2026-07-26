#include "TetraCache.hpp"
#include <fstream>
#include <cstdint>
#include <cstring>

namespace TetraCache {

namespace {
constexpr char kMagic[4] = {'T', 'E', 'T', 'C'};
constexpr int32_t kVersion = 1;
}  // namespace

std::string cachePath(const std::string& cacheDir, const std::string& organName, int numPoints) {
    return cacheDir + "/" + organName + "_" + std::to_string(numPoints) + ".bin";
}

bool exists(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    return f.good();
}

bool save(const std::string& path, const std::vector<Vec3>& points,
          const std::vector<Tetrahedron>& tets) {
    std::ofstream out(path, std::ios::binary);
    if (!out.is_open()) return false;

    out.write(kMagic, 4);
    out.write(reinterpret_cast<const char*>(&kVersion), sizeof(kVersion));

    int32_t numPoints = static_cast<int32_t>(points.size());
    int32_t numTets = static_cast<int32_t>(tets.size());
    out.write(reinterpret_cast<const char*>(&numPoints), sizeof(numPoints));
    out.write(reinterpret_cast<const char*>(&numTets), sizeof(numTets));

    for (const auto& p : points) {
        out.write(reinterpret_cast<const char*>(&p.x), sizeof(double));
        out.write(reinterpret_cast<const char*>(&p.y), sizeof(double));
        out.write(reinterpret_cast<const char*>(&p.z), sizeof(double));
    }

    for (const auto& t : tets) {
        int32_t idx[4] = {t.v[0], t.v[1], t.v[2], t.v[3]};
        out.write(reinterpret_cast<const char*>(idx), sizeof(idx));
        out.write(reinterpret_cast<const char*>(&t.center.x), sizeof(double));
        out.write(reinterpret_cast<const char*>(&t.center.y), sizeof(double));
        out.write(reinterpret_cast<const char*>(&t.center.z), sizeof(double));
        out.write(reinterpret_cast<const char*>(&t.radiusSq), sizeof(double));
    }

    return out.good();
}

bool load(const std::string& path, std::vector<Vec3>& points, std::vector<Tetrahedron>& tets) {
    std::ifstream in(path, std::ios::binary);
    if (!in.is_open()) return false;

    char magic[4];
    in.read(magic, 4);
    if (!in.good() || std::memcmp(magic, kMagic, 4) != 0) return false;

    int32_t version = 0;
    in.read(reinterpret_cast<char*>(&version), sizeof(version));
    if (!in.good() || version != kVersion) return false;

    int32_t numPoints = 0, numTets = 0;
    in.read(reinterpret_cast<char*>(&numPoints), sizeof(numPoints));
    in.read(reinterpret_cast<char*>(&numTets), sizeof(numTets));
    if (!in.good() || numPoints < 0 || numTets < 0) return false;

    points.clear();
    points.reserve(numPoints);
    for (int32_t i = 0; i < numPoints; ++i) {
        double x, y, z;
        in.read(reinterpret_cast<char*>(&x), sizeof(double));
        in.read(reinterpret_cast<char*>(&y), sizeof(double));
        in.read(reinterpret_cast<char*>(&z), sizeof(double));
        if (!in.good()) return false;
        points.emplace_back(x, y, z);
    }

    tets.clear();
    tets.reserve(numTets);
    for (int32_t i = 0; i < numTets; ++i) {
        int32_t idx[4];
        in.read(reinterpret_cast<char*>(idx), sizeof(idx));
        Tetrahedron t;
        t.v = {idx[0], idx[1], idx[2], idx[3]};
        in.read(reinterpret_cast<char*>(&t.center.x), sizeof(double));
        in.read(reinterpret_cast<char*>(&t.center.y), sizeof(double));
        in.read(reinterpret_cast<char*>(&t.center.z), sizeof(double));
        in.read(reinterpret_cast<char*>(&t.radiusSq), sizeof(double));
        if (!in.good()) return false;
        tets.push_back(t);
    }

    return true;
}

}  // namespace TetraCache
