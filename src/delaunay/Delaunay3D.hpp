#pragma once
#include "Vec3.hpp"
#include <vector>
#include <array>

// Tetraedro representado como 4 indices hacia el arreglo de puntos
// (incluye los puntos del super-tetraedro mientras se construye).
struct Tetrahedron {
    std::array<int, 4> v;

    // Circunesfera cacheada para no recalcular en cada test de punto.
    Vec3 center;
    double radiusSq = 0.0;
};

// Cara triangular no orientada, usada para detectar el "hueco" que deja
// la eliminacion de tetraedros invalidos.
struct Face {
    std::array<int, 3> v;

    Face(int a, int b, int c) : v{a, b, c} {
        // Orden canonico para poder comparar caras sin importar el
        // orden de insercion.
        if (v[0] > v[1]) std::swap(v[0], v[1]);
        if (v[1] > v[2]) std::swap(v[1], v[2]);
        if (v[0] > v[1]) std::swap(v[0], v[1]);
    }

    bool operator==(const Face& o) const { return v == o.v; }
};

class BowyerWatson3D {
public:
    // points: nube de puntos de entrada.
    // Devuelve los tetraedros finales (indices referidos a 'points',
    // ya SIN los vertices del super-tetraedro).
    static std::vector<Tetrahedron> triangulate(const std::vector<Vec3>& points);

    // Dado el resultado de triangulate(), extrae las caras de frontera
    // (las que pertenecen a un solo tetraedro) = superficie externa /
    // envolvente convexa de la nube de puntos.
    static std::vector<Face> boundaryFaces(const std::vector<Tetrahedron>& tets);

    // Alpha-shape: descarta los tetraedros cuya circunesfera sea mayor
    // a 'alphaRadius'. Necesario para reconstruir formas NO convexas
    // (organos con concavidades) -- boundaryFaces() sola solo da el
    // casco convexo, que no sirve para eso.
    static std::vector<Tetrahedron> filterByAlpha(const std::vector<Tetrahedron>& tets,
                                                    double alphaRadius);

    // Alpha ADAPTATIVO: en vez de un unico radio global, cada tetraedro
    // se compara contra un umbral calculado a partir de sus propios 4
    // vertices: alpha_local(t) = multiplier * promedio(localScale[v] :
    // v en t.v). 'localScale' es la densidad local por punto (ver
    // scripts/tiff_utils.py::per_point_local_scale) -- mismo tamano que
    // 'points'. Reconstruye mejor zonas de densidad no uniforme (cuerpo
    // ancho + extremidades finas del mismo organo) que un alpha global.
    static std::vector<Tetrahedron> filterByAdaptiveAlpha(
        const std::vector<Tetrahedron>& tets,
        const std::vector<double>& localScale,
        double multiplier);

private:
    static bool computeCircumsphere(const Vec3& a, const Vec3& b,
                                     const Vec3& c, const Vec3& d,
                                     Vec3& outCenter, double& outRadiusSq);

    static bool inCircumsphere(const Tetrahedron& t, const Vec3& p,
                                const std::vector<Vec3>& allPoints,
                                double epsilon);

    static std::array<Face, 4> facesOf(const Tetrahedron& t);

    static Tetrahedron makeSuperTetrahedron(const std::vector<Vec3>& points,
                                             std::vector<Vec3>& allPoints);
};
