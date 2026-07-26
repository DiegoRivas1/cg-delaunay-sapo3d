#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>

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
        // Velocidad proporcional a la distancia actual: paso fino de cerca,
        // paso grande de lejos (si no, alejarse del sapo completo a paso
        // fijo de 1.0 requeriria cientos de scrolls).
        float speed = std::max(0.5f, distance * 0.1f);
        distance -= scrollY * speed;
        if (distance < 0.5f) distance = 0.5f;
        if (distance > 3000.0f) distance = 3000.0f;
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
        return glm::perspective(glm::radians(fovDeg), aspect, 0.1f, 5000.0f);
    }
};
