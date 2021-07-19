#ifndef MATERIAL_H_
#define MATERIAL_H_

#include <string>

#include <glm/glm.hpp>

#include "texture.h"

class Material {

public:

    Material();

    ~Material();

    Material& setDiffuseTexture(const Texture* pDiffuseTexture);

    Material& setNormalsTexture(const Texture* pNormalsTexture);

    Material& setEmissionTexture(const Texture* pEmissionTexture);

    Material& setMetallicRoughnessTexture(const Texture* pMetallicRoughnessTexture);

    Material& setTintColor(const glm::vec3& tintColor);

    Material& setEmissionIntensity(const glm::vec3& emission);

    Material& setMetallic(float metallic);

    Material& setRoughness(float roughness);

    Material& setAlpha(float alpha);

    Material& setAlphaMaskThreshold(float threshold);

    Material& setShadowCastingEnabled(bool value);

    Material& setObjectMotionVectors(bool value);

    Material& setTransparencyEnabled(bool value);

    void setName(const std::string& name) {
        m_name = name;
    }

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

    float getAlpha() const {
        return m_alpha;
    }

    float getAlphaMaskThreshold() const {
        return m_alphaMaskThreshold;
    }

    bool isShadowCastingEnabled() const {
        return m_enableShadowCasting;
    }

    bool useObjectMotionVectors() const {
        return m_useObjectMotionVectors;
    }

    bool isTransparencyEnabled() const {
        return m_enableTransparency;
    }

    const std::string& getName() const {
        return m_name;
    }

private:

    std::string m_name;

    const Texture* m_pDiffuseTexture;
    const Texture* m_pNormalsTexture;
    const Texture* m_pEmissionTexture;
    const Texture* m_pMetallicRoughnessTexture;

    glm::vec3 m_tintColor;
    glm::vec3 m_emission;

    float m_metallic;
    float m_roughness;
    float m_alpha;
    float m_alphaMaskThreshold;

    bool m_enableShadowCasting;
    bool m_useObjectMotionVectors;
    bool m_enableTransparency;

};

#endif // MATERIAL_H_
