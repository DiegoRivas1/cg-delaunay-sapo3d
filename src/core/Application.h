#pragma once
#include <string>
#include <functional>
#include "Camera.h"

struct GLFWwindow;

// Encapsula ventana + contexto OpenGL + ImGui + loop principal.
// El usuario solo escribe la logica de un frame via el callback de run().
class Application {
public:
    Application(int width, int height, const std::string& title);
    ~Application();

    bool ok() const { return window != nullptr; }

    GLFWwindow* getWindow() const { return window; }
    Camera& getCamera() { return camera; }
    float aspectRatio() const;

    // onFrame(deltaTime) se llama una vez por frame, DESPUES de
    // ImGui::NewFrame() y ANTES de ImGui::Render(). Ahi el usuario
    // dibuja su escena 3D y sus paneles de ImGui.
    void run(const std::function<void(float)>& onFrame);

private:
    GLFWwindow* window = nullptr;
    Camera camera;

    bool dragging = false;
    double lastMouseX = 0.0, lastMouseY = 0.0;

    static void scrollCallback(GLFWwindow* w, double xoff, double yoff);
    static void mouseButtonCallback(GLFWwindow* w, int button, int action, int mods);
    static void cursorPosCallback(GLFWwindow* w, double x, double y);
};
