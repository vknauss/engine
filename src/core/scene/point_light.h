#ifndef POINT_LIGHT_H_
#define POINT_LIGHT_H_

#include <glm/glm.hpp>

class PointLight {

public:

    PointLight();

    ~PointLight();

    PointLight& setPosition(const glm::vec3& position);

    PointLight& setIntensity(const glm::vec3& intensity);

    PointLight& setShadowMapEnabled(bool value);

    const glm::vec3& getPosition() const {
        return m_position;
    }

    const glm::vec3& getIntensity() const {
        return m_intensity;
    }

    float getBoundingSphereRadius() const {
        return m_boundingSphereRadius;
    }

    bool isShadowMapEnabled() const {
        return m_enableShadowMap;
    }

private:

    glm::vec3 m_position;

    glm::vec3 m_intensity;

    float m_boundingSphereRadius;

    bool m_enableShadowMap;

    void computeBoundingSphereRadius();

};

#endif // POINT_LIGHT_H_
