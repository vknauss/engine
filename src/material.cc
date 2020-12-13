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
    m_enableShadowCasting(true)
{
}

Material::~Material() {
}

Material& Material::setDiffuseTexture(Texture* pDiffuseTexture) {
    m_pDiffuseTexture = pDiffuseTexture;

    return *this;
}

Material& Material::setNormalsTexture(Texture* pNormalsTexture) {
    m_pNormalsTexture = pNormalsTexture;

    return *this;
}

Material& Material::setEmissionTexture(Texture* pEmissionTexture) {
    m_pEmissionTexture = pEmissionTexture;

    return *this;
}

Material& Material::setMetallicRoughnessTexture(Texture* pMetallicRoughnessTexture) {
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

Material& Material::setShadowCastingEnabled(bool value) {
    m_enableShadowCasting = value;

    return *this;
}
