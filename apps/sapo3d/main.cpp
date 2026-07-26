#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <imgui.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <vector>
#include <string>
#include <cmath>
#include <iostream>
#include <algorithm>

#include "core/Application.h"
#include "core/Shader.h"
#include "core/MeshBuilder.h"
#include "delaunay/IDelaunayStrategy.hpp"
#include "delaunay/PointCloudIO.hpp"
#include "delaunay/ManifestIO.hpp"

// Convierte HSV (h en grados 0-360, s,v en 0-1) a RGB. Se usa para
// darle a cada organo un color distinto y reproducible por indice.
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

// Estado de un organo: datos crudos (puntos/tetraedros) se calculan una
// sola vez ("triangulated"); la malla de superficie se puede reconstruir
// barato cuando cambia alpha (no hace falta re-triangular, solo re-filtrar
// y recalcular el borde).
struct OrganState {
    OrganEntry entry;
    glm::vec3 color{1.0f};
    bool visible = false;
    bool triangulated = false;
    bool hasError = false;
    std::string errorMsg;

    std::vector<Vec3> points;
    std::vector<Tetrahedron> tets;
    GpuMesh mesh;
};

static void ensureTriangulated(OrganState& o, BowyerWatsonStrategy& strategy, const std::string& dataDir) {
    if (o.triangulated || o.hasError) return;

    std::string err;
    o.points = PointCloudIO::loadXYZ(dataDir + "/" + o.entry.pointsFile, &err);
    if (o.points.size() < 4) {
        o.hasError = true;
        o.errorMsg = err.empty() ? "muy pocos puntos" : err;
        return;
    }

    o.tets = strategy.triangulate(o.points);
    o.triangulated = true;
}

static void rebuildMesh(OrganState& o, double alphaMultiplier) {
    if (!o.triangulated) return;

    double alpha = o.entry.alphaAuto * alphaMultiplier;
    auto filtered = BowyerWatson3D::filterByAlpha(o.tets, alpha);
    auto boundary = BowyerWatson3D::boundaryFaces(filtered);

    MeshBuilder::deleteMesh(o.mesh);
    o.mesh = MeshBuilder::buildSurfaceMesh(o.points, boundary);
}

int main() {
    Application app(1400, 900, "cg-delaunay-sapo3d :: Sapo Completo");
    if (!app.ok()) return -1;

    Shader surfaceShader("shaders/basic.vert", "shaders/basic.frag");
    BowyerWatsonStrategy strategy;

    const std::string dataDir = "data";
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

    float alphaMultiplier = 1.0f;
    bool autoRotate = true;
    float rotationAngle = 0.0f;

    app.getCamera().distance = 400.0f;
    app.getCamera().pitch = 15.0f;

    app.run([&](float dt) {
        if (autoRotate) rotationAngle += dt * 10.0f;

        ImGui::Begin("Sapo 3D - Organos");

        if (!manifestError.empty()) {
            ImGui::TextColored(ImVec4(1, 0.4f, 0.4f, 1), "Error cargando manifest: %s", manifestError.c_str());
            ImGui::TextWrapped("Corre 'python scripts/extract_all.py data data' primero.");
        } else {
            ImGui::Text("%d organos disponibles", (int)organs.size());

            if (ImGui::Button("Mostrar todos")) {
                for (auto& o : organs) {
                    ensureTriangulated(o, strategy, dataDir);
                    if (o.triangulated && o.mesh.vertexCount == 0) rebuildMesh(o, alphaMultiplier);
                    o.visible = !o.hasError;
                }
            }
            ImGui::SameLine();
            if (ImGui::Button("Ocultar todos")) {
                for (auto& o : organs) o.visible = false;
            }

            ImGui::Separator();
            bool alphaChanged = ImGui::SliderFloat("Alpha global (multiplicador)", &alphaMultiplier, 0.3f, 3.0f);
            ImGui::SameLine();
            if (ImGui::Button("Aplicar") || alphaChanged) {
                for (auto& o : organs) {
                    if (o.triangulated) rebuildMesh(o, alphaMultiplier);
                }
            }
            ImGui::Checkbox("Auto-rotar", &autoRotate);

            ImGui::Separator();
            ImGui::BeginChild("organ_list", ImVec2(0, 320), true);
            for (auto& o : organs) {
                ImGui::PushID(o.entry.name.c_str());

                ImGui::ColorButton(("##color_" + o.entry.name).c_str(),
                                    ImVec4(o.color.r, o.color.g, o.color.b, 1.0f),
                                    ImGuiColorEditFlags_NoTooltip, ImVec2(14, 14));
                ImGui::SameLine();

                bool wasVisible = o.visible;
                std::string label = o.entry.name + " (" + std::to_string(o.entry.numPoints) + " pts)";
                if (o.hasError) label += " [ERROR]";
                ImGui::Checkbox(label.c_str(), &o.visible);

                if (o.visible && !wasVisible) {
                    ensureTriangulated(o, strategy, dataDir);
                    if (!o.triangulated) o.visible = false;
                    else if (o.mesh.vertexCount == 0) rebuildMesh(o, alphaMultiplier);
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
