#include "directional_light.h"

DirectionalLight::DirectionalLight() :
    m_direction(0, -1, 0),
    m_intensity(1, 1, 1),
    m_enableShadowMap(true) {
}

DirectionalLight::DirectionalLight(const glm::vec3& direction, const glm::vec3& intensity, float shadowMapEnabled) :
    m_intensity(intensity),
    m_enableShadowMap(shadowMapEnabled) {
    setDirection(direction);
}


DirectionalLight::~DirectionalLight() {
}

DirectionalLight& DirectionalLight::setDirection(const glm::vec3& direction) {
    m_direction = glm::normalize(direction);
    return *this;
}

DirectionalLight& DirectionalLight::setIntensity(const glm::vec3& intensity) {
    m_intensity = intensity;
    return *this;
}

DirectionalLight& DirectionalLight::setShadowMapEnabled(bool value) {
    m_enableShadowMap = value;
    return *this;
}
