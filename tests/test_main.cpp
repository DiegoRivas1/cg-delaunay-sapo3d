#include "delaunay/Delaunay3D.hpp"
#include <iostream>
#include <fstream>
#include <random>
#include <cmath>

// Genera puntos distribuidos ~uniformemente sobre una esfera (Fibonacci sphere).
std::vector<Vec3> generateSpherePoints(int n, double radius) {
    std::vector<Vec3> pts;
    pts.reserve(n);
    const double phi = M_PI * (3.0 - std::sqrt(5.0));  // angulo dorado

    for (int i = 0; i < n; ++i) {
        double y = 1.0 - (i / double(n - 1)) * 2.0;
        double r = std::sqrt(std::max(0.0, 1.0 - y * y));
        double theta = phi * i;
        double x = std::cos(theta) * r;
        double z = std::sin(theta) * r;
        pts.emplace_back(x * radius, y * radius, z * radius);
    }
    return pts;
}

// Toro (forma NO convexa: tiene un agujero en el medio). Sirve para
// probar que el alpha-shape reconstruye la concavidad real, en vez
// del casco convexo (que "taparia" el agujero).
std::vector<Vec3> generateTorusPoints(int nMajor, int nMinor, double R, double r) {
    std::vector<Vec3> pts;
    pts.reserve(nMajor * nMinor);
    for (int i = 0; i < nMajor; ++i) {
        double u = 2.0 * M_PI * i / nMajor;
        for (int j = 0; j < nMinor; ++j) {
            double v = 2.0 * M_PI * j / nMinor;
            double x = (R + r * std::cos(v)) * std::cos(u);
            double y = r * std::sin(v);
            double z = (R + r * std::cos(v)) * std::sin(u);
            pts.emplace_back(x, y, z);
        }
    }
    return pts;
}

// Verifica la propiedad de Delaunay: ningun punto de la nube debe caer
// estrictamente dentro de la circunesfera de ningun tetraedro resultante.
bool validateDelaunayProperty(const std::vector<Tetrahedron>& tets,
                               const std::vector<Vec3>& points) {
    const double eps = 1e-6;
    int violations = 0;

    for (const auto& t : tets) {
        for (size_t i = 0; i < points.size(); ++i) {
            // Saltar los vertices que forman parte del propio tetraedro
            if (static_cast<int>(i) == t.v[0] || static_cast<int>(i) == t.v[1] ||
                static_cast<int>(i) == t.v[2] || static_cast<int>(i) == t.v[3]) continue;

            double distSq = (points[i] - t.center).lengthSq();
            if (distSq < t.radiusSq - eps) {
                violations++;
            }
        }
    }

    std::cout << "  Violaciones de la propiedad Delaunay: " << violations << "\n";
    return violations == 0;
}

void exportOBJ(const std::string& path, const std::vector<Vec3>& points,
               const std::vector<Face>& faces) {
    std::ofstream out(path);
    for (const auto& p : points) {
        out << "v " << p.x << " " << p.y << " " << p.z << "\n";
    }
    for (const auto& f : faces) {
        // OBJ usa indices desde 1
        out << "f " << (f.v[0] + 1) << " " << (f.v[1] + 1) << " " << (f.v[2] + 1) << "\n";
    }
}

int main() {
    std::cout << "=== Test sintetico: Bowyer-Watson 3D ===\n\n";

    // --- Caso 1: puntos sobre una esfera (debe dar un casco convexo esferico) ---
    {
        std::cout << "[Caso 1] Esfera con 40 puntos\n";
        auto points = generateSpherePoints(40, 5.0);

        auto tets = BowyerWatson3D::triangulate(points);
        std::cout << "  Tetraedros generados: " << tets.size() << "\n";

        bool ok = validateDelaunayProperty(tets, points);
        std::cout << "  Propiedad Delaunay: " << (ok ? "OK" : "FALLO") << "\n";

        auto boundary = BowyerWatson3D::boundaryFaces(tets);
        std::cout << "  Caras de frontera (superficie): " << boundary.size() << "\n";

        // Chequeo topologico: para un casco convexo triangulado,
        // Euler: V - E + F = 2  =>  F = 2V - 4 (con E = 3F/2)
        int V = static_cast<int>(points.size());
        int expectedF = 2 * V - 4;
        std::cout << "  Caras esperadas (Euler, V=" << V << "): " << expectedF << "\n";

        exportOBJ("./sphere_surface.obj", points, boundary);
        std::cout << "  -> Exportado a sphere_surface.obj\n\n";
    }

    // --- Caso 2: puntos aleatorios dentro de un cubo (nube generica) ---
    {
        std::cout << "[Caso 2] Nube aleatoria, 60 puntos en un cubo\n";
        std::mt19937 rng(42);
        std::uniform_real_distribution<double> dist(-5.0, 5.0);

        std::vector<Vec3> points;
        for (int i = 0; i < 60; ++i) {
            points.emplace_back(dist(rng), dist(rng), dist(rng));
        }

        auto tets = BowyerWatson3D::triangulate(points);
        std::cout << "  Tetraedros generados: " << tets.size() << "\n";

        bool ok = validateDelaunayProperty(tets, points);
        std::cout << "  Propiedad Delaunay: " << (ok ? "OK" : "FALLO") << "\n";

        auto boundary = BowyerWatson3D::boundaryFaces(tets);
        std::cout << "  Caras de frontera (envolvente convexa): " << boundary.size() << "\n\n";

        exportOBJ("./cube_hull.obj", points, boundary);
        std::cout << "  -> Exportado a cube_hull.obj\n\n";
    }

    // --- Caso 3: toro (forma NO convexa) -> aqui se ve si alpha-shape sirve ---
    {
        std::cout << "[Caso 3] Toro (R=5, r=2), forma NO convexa\n";
        auto points = generateTorusPoints(24, 12, 5.0, 2.0);
        std::cout << "  Puntos: " << points.size() << "\n";

        auto tets = BowyerWatson3D::triangulate(points);
        std::cout << "  Tetraedros (triangulacion completa): " << tets.size() << "\n";

        bool ok = validateDelaunayProperty(tets, points);
        std::cout << "  Propiedad Delaunay: " << (ok ? "OK" : "FALLO") << "\n";

        auto convexBoundary = BowyerWatson3D::boundaryFaces(tets);
        std::cout << "  Caras de frontera SIN alpha (= casco convexo, tapa el agujero): "
                  << convexBoundary.size() << "\n";
        exportOBJ("./torus_convex.obj", points, convexBoundary);

        // El radio de circunesfera "normal" para este muestreo ronda la
        // distancia entre puntos vecinos; probamos un par de valores de
        // alpha para ver el efecto.
        for (double alpha : {1.5, 2.5, 4.0}) {
            auto filtered = BowyerWatson3D::filterByAlpha(tets, alpha);
            auto boundaryAlpha = BowyerWatson3D::boundaryFaces(filtered);
            std::cout << "  alpha=" << alpha << " -> tetraedros: " << filtered.size()
                      << ", caras de frontera: " << boundaryAlpha.size() << "\n";
            exportOBJ("./torus_alpha_" + std::to_string(alpha) + ".obj",
                       points, boundaryAlpha);
        }
        std::cout << "\n";
    }

    return 0;
}
