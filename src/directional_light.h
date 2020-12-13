#ifndef DIRECTIONAL_LIGHT_H_
#define DIRECTIONAL_LIGHT_H_

#include <glm/glm.hpp>

class DirectionalLight {

public:

    DirectionalLight();

    DirectionalLight(const glm::vec3& direction, const glm::vec3& intensity, float shadowMapEnabled);

    ~DirectionalLight();

    DirectionalLight& setDirection(const glm::vec3& direction);

    DirectionalLight& setIntensity(const glm::vec3& intensity);

    DirectionalLight& setShadowMapEnabled(bool value);

    const glm::vec3& getDirection() const {
        return m_direction;
    }

    const glm::vec3& getIntensity() const {
        return m_intensity;
    }

    bool isShadowMapEnabled() const {
        return m_enableShadowMap;
    }

private:

    glm::vec3 m_direction;

    glm::vec3 m_intensity;

    bool m_enableShadowMap;
};

#endif // DIRECTIONAL_LIGHT_H_
