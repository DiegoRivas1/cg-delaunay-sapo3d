#include "Delaunay3D.hpp"
#include <algorithm>
#include <cmath>
#include <unordered_map>

namespace {

// Resuelve M * x = b para un sistema 3x3 via regla de Cramer.
bool solve3x3(const double M[3][3], const double b[3], Vec3& out) {
    auto det3 = [](const double m[3][3]) {
        return m[0][0] * (m[1][1] * m[2][2] - m[1][2] * m[2][1])
             - m[0][1] * (m[1][0] * m[2][2] - m[1][2] * m[2][0])
             + m[0][2] * (m[1][0] * m[2][1] - m[1][1] * m[2][0]);
    };

    double det = det3(M);
    if (std::fabs(det) < 1e-12) return false;  // puntos casi coplanares

    double Mx[3][3] = {{b[0], M[0][1], M[0][2]},
                        {b[1], M[1][1], M[1][2]},
                        {b[2], M[2][1], M[2][2]}};
    double My[3][3] = {{M[0][0], b[0], M[0][2]},
                        {M[1][0], b[1], M[1][2]},
                        {M[2][0], b[2], M[2][2]}};
    double Mz[3][3] = {{M[0][0], M[0][1], b[0]},
                        {M[1][0], M[1][1], b[1]},
                        {M[2][0], M[2][1], b[2]}};

    out.x = det3(Mx) / det;
    out.y = det3(My) / det;
    out.z = det3(Mz) / det;
    return true;
}

}  // namespace

bool BowyerWatson3D::computeCircumsphere(const Vec3& a, const Vec3& b,
                                          const Vec3& c, const Vec3& d,
                                          Vec3& outCenter, double& outRadiusSq) {
    Vec3 d1 = b - a, d2 = c - a, d3 = d - a;

    double M[3][3] = {{d1.x, d1.y, d1.z},
                       {d2.x, d2.y, d2.z},
                       {d3.x, d3.y, d3.z}};
    double rhs[3] = {0.5 * (b.lengthSq() - a.lengthSq()),
                      0.5 * (c.lengthSq() - a.lengthSq()),
                      0.5 * (d.lengthSq() - a.lengthSq())};

    if (!solve3x3(M, rhs, outCenter)) return false;

    outRadiusSq = (outCenter - a).lengthSq();
    return true;
}

bool BowyerWatson3D::inCircumsphere(const Tetrahedron& t, const Vec3& p,
                                     const std::vector<Vec3>&, double epsilonRel) {
    double distSq = (p - t.center).lengthSq();
    double threshold = t.radiusSq * (1.0 - epsilonRel);
    return distSq < threshold;
}

std::array<Face, 4> BowyerWatson3D::facesOf(const Tetrahedron& t) {
    return {Face(t.v[0], t.v[1], t.v[2]), Face(t.v[0], t.v[1], t.v[3]),
            Face(t.v[0], t.v[2], t.v[3]), Face(t.v[1], t.v[2], t.v[3])};
}

Tetrahedron BowyerWatson3D::makeSuperTetrahedron(const std::vector<Vec3>& points,
                                                  std::vector<Vec3>& allPoints) {
    Vec3 minP = points[0], maxP = points[0];
    for (const auto& p : points) {
        minP.x = std::min(minP.x, p.x);
        minP.y = std::min(minP.y, p.y);
        minP.z = std::min(minP.z, p.z);
        maxP.x = std::max(maxP.x, p.x);
        maxP.y = std::max(maxP.y, p.y);
        maxP.z = std::max(maxP.z, p.z);
    }

    Vec3 center = (minP + maxP) * 0.5;
    double dx = maxP.x - minP.x, dy = maxP.y - minP.y, dz = maxP.z - minP.z;
    double diag = std::sqrt(dx * dx + dy * dy + dz * dz);
    if (diag < 1e-9) diag = 1.0;
    double s = diag * 5.0;  // margen amplio para que TODOS los puntos queden dentro

    // Tetraedro regular (vertices alternados de un cubo), escalado y centrado.
    Vec3 p0 = center + Vec3(1, 1, 1) * s;
    Vec3 p1 = center + Vec3(1, -1, -1) * s;
    Vec3 p2 = center + Vec3(-1, 1, -1) * s;
    Vec3 p3 = center + Vec3(-1, -1, 1) * s;

    int base = static_cast<int>(allPoints.size());
    allPoints.push_back(p0);
    allPoints.push_back(p1);
    allPoints.push_back(p2);
    allPoints.push_back(p3);

    Tetrahedron t;
    t.v = {base, base + 1, base + 2, base + 3};
    computeCircumsphere(p0, p1, p2, p3, t.center, t.radiusSq);
    return t;
}

std::vector<Tetrahedron> BowyerWatson3D::triangulate(const std::vector<Vec3>& points) {
    std::vector<Vec3> allPoints = points;
    Tetrahedron superTet = makeSuperTetrahedron(points, allPoints);

    std::vector<Tetrahedron> tets;
    tets.push_back(superTet);

    const double epsilonRel = 1e-9;
    const int n = static_cast<int>(points.size());

    for (int i = 0; i < n; ++i) {
        const Vec3& p = allPoints[i];

        std::vector<Tetrahedron> badTets;
        std::vector<Tetrahedron> goodTets;
        badTets.reserve(8);
        goodTets.reserve(tets.size());

        // Clasificar cada tetraedro (bueno/malo) es un chequeo de
        // circunesfera independiente por tetraedro -- se puede paralelizar
        // sin problemas de dependencia. Se guarda en un vector de flags y
        // se particiona secuencialmente despues (evita condiciones de
        // carrera al hacer push_back desde varios hilos).
        std::vector<char> isBad(tets.size(), 0);
        const long numTets = static_cast<long>(tets.size());
        // Umbral calibrado con mediciones reales (ver README/informe,
        // seccion Complejidad y optimizaciones): con la granularidad
        // actual (una region paralela por punto insertado), OpenMP
        // recien compensa el overhead de sincronizar hilos a partir de
        // ~100k tetraedros vigentes (~15-20k puntos insertados). Por
        // debajo de eso, la version secuencial es mas rapida -- que es
        // el caso de los 17 organos de este proyecto (maximo ~3000
        // puntos), asi que en la practica esto corre secuencial.
#pragma omp parallel for schedule(static) if (numTets > 100000)
        for (long ti = 0; ti < numTets; ++ti) {
            if (inCircumsphere(tets[static_cast<size_t>(ti)], p, allPoints, epsilonRel)) {
                isBad[static_cast<size_t>(ti)] = 1;
            }
        }
        for (size_t ti = 0; ti < tets.size(); ++ti) {
            if (isBad[ti]) badTets.push_back(tets[ti]);
            else goodTets.push_back(tets[ti]);
        }

        // Caras de los tetraedros invalidos. Las que se repiten son
        // internas (compartidas entre dos tetraedros malos); las que
        // aparecen una sola vez forman el borde del "hueco".
        std::vector<Face> allFaces;
        allFaces.reserve(badTets.size() * 4);
        for (const auto& t : badTets) {
            for (const auto& f : facesOf(t)) allFaces.push_back(f);
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

        tets = std::move(goodTets);

        for (const auto& f : boundary) {
            Tetrahedron nt;
            nt.v = {f.v[0], f.v[1], f.v[2], i};
            Vec3 c;
            double r2;
            if (computeCircumsphere(allPoints[nt.v[0]], allPoints[nt.v[1]],
                                     allPoints[nt.v[2]], allPoints[nt.v[3]], c, r2)) {
                nt.center = c;
                nt.radiusSq = r2;
                tets.push_back(nt);
            }
            // Si es degenerado (coplanar) se descarta; no deberia ocurrir
            // con nubes de puntos genericas.
        }
    }

    // Elimina cualquier tetraedro que aun toque un vertice del super-tetraedro.
    std::vector<Tetrahedron> result;
    result.reserve(tets.size());
    for (const auto& t : tets) {
        bool touchesSuper = false;
        for (int idx : t.v) {
            for (int sv : superTet.v) {
                if (idx == sv) { touchesSuper = true; break; }
            }
            if (touchesSuper) break;
        }
        if (!touchesSuper) result.push_back(t);
    }
    return result;
}

std::vector<Tetrahedron> BowyerWatson3D::filterByAdaptiveAlpha(
        const std::vector<Tetrahedron>& tets,
        const std::vector<double>& localScale,
        double multiplier) {
    std::vector<Tetrahedron> result;
    result.reserve(tets.size());

    for (const auto& t : tets) {
        double avgLocal = 0.0;
        for (int idx : t.v) {
            avgLocal += (idx >= 0 && static_cast<size_t>(idx) < localScale.size())
                            ? localScale[static_cast<size_t>(idx)]
                            : 0.0;
        }
        avgLocal *= 0.25;  // promedio de los 4 vertices

        double alphaLocal = multiplier * avgLocal;
        double alphaLocalSq = alphaLocal * alphaLocal;

        if (t.radiusSq <= alphaLocalSq) result.push_back(t);
    }
    return result;
}

std::vector<Tetrahedron> BowyerWatson3D::filterByAlpha(const std::vector<Tetrahedron>& tets,
                                                         double alphaRadius) {
    double alphaSq = alphaRadius * alphaRadius;
    std::vector<Tetrahedron> result;
    result.reserve(tets.size());
    for (const auto& t : tets) {
        if (t.radiusSq <= alphaSq) result.push_back(t);
    }
    return result;
}

namespace {
// Hash simple (FNV-1a) sobre los 3 indices ya ordenados canonicamente
// dentro de Face -- permite usar unordered_map en vez de comparar cada
// cara contra todas las demas.
struct FaceHash {
    size_t operator()(const Face& f) const {
        size_t h = 1469598103934665603ull;
        for (int v : f.v) {
            h ^= static_cast<size_t>(v);
            h *= 1099511628211ull;
        }
        return h;
    }
};
}  // namespace

std::vector<Face> BowyerWatson3D::boundaryFaces(const std::vector<Tetrahedron>& tets) {
    std::unordered_map<Face, int, FaceHash> faceCount;
    faceCount.reserve(tets.size() * 4);
    for (const auto& t : tets) {
        for (const auto& f : facesOf(t)) {
            faceCount[f]++;
        }
    }

    std::vector<Face> boundary;
    boundary.reserve(faceCount.size());
    for (const auto& [f, count] : faceCount) {
        if (count == 1) boundary.push_back(f);
    }
    return boundary;
}
