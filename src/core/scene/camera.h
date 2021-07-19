#ifndef CAMERA_H_
#define CAMERA_H_

#include <glm/glm.hpp>

class Camera {

public:

    Camera(float fov, float aspect, float near, float far);
    Camera();

    ~Camera();

    glm::mat4 calculateViewMatrix() const;
    glm::mat4 calculateProjectionMatrix() const;

    Camera& setPosition(const glm::vec3& position);
    Camera& setDirection(const glm::vec3& direction);
    Camera& setUp(const glm::vec3& up);

    Camera& setFOV(float fov);
    Camera& setAspectRatio(float aspect);
    Camera& setNearPlane(float nearPlane);
    Camera& setFarPlane(float farPlane);

    glm::vec3 getPosition() const {
        return m_position;
    }

    glm::vec3 getDirection() const {
        return m_direction;
    }

    glm::vec3 getUp() const {
        return m_up;
    }

    float getFOV() const {
        return m_fov;
    }

    float getAspectRatio() const {
        return m_aspect;
    }

    float getNearPlane() const {
        return m_nearPlane;
    }

    float getFarPlane() const {
        return m_farPlane;
    }

private:

    glm::vec3 m_position;
    glm::vec3 m_direction;
    glm::vec3 m_up;

    float m_fov;
    float m_aspect;
    float m_nearPlane;
    float m_farPlane;
};

#endif // CAMERA_H_