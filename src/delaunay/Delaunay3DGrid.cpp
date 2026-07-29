#include "Delaunay3DGrid.hpp"
#include <algorithm>
#include <cmath>
#include <unordered_map>

// ---------------------------------------------------------------------
// Helpers pequenos, duplicados a proposito de Delaunay3D.cpp (son
// privados alli). Mantenerlos separados evita tocar la clase original
// ya validada -- cero riesgo de romper BowyerWatson3D mientras se
// experimenta con la aceleracion espacial.
// ---------------------------------------------------------------------
namespace {

bool solve3x3(const double M[3][3], const double b[3], Vec3& out) {
    auto det3 = [](const double m[3][3]) {
        return m[0][0] * (m[1][1] * m[2][2] - m[1][2] * m[2][1])
             - m[0][1] * (m[1][0] * m[2][2] - m[1][2] * m[2][0])
             + m[0][2] * (m[1][0] * m[2][1] - m[1][1] * m[2][0]);
    };
    double det = det3(M);
    if (std::fabs(det) < 1e-12) return false;

    double Mx[3][3] = {{b[0], M[0][1], M[0][2]}, {b[1], M[1][1], M[1][2]}, {b[2], M[2][1], M[2][2]}};
    double My[3][3] = {{M[0][0], b[0], M[0][2]}, {M[1][0], b[1], M[1][2]}, {M[2][0], b[2], M[2][2]}};
    double Mz[3][3] = {{M[0][0], M[0][1], b[0]}, {M[1][0], M[1][1], b[1]}, {M[2][0], M[2][1], b[2]}};

    out.x = det3(Mx) / det;
    out.y = det3(My) / det;
    out.z = det3(Mz) / det;
    return true;
}

bool computeCircumsphere(const Vec3& a, const Vec3& b, const Vec3& c, const Vec3& d,
                          Vec3& outCenter, double& outRadiusSq) {
    Vec3 d1 = b - a, d2 = c - a, d3 = d - a;
    double M[3][3] = {{d1.x, d1.y, d1.z}, {d2.x, d2.y, d2.z}, {d3.x, d3.y, d3.z}};
    double rhs[3] = {0.5 * (b.lengthSq() - a.lengthSq()),
                      0.5 * (c.lengthSq() - a.lengthSq()),
                      0.5 * (d.lengthSq() - a.lengthSq())};
    if (!solve3x3(M, rhs, outCenter)) return false;
    outRadiusSq = (outCenter - a).lengthSq();
    return true;
}

bool inCircumsphere(const Tetrahedron& t, const Vec3& p, double epsilonRel) {
    double distSq = (p - t.center).lengthSq();
    return distSq < t.radiusSq * (1.0 - epsilonRel);
}

std::array<Face, 4> facesOf(const Tetrahedron& t) {
    return {Face(t.v[0], t.v[1], t.v[2]), Face(t.v[0], t.v[1], t.v[3]),
            Face(t.v[0], t.v[2], t.v[3]), Face(t.v[1], t.v[2], t.v[3])};
}

Tetrahedron makeSuperTetrahedron(const std::vector<Vec3>& points, std::vector<Vec3>& allPoints) {
    Vec3 minP = points[0], maxP = points[0];
    for (const auto& p : points) {
        minP.x = std::min(minP.x, p.x); minP.y = std::min(minP.y, p.y); minP.z = std::min(minP.z, p.z);
        maxP.x = std::max(maxP.x, p.x); maxP.y = std::max(maxP.y, p.y); maxP.z = std::max(maxP.z, p.z);
    }
    Vec3 center = (minP + maxP) * 0.5;
    double dx = maxP.x - minP.x, dy = maxP.y - minP.y, dz = maxP.z - minP.z;
    double diag = std::sqrt(dx * dx + dy * dy + dz * dz);
    if (diag < 1e-9) diag = 1.0;
    double s = diag * 5.0;

    Vec3 p0 = center + Vec3(1, 1, 1) * s;
    Vec3 p1 = center + Vec3(1, -1, -1) * s;
    Vec3 p2 = center + Vec3(-1, 1, -1) * s;
    Vec3 p3 = center + Vec3(-1, -1, 1) * s;

    int base = static_cast<int>(allPoints.size());
    allPoints.push_back(p0); allPoints.push_back(p1); allPoints.push_back(p2); allPoints.push_back(p3);

    Tetrahedron t;
    t.v = {base, base + 1, base + 2, base + 3};
    computeCircumsphere(p0, p1, p2, p3, t.center, t.radiusSq);
    return t;
}

// ---------------------------------------------------------------------
// Grilla de "insercion gorda": cada tetraedro se guarda en TODAS las
// celdas que su circunesfera toca (bounding box del circumsphere). La
// consulta para un punto p solo necesita mirar la celda que contiene a
// p -- si un tetraedro tiene a p dentro de su circunesfera, su
// circunesfera necesariamente toca la celda de p, asi que ya esta
// insertado ahi. Garantiza no perder ningun candidato SIN depender de
// ninguna cota global de radio (a diferencia de un primer intento con
// una cota 'Rmax' monotona, que se probo y fallo con nubes grandes).
// El costo de insertar un tetraedro es proporcional a cuantas celdas
// toca su circunesfera, que para la gran mayoria (tetraedros
// "normales", ya localizados) es un punado; solo los pocos tetraedros
// grandes de las primeras inserciones tocan muchas celdas, y son pocos
// por naturaleza del algoritmo.
class FatGrid {
public:
    explicit FatGrid(double cellSize) : cellSize_(cellSize > 1e-9 ? cellSize : 1.0) {}

    void insert(int idx, const Vec3& center, double radius) {
        auto range = cellRange(center, radius);
        for (int cx = range[0][0]; cx <= range[1][0]; ++cx)
            for (int cy = range[0][1]; cy <= range[1][1]; ++cy)
                for (int cz = range[0][2]; cz <= range[1][2]; ++cz)
                    cells_[packKey(cx, cy, cz)].push_back(idx);
    }

    void remove(int idx, const Vec3& center, double radius) {
        auto range = cellRange(center, radius);
        for (int cx = range[0][0]; cx <= range[1][0]; ++cx) {
            for (int cy = range[0][1]; cy <= range[1][1]; ++cy) {
                for (int cz = range[0][2]; cz <= range[1][2]; ++cz) {
                    auto it = cells_.find(packKey(cx, cy, cz));
                    if (it == cells_.end()) continue;
                    auto& bucket = it->second;
                    for (size_t i = 0; i < bucket.size(); ++i) {
                        if (bucket[i] == idx) { bucket[i] = bucket.back(); bucket.pop_back(); break; }
                    }
                }
            }
        }
    }

    // Solo la celda que contiene a 'p' -- ver justificacion arriba.
    void queryCell(const Vec3& p, std::vector<int>& out) const {
        auto c = cellCoord(p);
        auto it = cells_.find(packKey(c[0], c[1], c[2]));
        if (it != cells_.end()) out.insert(out.end(), it->second.begin(), it->second.end());
    }

private:
    double cellSize_;
    std::unordered_map<int64_t, std::vector<int>> cells_;

    std::array<int, 3> cellCoord(const Vec3& p) const {
        return {static_cast<int>(std::floor(p.x / cellSize_)),
                static_cast<int>(std::floor(p.y / cellSize_)),
                static_cast<int>(std::floor(p.z / cellSize_))};
    }

    std::array<std::array<int, 3>, 2> cellRange(const Vec3& center, double radius) const {
        Vec3 lo3(center.x - radius, center.y - radius, center.z - radius);
        Vec3 hi3(center.x + radius, center.y + radius, center.z + radius);
        return {cellCoord(lo3), cellCoord(hi3)};
    }

    static int64_t packKey(int cx, int cy, int cz) {
        constexpr int64_t OFF = 1 << 20;
        return (static_cast<int64_t>(cx + OFF) << 42) |
               (static_cast<int64_t>(cy + OFF) << 21) |
               static_cast<int64_t>(cz + OFF);
    }
};

}  // namespace

std::vector<Tetrahedron> BowyerWatsonGrid::triangulate(const std::vector<Vec3>& points) {
    std::vector<Vec3> allPoints = points;
    Tetrahedron superTet = makeSuperTetrahedron(points, allPoints);

    std::vector<Tetrahedron> tets;
    std::vector<char> alive;
    tets.reserve(points.size() * 8);
    alive.reserve(points.size() * 8);
    tets.push_back(superTet);
    alive.push_back(1);

    const int n = static_cast<int>(points.size());

    auto touchesSuper = [&superTet](const Tetrahedron& t) {
        for (int vi : t.v)
            for (int sv : superTet.v)
                if (vi == sv) return true;
        return false;
    };

    Vec3 minP = points[0], maxP = points[0];
    for (const auto& p : points) {
        minP.x = std::min(minP.x, p.x); minP.y = std::min(minP.y, p.y); minP.z = std::min(minP.z, p.z);
        maxP.x = std::max(maxP.x, p.x); maxP.y = std::max(maxP.y, p.y); maxP.z = std::max(maxP.z, p.z);
    }
    double diag = (maxP - minP).length();
    if (diag < 1e-9) diag = 1.0;
    double cellSize = diag / std::cbrt(static_cast<double>(std::max(n, 1)));
    if (cellSize < 1e-9) cellSize = diag;

    FatGrid grid(cellSize);

    // Tetraedros que todavia tocan algun vertice del super-tetraedro:
    // su circunesfera puede ser enorme (harian la insercion "gorda"
    // carisima), asi que se manejan aparte, revisados exhaustivamente
    // via una lista chica que se va vaciando a medida que avanza la
    // triangulacion.
    std::vector<int> superShell;
    superShell.push_back(0);

    auto eraseFromVec = [](std::vector<int>& v, int value) {
        for (size_t i = 0; i < v.size(); ++i) {
            if (v[i] == value) { v[i] = v.back(); v.pop_back(); return true; }
        }
        return false;
    };

    const double epsilonRel = 1e-9;
    std::vector<int> candidates;
    candidates.reserve(64);

    for (int i = 0; i < n; ++i) {
        const Vec3& p = allPoints[i];

        candidates.clear();
        grid.queryCell(p, candidates);

        std::vector<int> badIdx;
        badIdx.reserve(8 + superShell.size());

        for (int idx : superShell) {
            if (alive[static_cast<size_t>(idx)] && inCircumsphere(tets[static_cast<size_t>(idx)], p, epsilonRel)) {
                badIdx.push_back(idx);
            }
        }
        for (int idx : candidates) {
            if (alive[static_cast<size_t>(idx)] && inCircumsphere(tets[static_cast<size_t>(idx)], p, epsilonRel)) {
                badIdx.push_back(idx);
            }
        }

        std::vector<Face> allFaces;
        allFaces.reserve(badIdx.size() * 4);
        for (int idx : badIdx) {
            for (const auto& f : facesOf(tets[static_cast<size_t>(idx)])) allFaces.push_back(f);
        }

        std::vector<Face> boundary;
        boundary.reserve(allFaces.size());
        for (size_t a = 0; a < allFaces.size(); ++a) {
            bool shared = false;
            for (size_t b = 0; b < allFaces.size(); ++b) {
                if (a != b && allFaces[a] == allFaces[b]) { shared = true; break; }
            }
            if (!shared) boundary.push_back(allFaces[a]);
        }

        for (int idx : badIdx) {
            alive[static_cast<size_t>(idx)] = 0;
            if (!eraseFromVec(superShell, idx)) {
                const auto& t = tets[static_cast<size_t>(idx)];
                grid.remove(idx, t.center, std::sqrt(t.radiusSq));
            }
        }

        for (const auto& f : boundary) {
            Tetrahedron nt;
            nt.v = {f.v[0], f.v[1], f.v[2], i};
            Vec3 c;
            double r2;
            if (computeCircumsphere(allPoints[nt.v[0]], allPoints[nt.v[1]], allPoints[nt.v[2]], allPoints[nt.v[3]], c, r2)) {
                nt.center = c;
                nt.radiusSq = r2;

                int newIdx = static_cast<int>(tets.size());
                tets.push_back(nt);
                alive.push_back(1);

                if (touchesSuper(nt)) {
                    superShell.push_back(newIdx);
                } else {
                    grid.insert(newIdx, nt.center, std::sqrt(r2));
                }
            }
        }
    }

    std::vector<Tetrahedron> result;
    result.reserve(tets.size());
    for (size_t idx = 0; idx < tets.size(); ++idx) {
        if (!alive[idx]) continue;
        if (!touchesSuper(tets[idx])) result.push_back(tets[idx]);
    }
    return result;
}
