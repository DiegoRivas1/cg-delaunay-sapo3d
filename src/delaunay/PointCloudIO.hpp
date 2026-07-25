#pragma once
#include "Vec3.hpp"
#include <vector>
#include <string>

// Lector simple de archivos .xyz (texto plano, una fila "x y z" por punto,
// lineas que empiezan con '#' se ignoran). Formato de salida de
// scripts/extract_surface.py.
namespace PointCloudIO {

// Devuelve un vector vacio si el archivo no existe o no se pudo parsear.
// 'outError' (opcional) recibe un mensaje legible para mostrar en UI.
std::vector<Vec3> loadXYZ(const std::string& path, std::string* outError = nullptr);

}  // namespace PointCloudIO
