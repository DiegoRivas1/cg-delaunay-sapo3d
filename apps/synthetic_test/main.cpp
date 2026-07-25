#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <imgui.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <vector>
#include <algorithm>
#include <cmath>
#include <random>
#include <string>
#include <cstring>

#include "core/Application.h"
#include "core/Shader.h"
#include "core/MeshBuilder.h"
#include "delaunay/IDelaunayStrategy.hpp"
#include "delaunay/PointCloudIO.hpp"

// ---------- Generacion de nubes de puntos sinteticas ----------

enum class CloudType { Sphere, RandomCube };

static std::vector<Vec3> generateSpherePoints(int n, double radius) {
    std::vector<Vec3> pts;
    pts.reserve(n);
    const double phi = M_PI * (3.0 - std::sqrt(5.0));
    for (int i = 0; i < n; ++i) {
        double y = 1.0 - (i / double(std::max(1, n - 1))) * 2.0;
        double r = std::sqrt(std::max(0.0, 1.0 - y * y));
        double theta = phi * i;
        pts.emplace_back(std::cos(theta) * r * radius, y * radius, std::sin(theta) * r * radius);
    }
    return pts;
}

static std::vector<Vec3> generateRandomCubePoints(int n, double halfExtent) {
    std::mt19937 rng(42);
    std::uniform_real_distribution<double> dist(-halfExtent, halfExtent);
    std::vector<Vec3> pts;
    pts.reserve(n);
    for (int i = 0; i < n; ++i) pts.emplace_back(dist(rng), dist(rng), dist(rng));
    return pts;
}

// Toro: forma NO convexa (tiene un agujero), sirve para validar alpha-shape.
static std::vector<Vec3> generateTorusPoints(int nMajor, int nMinor, double R, double r) {
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

// Distancia mediana al vecino mas cercano -> base para sugerir un alpha
// razonable sin tener que adivinar a mano (O(n^2), aceptable para las
// nubes de algunos miles de puntos que manejamos aca).
static double medianNearestNeighborDistance(const std::vector<Vec3>& points) {
    if (points.size() < 2) return 1.0;

    std::vector<double> nnDist(points.size(), 1e18);
    for (size_t i = 0; i < points.size(); ++i) {
        for (size_t j = 0; j < points.size(); ++j) {
            if (i == j) continue;
            double d = (points[i] - points[j]).length();
            if (d < nnDist[i]) nnDist[i] = d;
        }
    }
    std::sort(nnDist.begin(), nnDist.end());
    return nnDist[nnDist.size() / 2];
}

// ---------- Buffers GPU ----------
// (buildSurfaceMesh, buildTetraEdgesMesh, deleteMesh -> ver src/core/MeshBuilder.h)
using MeshBuilder::buildSurfaceMesh;
using MeshBuilder::buildTetraEdgesMesh;
using MeshBuilder::deleteMesh;

// ---------- main ----------

int main() {
    Application app(1280, 720, "cg-delaunay-sapo3d :: Test Sintetico");
    if (!app.ok()) return -1;

    Shader surfaceShader("shaders/basic.vert", "shaders/basic.frag");
    Shader lineShader("shaders/line.vert", "shaders/line.frag");

    BowyerWatsonStrategy strategy;

    // Estado de la escena, se reconstruye cuando el usuario cambia parametros.
    int numPoints = 40;
    int cloudTypeIdx = 0;  // 0=esfera, 1=cubo aleatorio, 2=toro, 3=archivo .xyz
    bool showSurface = true;
    bool showTetraEdges = false;
    bool autoRotate = true;
    float rotationAngle = 0.0f;
    bool useAlpha = false;
    float alphaRadius = 2.5f;
    char filePathBuf[256] = "data/spleen_points.xyz";
    std::string loadError;

    std::vector<Vec3> points;
    std::vector<Tetrahedron> tets;
    std::vector<Face> boundary;
    GpuMesh surfaceMesh, edgesMesh;

    std::string lastValidation = "(sin validar)";

    auto rebuild = [&]() {
        loadError.clear();

        if (cloudTypeIdx == 0) points = generateSpherePoints(numPoints, 5.0);
        else if (cloudTypeIdx == 1) points = generateRandomCubePoints(numPoints, 5.0);
        else if (cloudTypeIdx == 2) points = generateTorusPoints(24, 12, 5.0, 2.0);
        else points = PointCloudIO::loadXYZ(filePathBuf, &loadError);

        if (points.size() < 4) {
            if (loadError.empty()) loadError = "Se necesitan al menos 4 puntos";
            tets.clear();
            boundary.clear();
            deleteMesh(surfaceMesh);
            deleteMesh(edgesMesh);
            return;
        }

        tets = strategy.triangulate(points);

        std::vector<Tetrahedron> tetsForSurface = tets;
        if (useAlpha) tetsForSurface = BowyerWatson3D::filterByAlpha(tets, alphaRadius);

        boundary = BowyerWatson3D::boundaryFaces(tetsForSurface);

        deleteMesh(surfaceMesh);
        deleteMesh(edgesMesh);
        surfaceMesh = buildSurfaceMesh(points, boundary);
        edgesMesh = buildTetraEdgesMesh(points, tetsForSurface);

        lastValidation = "(sin validar)";
    };

    rebuild();

    app.run([&](float dt) {
        if (autoRotate) rotationAngle += dt * 20.0f;  // grados/seg

        // --- Panel ImGui ---
        ImGui::Begin("Delaunay 3D - Controles");
        ImGui::Text("Estrategia: %s", strategy.name());
        ImGui::Separator();

        const char* cloudNames[] = {"Esfera (Fibonacci)", "Cubo aleatorio", "Toro (no convexo)",
                                     "Cargar desde archivo (.xyz)"};
        bool dirty = false;
        dirty |= ImGui::Combo("Nube de puntos", &cloudTypeIdx, cloudNames, 4);

        if (cloudTypeIdx == 0 || cloudTypeIdx == 1) {
            dirty |= ImGui::SliderInt("Cantidad de puntos", &numPoints, 4, 300);
        } else if (cloudTypeIdx == 2) {
            ImGui::TextDisabled("Toro: 24x12 = 288 puntos fijos");
        } else {
            ImGui::InputText("Ruta .xyz", filePathBuf, sizeof(filePathBuf));
            if (ImGui::Button("Cargar")) dirty = true;
            if (!loadError.empty()) {
                ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "%s", loadError.c_str());
            }
        }
        if (ImGui::Button("Regenerar")) dirty = true;

        ImGui::Separator();
        dirty |= ImGui::Checkbox("Usar alpha-shape (necesario para formas no convexas)", &useAlpha);
        if (useAlpha) {
            dirty |= ImGui::SliderFloat("Alpha (radio max. circunesfera)", &alphaRadius, 0.1f, 20.0f);
            if (ImGui::Button("Auto-alpha (2.5x vecino mas cercano)") && points.size() >= 2) {
                alphaRadius = static_cast<float>(2.5 * medianNearestNeighborDistance(points));
                dirty = true;
            }
        }
        if (dirty) rebuild();

        ImGui::Separator();
        ImGui::Checkbox("Mostrar superficie (caras de frontera)", &showSurface);
        ImGui::Checkbox("Mostrar aristas de tetraedros", &showTetraEdges);
        ImGui::Checkbox("Auto-rotar", &autoRotate);

        ImGui::Separator();
        ImGui::Text("Puntos: %d", (int)points.size());
        ImGui::Text("Tetraedros (triangulacion completa): %d", (int)tets.size());
        if (useAlpha) {
            std::vector<Tetrahedron> tetsForSurface = BowyerWatson3D::filterByAlpha(tets, alphaRadius);
            ImGui::Text("Tetraedros tras alpha-shape: %d", (int)tetsForSurface.size());
        }
        ImGui::Text("Caras de frontera: %d", (int)boundary.size());

        if (cloudTypeIdx == 0) {
            int expectedF = 2 * (int)points.size() - 4;
            ImGui::Text("Caras esperadas (Euler, esfera/genero 0): %d", expectedF);
        } else if (cloudTypeIdx == 2) {
            int expectedF = 2 * (int)points.size();
            ImGui::Text("Caras esperadas (Euler, toro/genero 1): %d", expectedF);
            ImGui::TextDisabled("(solo si alpha reconstruye bien el agujero)");
        }

        if (ImGui::Button("Validar propiedad Delaunay")) {
            const double eps = 1e-6;
            int violations = 0;
            for (const auto& t : tets) {
                for (size_t i = 0; i < points.size(); ++i) {
                    if ((int)i == t.v[0] || (int)i == t.v[1] || (int)i == t.v[2] || (int)i == t.v[3]) continue;
                    double distSq = (points[i] - t.center).lengthSq();
                    if (distSq < t.radiusSq - eps) violations++;
                }
            }
            lastValidation = violations == 0 ? "OK: 0 violaciones" : ("FALLO: " + std::to_string(violations) + " violaciones");
        }
        ImGui::Text("Ultima validacion: %s", lastValidation.c_str());

        ImGui::End();

        // --- Render 3D ---
        glm::mat4 model = glm::rotate(glm::mat4(1.0f), glm::radians(rotationAngle), glm::vec3(0, 1, 0));
        glm::mat4 view = app.getCamera().viewMatrix();
        glm::mat4 proj = app.getCamera().projectionMatrix(app.aspectRatio());
        glm::vec3 viewPos = app.getCamera().position();

        if (showSurface && surfaceMesh.vertexCount > 0) {
            surfaceShader.use();
            surfaceShader.setMat4("uModel", model);
            surfaceShader.setMat4("uView", view);
            surfaceShader.setMat4("uProjection", proj);
            surfaceShader.setVec3("uViewPos", viewPos);
            surfaceShader.setVec3("uColor", glm::vec3(0.35f, 0.65f, 0.85f));
            surfaceShader.setInt("uFlatShading", 0);

            glBindVertexArray(surfaceMesh.vao);
            glDrawArrays(GL_TRIANGLES, 0, surfaceMesh.vertexCount);
        }

        if (showTetraEdges && edgesMesh.vertexCount > 0) {
            lineShader.use();
            lineShader.setMat4("uModel", model);
            lineShader.setMat4("uView", view);
            lineShader.setMat4("uProjection", proj);
            lineShader.setVec3("uColor", glm::vec3(1.0f, 0.55f, 0.2f));

            glBindVertexArray(edgesMesh.vao);
            glDrawArrays(GL_LINES, 0, edgesMesh.vertexCount);
        }

        glBindVertexArray(0);
    });

    deleteMesh(surfaceMesh);
    deleteMesh(edgesMesh);
    return 0;
}
