#include "camera.h"

#include <glm/gtc/matrix_transform.hpp>

Camera::Camera(float fov, float aspect, float near, float far) :
    m_position(0, 0, 0), m_direction(0, 0, -1), m_up(0, 1, 0),
    m_fov(fov), m_aspect(aspect), m_nearPlane(near), m_farPlane(far) {
}

Camera::Camera() :
    Camera(M_PI/4.0, 4.0/3.0, 1.0, 100.0) {
}

Camera::~Camera() {
}

glm::mat4 Camera::calculateViewMatrix() const {
    return glm::lookAt(m_position, m_position + m_direction, m_up);
}

glm::mat4 Camera::calculateProjectionMatrix() const {
    return glm::perspective(m_fov, m_aspect, m_nearPlane, m_farPlane);
}

void Camera::setPosition(const glm::vec3& position) {
    m_position = position;
}

void Camera::setDirection(const glm::vec3& direction) {
    m_direction = glm::normalize(direction);
}

void Camera::setUp(const glm::vec3& up) {
    m_up = glm::normalize(up);
}

void Camera::setFOV(float fov) {
    m_fov = fov;
}

void Camera::setAspectRatio(float aspect) {
    m_aspect = aspect;
}

void Camera::setNearPlane(float near) {
    m_nearPlane = near;
}

void Camera::setFarPlane(float far) {
    m_farPlane = far;
}
