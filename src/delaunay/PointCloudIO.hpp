#pragma once
#include "Vec3.hpp"
#include <vector>
#include <string>

// Lector simple de archivos .xyz (texto plano, una fila "x y z" o
// "x y z local_scale" por punto, lineas que empiezan con '#' se
// ignoran). Formato de salida de scripts/extract_surface.py.
namespace PointCloudIO {

// Devuelve un vector vacio si el archivo no existe o no se pudo parsear.
// 'outError' (opcional) recibe un mensaje legible para mostrar en UI.
std::vector<Vec3> loadXYZ(const std::string& path, std::string* outError = nullptr);

// Version que ademas devuelve la 4ta columna (escala/densidad local por
// punto) si esta presente en el archivo. Si el archivo es del formato
// viejo (solo x y z, sin 4ta columna), outLocalScale queda del mismo
// tamano que los puntos pero relleno con 0.0 -- el llamador debe tratar
// 0.0 como "no disponible" y usar un fallback (ej. alpha global).
std::vector<Vec3> loadXYZWithScale(const std::string& path,
                                    std::vector<double>& outLocalScale,
                                    std::string* outError = nullptr);

}  // namespace PointCloudIO
