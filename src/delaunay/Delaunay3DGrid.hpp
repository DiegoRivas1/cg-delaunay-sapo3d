#pragma once
#include "Delaunay3D.hpp"

// Variante de Bowyer-Watson 3D acelerada con una grilla espacial sobre
// las circunesferas de los tetraedros vigentes, en vez de escanear
// TODOS los tetraedros en cada insercion (que es lo que hace
// BowyerWatson3D::triangulate()).
//
// Ver Delaunay3DGrid.cpp para el diseño (insercion "gorda" en todas las
// celdas que toca cada circunesfera; consulta O(1) amortizado en la
// celda del punto que se esta insertando). Reusa BowyerWatson3D para
// todo lo posterior a la triangulacion (filterByAlpha,
// filterByAdaptiveAlpha, boundaryFaces son identicos para ambas
// variantes, ya que solo dependen de la LISTA de tetraedros resultante,
// no de como se llego a ella).
class BowyerWatsonGrid {
public:
    static std::vector<Tetrahedron> triangulate(const std::vector<Vec3>& points);
};
