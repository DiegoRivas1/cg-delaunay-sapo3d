#include "Application.h"
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <imgui.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>
#include <iostream>

Application::Application(int width, int height, const std::string& title) {
    if (!glfwInit()) {
        std::cerr << "[Application] glfwInit fallo\n";
        return;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    window = glfwCreateWindow(width, height, title.c_str(), nullptr, nullptr);
    if (!window) {
        std::cerr << "[Application] glfwCreateWindow fallo\n";
        glfwTerminate();
        return;
    }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);  // vsync

    if (!gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress))) {
        std::cerr << "[Application] gladLoadGLLoader fallo\n";
        window = nullptr;
        return;
    }

    glEnable(GL_DEPTH_TEST);

    glfwSetWindowUserPointer(window, this);
    glfwSetScrollCallback(window, scrollCallback);
    glfwSetMouseButtonCallback(window, mouseButtonCallback);
    glfwSetCursorPosCallback(window, cursorPosCallback);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330 core");
}

Application::~Application() {
    if (window) {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        glfwDestroyWindow(window);
    }
    glfwTerminate();
}

float Application::aspectRatio() const {
    int w = 1, h = 1;
    glfwGetFramebufferSize(window, &w, &h);
    if (h == 0) h = 1;
    return static_cast<float>(w) / static_cast<float>(h);
}

void Application::run(const std::function<void(float)>& onFrame) {
    double lastTime = glfwGetTime();

    while (!glfwWindowShouldClose(window)) {
        double now = glfwGetTime();
        float dt = static_cast<float>(now - lastTime);
        lastTime = now;

        glfwPollEvents();

        int w, h;
        glfwGetFramebufferSize(window, &w, &h);
        glViewport(0, 0, w, h);
        glClearColor(0.08f, 0.09f, 0.11f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        onFrame(dt);

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
    }
}

void Application::scrollCallback(GLFWwindow* w, double, double yoff) {
    if (ImGui::GetIO().WantCaptureMouse) return;
    auto* app = static_cast<Application*>(glfwGetWindowUserPointer(w));
    app->camera.zoom(static_cast<float>(yoff));
}

void Application::mouseButtonCallback(GLFWwindow* w, int button, int action, int) {
    auto* app = static_cast<Application*>(glfwGetWindowUserPointer(w));
    if (ImGui::GetIO().WantCaptureMouse) { app->dragging = false; return; }

    if (button == GLFW_MOUSE_BUTTON_LEFT) {
        if (action == GLFW_PRESS) {
            app->dragging = true;
            glfwGetCursorPos(w, &app->lastMouseX, &app->lastMouseY);
        } else if (action == GLFW_RELEASE) {
            app->dragging = false;
        }
    }
}

void Application::cursorPosCallback(GLFWwindow* w, double x, double y) {
    auto* app = static_cast<Application*>(glfwGetWindowUserPointer(w));
    if (!app->dragging) return;

    double dx = x - app->lastMouseX;
    double dy = y - app->lastMouseY;
    app->lastMouseX = x;
    app->lastMouseY = y;

    app->camera.rotate(static_cast<float>(dx), static_cast<float>(dy));
}
