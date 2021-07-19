#include "material.h"

Material::Material() :
    m_pDiffuseTexture(nullptr),
    m_pNormalsTexture(nullptr),
    m_pEmissionTexture(nullptr),
    m_pMetallicRoughnessTexture(nullptr),
    m_tintColor(1.0, 1.0, 1.0),
    m_emission(0.0, 0.0, 0.0),
    m_metallic(0.0),
    m_roughness(0.2),
    m_alpha(1.0),
    m_alphaMaskThreshold(0.0),
    m_enableShadowCasting(true),
    m_useObjectMotionVectors(true),
    m_enableTransparency(false)
{
}

Material::~Material() {
}

Material& Material::setDiffuseTexture(const Texture* pDiffuseTexture) {
    m_pDiffuseTexture = pDiffuseTexture;

    return *this;
}

Material& Material::setNormalsTexture(const Texture* pNormalsTexture) {
    m_pNormalsTexture = pNormalsTexture;

    return *this;
}

Material& Material::setEmissionTexture(const Texture* pEmissionTexture) {
    m_pEmissionTexture = pEmissionTexture;

    return *this;
}

Material& Material::setMetallicRoughnessTexture(const Texture* pMetallicRoughnessTexture) {
    m_pMetallicRoughnessTexture = pMetallicRoughnessTexture;

    return *this;
}

Material& Material::setTintColor(const glm::vec3& tintColor) {
    m_tintColor = tintColor;

    return *this;
}

Material& Material::setEmissionIntensity(const glm::vec3& emission) {
    m_emission = emission;

    return *this;
}

Material& Material::setMetallic(float metallic) {
    m_metallic = metallic;

    return *this;
}

Material& Material::setRoughness(float roughness) {
    m_roughness = roughness;

    return *this;
}

Material& Material::setAlpha(float alpha) {
    m_alpha = alpha;

    return *this;
}

Material& Material::setAlphaMaskThreshold(float threshold) {
    m_alphaMaskThreshold = threshold;

    return *this;
}

Material& Material::setShadowCastingEnabled(bool value) {
    m_enableShadowCasting = value;

    return *this;
}

Material& Material::setObjectMotionVectors(bool value) {
    m_useObjectMotionVectors = value;

    return *this;
}

Material& Material::setTransparencyEnabled(bool value) {
    m_enableTransparency = value;

    return *this;
}
