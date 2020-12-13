#ifndef MATERIAL_H_
#define MATERIAL_H_

#include <glm/glm.hpp>

#include "texture.h"

class Material {

public:

    Material();

    ~Material();

    Material& setDiffuseTexture(Texture* pDiffuseTexture);

    Material& setNormalsTexture(Texture* pNormalsTexture);

    Material& setEmissionTexture(Texture* pEmissionTexture);

    Material& setMetallicRoughnessTexture(Texture* pMetallicRoughnessTexture);

    Material& setTintColor(const glm::vec3& tintColor);

    Material& setEmissionIntensity(const glm::vec3& emission);

    Material& setMetallic(float metallic);

    Material& setRoughness(float roughness);

    Material& setShadowCastingEnabled(bool value);

    const Texture* getDiffuseTexture() const {
        return m_pDiffuseTexture;
    }

    const Texture* getNormalsTexture() const {
        return m_pNormalsTexture;
    }

    const Texture* getEmissionTexture() const {
        return m_pEmissionTexture;
    }

    const Texture* getMetallicRoughnessTexture() const {
        return m_pMetallicRoughnessTexture;
    }


    const glm::vec3& getTintColor() const {
        return m_tintColor;
    }

    const glm::vec3& getEmissionIntensity() const {
        return m_emission;
    }

    float getMetallic() const {
        return m_metallic;
    }

    float getRoughness() const {
        return m_roughness;
    }

    bool isShadowCastingEnabled() const {
        return m_enableShadowCasting;
    }

private:

    Texture* m_pDiffuseTexture;
    Texture* m_pNormalsTexture;
    Texture* m_pEmissionTexture;
    Texture* m_pMetallicRoughnessTexture;

    glm::vec3 m_tintColor;
    glm::vec3 m_emission;

    float m_metallic;
    float m_roughness;

    bool m_enableShadowCasting;

};

#endif // MATERIAL_H_
