#include "MeshBuilder.h"
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <set>
#include <utility>

namespace MeshBuilder {

GpuMesh buildSurfaceMesh(const std::vector<Vec3>& points, const std::vector<Face>& faces) {
    std::vector<float> data;
    data.reserve(faces.size() * 3 * 6);

    for (const auto& f : faces) {
        glm::vec3 a(points[f.v[0]].x, points[f.v[0]].y, points[f.v[0]].z);
        glm::vec3 b(points[f.v[1]].x, points[f.v[1]].y, points[f.v[1]].z);
        glm::vec3 c(points[f.v[2]].x, points[f.v[2]].y, points[f.v[2]].z);
        glm::vec3 n = glm::normalize(glm::cross(b - a, c - a));

        for (const glm::vec3& p : {a, b, c}) {
            data.push_back(p.x); data.push_back(p.y); data.push_back(p.z);
            data.push_back(n.x); data.push_back(n.y); data.push_back(n.z);
        }
    }

    GpuMesh mesh;
    mesh.vertexCount = static_cast<int>(faces.size() * 3);

    glGenVertexArrays(1, &mesh.vao);
    glGenBuffers(1, &mesh.vbo);
    glBindVertexArray(mesh.vao);
    glBindBuffer(GL_ARRAY_BUFFER, mesh.vbo);
    glBufferData(GL_ARRAY_BUFFER, data.size() * sizeof(float), data.data(), GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));

    glBindVertexArray(0);
    return mesh;
}

GpuMesh buildTetraEdgesMesh(const std::vector<Vec3>& points, const std::vector<Tetrahedron>& tets) {
    std::set<std::pair<int, int>> uniqueEdges;
    auto addEdge = [&](int a, int b) {
        if (a > b) std::swap(a, b);
        uniqueEdges.insert({a, b});
    };

    for (const auto& t : tets) {
        addEdge(t.v[0], t.v[1]); addEdge(t.v[0], t.v[2]); addEdge(t.v[0], t.v[3]);
        addEdge(t.v[1], t.v[2]); addEdge(t.v[1], t.v[3]); addEdge(t.v[2], t.v[3]);
    }

    std::vector<float> data;
    data.reserve(uniqueEdges.size() * 6);
    for (const auto& e : uniqueEdges) {
        const Vec3& a = points[e.first];
        const Vec3& b = points[e.second];
        data.push_back((float)a.x); data.push_back((float)a.y); data.push_back((float)a.z);
        data.push_back((float)b.x); data.push_back((float)b.y); data.push_back((float)b.z);
    }

    GpuMesh mesh;
    mesh.vertexCount = static_cast<int>(uniqueEdges.size() * 2);

    glGenVertexArrays(1, &mesh.vao);
    glGenBuffers(1, &mesh.vbo);
    glBindVertexArray(mesh.vao);
    glBindBuffer(GL_ARRAY_BUFFER, mesh.vbo);
    glBufferData(GL_ARRAY_BUFFER, data.size() * sizeof(float), data.data(), GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glBindVertexArray(0);
    return mesh;
}

void deleteMesh(GpuMesh& m) {
    if (m.vbo) glDeleteBuffers(1, &m.vbo);
    if (m.vao) glDeleteVertexArrays(1, &m.vao);
    m = GpuMesh{};
}

}  // namespace MeshBuilder
