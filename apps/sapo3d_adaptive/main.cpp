#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <imgui.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <vector>
#include <string>
#include <cmath>
#include <algorithm>
#include <filesystem>

#include "core/Application.h"
#include "core/Shader.h"
#include "core/MeshBuilder.h"
#include "delaunay/IDelaunayStrategy.hpp"
#include "delaunay/PointCloudIO.hpp"
#include "delaunay/ManifestIO.hpp"
#include "delaunay/TetraCache.hpp"

namespace fs = std::filesystem;

static glm::vec3 hsvToRgb(float h, float s, float v) {
    float c = v * s;
    float x = c * (1.0f - std::fabs(std::fmod(h / 60.0f, 2.0f) - 1.0f));
    float m = v - c;
    float r, g, b;
    if (h < 60) { r = c; g = x; b = 0; }
    else if (h < 120) { r = x; g = c; b = 0; }
    else if (h < 180) { r = 0; g = c; b = x; }
    else if (h < 240) { r = 0; g = x; b = c; }
    else if (h < 300) { r = x; g = 0; b = c; }
    else { r = c; g = 0; b = x; }
    return {r + m, g + m, b + m};
}

struct OrganState {
    OrganEntry entry;
    glm::vec3 color{1.0f};
    bool visible = false;
    bool triangulated = false;
    bool fromCache = false;
    bool hasLocalScale = false;  // false = .xyz viejo (sin 4ta columna), se usa fallback
    bool hasError = false;
    std::string errorMsg;

    std::vector<Vec3> points;
    std::vector<double> localScale;
    std::vector<Tetrahedron> tets;
    GpuMesh mesh;
};

// Carga puntos + escala local SIEMPRE desde el .xyz (es solo texto, rapido
// incluso para organos grandes). El cache binario (TetraCache) NO guarda
// la escala local -- solo geometria de la triangulacion -- asi que se usa
// unicamente para saltarse el paso caro (Bowyer-Watson), no la lectura.
static void ensureTriangulated(OrganState& o, BowyerWatsonStrategy& strategy,
                                const std::string& dataDir, const std::string& cacheDir) {
    if (o.triangulated || o.hasError) return;

    std::string err;
    o.points = PointCloudIO::loadXYZWithScale(dataDir + "/" + o.entry.pointsFile, o.localScale, &err);
    if (o.points.size() < 4) {
        o.hasError = true;
        o.errorMsg = err.empty() ? "muy pocos puntos" : err;
        return;
    }

    // Si el .xyz es del formato viejo (sin 4ta columna), localScale queda
    // en todo ceros -- fallback a un valor uniforme equivalente al alpha
    // global de siempre (alphaAuto = 2.5 * medianNN => medianNN = alphaAuto/2.5),
    // asi el resultado es identico al modo global hasta que se regenere
    // el .xyz con 'extract_all.py' actualizado.
    bool allZero = std::all_of(o.localScale.begin(), o.localScale.end(),
                                [](double v) { return v <= 0.0; });
    o.hasLocalScale = !allZero;
    if (allZero) {
        double fallback = o.entry.alphaAuto / 2.5;
        std::fill(o.localScale.begin(), o.localScale.end(), fallback);
    }

    // Reusa el cache binario si ya existe (mismo <organo>_<numPuntos>.bin
    // que genera sapo3d_hd -- no hace falta re-triangular nada que ya
    // este calculado).
    std::string cachePath = TetraCache::cachePath(cacheDir, o.entry.name, o.entry.numPoints);
    std::vector<Vec3> cachedPoints;
    if (TetraCache::load(cachePath, cachedPoints, o.tets) &&
        cachedPoints.size() == o.points.size()) {
        o.triangulated = true;
        o.fromCache = true;
        return;
    }

    o.tets = strategy.triangulate(o.points);
    o.triangulated = true;
    o.fromCache = false;
    TetraCache::save(cachePath, o.points, o.tets);
}

static void rebuildMesh(OrganState& o, double multiplier) {
    if (!o.triangulated) return;

    auto filtered = BowyerWatson3D::filterByAdaptiveAlpha(o.tets, o.localScale, multiplier);
    auto boundary = BowyerWatson3D::boundaryFaces(filtered);

    MeshBuilder::deleteMesh(o.mesh);
    o.mesh = MeshBuilder::buildSurfaceMesh(o.points, boundary);
}

int main() {
    Application app(1400, 900, "cg-delaunay-sapo3d :: Sapo Adaptativo");
    if (!app.ok()) return -1;

    Shader surfaceShader("shaders/basic.vert", "shaders/basic.frag");
    BowyerWatsonStrategy strategy;

    const std::string dataDir = "data";
    const std::string cacheDir = "data/cache";  // MISMO cache que sapo3d_hd
    fs::create_directories(cacheDir);

    std::string manifestError;
    auto entries = ManifestIO::loadManifest(dataDir + "/manifest.tsv", &manifestError);

    std::vector<OrganState> organs;
    organs.reserve(entries.size());
    for (size_t i = 0; i < entries.size(); ++i) {
        OrganState o;
        o.entry = entries[i];
        float hue = 360.0f * static_cast<float>(i) / std::max<size_t>(1, entries.size());
        o.color = hsvToRgb(hue, 0.65f, 0.9f);
        organs.push_back(std::move(o));
    }

    float multiplier = 2.5f;  // mismo factor que usabamos para alpha_auto global
    bool autoRotate = true;
    float rotationAngle = 0.0f;
    std::string bakeStatus;

    app.getCamera().distance = 400.0f;
    app.getCamera().pitch = 15.0f;

    app.run([&](float dt) {
        if (autoRotate) rotationAngle += dt * 10.0f;

        ImGui::Begin("Sapo 3D Adaptativo - Organos");

        if (!manifestError.empty()) {
            ImGui::TextColored(ImVec4(1, 0.4f, 0.4f, 1), "Error cargando manifest: %s", manifestError.c_str());
            ImGui::TextWrapped("Corre 'python scripts/extract_all.py data data' primero.");
        } else {
            ImGui::Text("%d organos disponibles  (cache compartido: %s)", (int)organs.size(), cacheDir.c_str());
            ImGui::TextWrapped(
                "Alpha ADAPTATIVO: cada tetraedro usa el promedio de densidad "
                "local de sus 4 vertices, en vez de un unico valor global por "
                "organo. Reconstruye mejor zonas anchas + extremidades finas "
                "del mismo organo.");

            if (ImGui::Button("Precompilar todos (sin mostrar)")) {
                int done = 0;
                for (auto& o : organs) {
                    ensureTriangulated(o, strategy, dataDir, cacheDir);
                    ++done;
                }
                bakeStatus = "Precompilados " + std::to_string(done) + "/" + std::to_string(organs.size());
            }
            ImGui::SameLine();
            if (ImGui::Button("Mostrar todos")) {
                for (auto& o : organs) {
                    ensureTriangulated(o, strategy, dataDir, cacheDir);
                    if (o.triangulated && o.mesh.vertexCount == 0) rebuildMesh(o, multiplier);
                    o.visible = !o.hasError;
                }
            }
            ImGui::SameLine();
            if (ImGui::Button("Ocultar todos")) {
                for (auto& o : organs) o.visible = false;
            }
            if (!bakeStatus.empty()) ImGui::TextDisabled("%s", bakeStatus.c_str());

            ImGui::Separator();
            bool changed = ImGui::SliderFloat("Multiplicador (x densidad local)", &multiplier, 0.5f, 10.0f);
            ImGui::SameLine();
            if (ImGui::Button("Aplicar") || changed) {
                for (auto& o : organs) {
                    if (o.triangulated) rebuildMesh(o, multiplier);
                }
            }
            ImGui::Checkbox("Auto-rotar", &autoRotate);

            ImGui::Separator();
            ImGui::BeginChild("organ_list", ImVec2(0, 360), true);
            for (auto& o : organs) {
                ImGui::PushID(o.entry.name.c_str());

                ImGui::SetNextItemWidth(24);
                ImGui::ColorEdit3("##color", &o.color.x,
                                   ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel);
                ImGui::SameLine();

                bool wasVisible = o.visible;
                std::string label = o.entry.name + " (" + std::to_string(o.entry.numPoints) + " pts)";
                if (o.triangulated && o.fromCache) label += " [cache]";
                if (o.triangulated && !o.hasLocalScale) label += " [sin datos locales, fallback global]";
                if (o.hasError) label += " [ERROR]";
                ImGui::Checkbox(label.c_str(), &o.visible);

                if (o.visible && !wasVisible) {
                    ensureTriangulated(o, strategy, dataDir, cacheDir);
                    if (!o.triangulated) o.visible = false;
                    else if (o.mesh.vertexCount == 0) rebuildMesh(o, multiplier);
                }
                if (o.hasError && ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("%s", o.errorMsg.c_str());
                }

                ImGui::PopID();
            }
            ImGui::EndChild();
        }

        ImGui::End();

        // --- Render 3D ---
        glm::mat4 model = glm::rotate(glm::mat4(1.0f), glm::radians(rotationAngle), glm::vec3(0, 1, 0));
        glm::mat4 view = app.getCamera().viewMatrix();
        glm::mat4 proj = app.getCamera().projectionMatrix(app.aspectRatio());
        glm::vec3 viewPos = app.getCamera().position();

        surfaceShader.use();
        surfaceShader.setMat4("uModel", model);
        surfaceShader.setMat4("uView", view);
        surfaceShader.setMat4("uProjection", proj);
        surfaceShader.setVec3("uViewPos", viewPos);
        surfaceShader.setInt("uFlatShading", 0);

        for (const auto& o : organs) {
            if (!o.visible || o.mesh.vertexCount == 0) continue;
            surfaceShader.setVec3("uColor", o.color);
            glBindVertexArray(o.mesh.vao);
            glDrawArrays(GL_TRIANGLES, 0, o.mesh.vertexCount);
        }
        glBindVertexArray(0);
    });

    for (auto& o : organs) MeshBuilder::deleteMesh(o.mesh);
    return 0;
}
