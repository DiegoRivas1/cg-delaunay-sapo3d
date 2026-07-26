#pragma once
#include "Delaunay3D.hpp"
#include <string>
#include <vector>

// Guarda/carga en disco el resultado de una triangulacion completa
// (puntos + tetraedros, con su circunesfera ya calculada) para no tener
// que re-triangular cada vez que se abre la app. Formato binario simple,
// propio del proyecto (no busca ser portable entre plataformas ni
// versiones -- si el algoritmo cambia, hay que re-generar los caches).
namespace TetraCache {

// Nombre de archivo sugerido y consistente: <dir>/<organo>_<numPuntos>.bin
std::string cachePath(const std::string& cacheDir, const std::string& organName, int numPoints);

bool save(const std::string& path, const std::vector<Vec3>& points,
          const std::vector<Tetrahedron>& tets);

// Devuelve false si el archivo no existe, esta corrupto, o no matchea version.
bool load(const std::string& path, std::vector<Vec3>& points, std::vector<Tetrahedron>& tets);

bool exists(const std::string& path);

}  // namespace TetraCache
