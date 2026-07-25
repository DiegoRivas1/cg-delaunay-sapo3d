#pragma once
#include "../delaunay/Delaunay3D.hpp"
#include <vector>

// Buffers GPU (VAO/VBO) para una malla ya construida. No posee los datos
// en CPU, solo referencia los handles de OpenGL.
struct GpuMesh {
    unsigned int vao = 0, vbo = 0;
    int vertexCount = 0;
};

namespace MeshBuilder {

// Malla de triangulos con normales por cara (flat shading), 6 floats por
// vertice (pos.xyz + normal.xyz), sin index buffer.
GpuMesh buildSurfaceMesh(const std::vector<Vec3>& points, const std::vector<Face>& faces);

// Aristas UNICAS de un conjunto de tetraedros (GL_LINES, solo posicion).
GpuMesh buildTetraEdgesMesh(const std::vector<Vec3>& points, const std::vector<Tetrahedron>& tets);

void deleteMesh(GpuMesh& mesh);

}  // namespace MeshBuilder
