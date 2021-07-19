#ifndef TRANSPARENCY_PASS_H_INCLUDED
#define TRANSPARENCY_PASS_H_INCLUDED

#include <glm/glm.hpp>

#include "geometry_render_pass.h"
#include "core/render/render_layer.h"

class TransparencyPass : public GeometryRenderPass {

public:

    // overrides from RenderPass

    void onViewportResize(uint32_t width, uint32_t height) override;

    void setState() override;

    void setSceneDepthBuffer(RenderBuffer* pDepthRenderBuffer);

    void setInverseProjectionMatrix(const glm::mat4& inverseProjection);

    void setLightDirectionViewSpace(const glm::vec3& lightDirectionViewSpace);

    void setLightIntensity(const glm::vec3& lightIntensity);

    void setAmbientLight(const glm::vec3& ambientLight);

    Texture* getAccumTexture() {
        return &m_accumTexture;
    }

    Texture* getRevealageTexture() {
        return &m_revealageTexture;
    }

protected:

    // overrides from GeometryRenderPass

    bool loadShaders() override;

    void initRenderTargets() override;

    void cleanupRenderTargets() override;

    void bindMaterial(const Material* pMaterial, const Shader* pShader) override;

    void onBindShader(const Shader* pShader) override;

    InstanceListBuilder::filterPredicate getFilterPredicate() const override {
        return [] (const Model* pModel) { return pModel->getMaterial() && pModel->getMaterial()->isTransparencyEnabled(); };
    }

    bool useNormalsMatrix() const override {
        return true;
    }

    bool useLastFrameMatrix() const override {
        return false;
    }

private:

    Texture m_accumTexture;
    Texture m_revealageTexture;

    RenderLayer m_renderLayer;

    RenderBuffer* m_pDepthRenderBuffer;

    uint32_t m_viewportWidth, m_viewportHeight;

    glm::mat4 m_projectionInverse;
    glm::vec3 m_lightDirectionViewSpace;
    glm::vec3 m_lightIntensity;
    glm::vec3 m_ambientLight;

    const Texture* m_pBoundDiffuseTexture = nullptr;
    const Texture* m_pBoundNormalsTexture = nullptr;
    const Texture* m_pBoundEmissionTexture = nullptr;
    const Texture* m_pBoundMetallicRoughnessTexture = nullptr;

    bool m_useDiffuseTexture = false;
    bool m_useNormalsTexture = false;
    bool m_useEmissionTexture = false;
    bool m_useMetallicRoughnessTexture = false;

};

#endif // TRANSPARENCY_PASS_H_INCLUDED
