#include "point_light.h"

PointLight::PointLight() :
    m_position(0.0f),
    m_intensity(1.0f),
    m_enableShadowMap(false) {
    computeBoundingSphereRadius();
}

PointLight::~PointLight() {
}

PointLight& PointLight::setPosition(const glm::vec3& position) {
    m_position = position;
    return *this;
}

PointLight& PointLight::setIntensity(const glm::vec3& intensity) {
    m_intensity = intensity;
    computeBoundingSphereRadius();
    return *this;
}

PointLight& PointLight::setShadowMapEnabled(bool value) {
    m_enableShadowMap = value;
    return *this;
}

void PointLight::computeBoundingSphereRadius() {
    static const float minIntensity = 0.1;
    m_boundingSphereRadius = glm::sqrt(glm::max(glm::max(m_intensity.r, m_intensity.g), m_intensity.b) / minIntensity - 1.0f) + minIntensity;
}

