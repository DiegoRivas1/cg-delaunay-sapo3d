#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

// Camara orbital: rota alrededor de un target fijo (el centro del modelo).
class Camera {
public:
    glm::vec3 target{0.0f, 0.0f, 0.0f};
    float distance = 20.0f;
    float yaw = -90.0f;    // grados
    float pitch = 20.0f;   // grados
    float fovDeg = 45.0f;

    void rotate(float dx, float dy) {
        const float sensitivity = 0.25f;
        yaw += dx * sensitivity;
        pitch -= dy * sensitivity;
        if (pitch > 89.0f) pitch = 89.0f;
        if (pitch < -89.0f) pitch = -89.0f;
    }

    void zoom(float scrollY) {
        distance -= scrollY * 1.0f;
        if (distance < 1.0f) distance = 1.0f;
        if (distance > 200.0f) distance = 200.0f;
    }

    glm::vec3 position() const {
        float yawRad = glm::radians(yaw);
        float pitchRad = glm::radians(pitch);
        glm::vec3 offset;
        offset.x = distance * std::cos(pitchRad) * std::cos(yawRad);
        offset.y = distance * std::sin(pitchRad);
        offset.z = distance * std::cos(pitchRad) * std::sin(yawRad);
        return target + offset;
    }

    glm::mat4 viewMatrix() const {
        return glm::lookAt(position(), target, glm::vec3(0.0f, 1.0f, 0.0f));
    }

    glm::mat4 projectionMatrix(float aspect) const {
        return glm::perspective(glm::radians(fovDeg), aspect, 0.1f, 500.0f);
    }
};
