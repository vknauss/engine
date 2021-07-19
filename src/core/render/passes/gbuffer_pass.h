#ifndef GBUFFER_PASS_H_INCLUDED
#define GBUFFER_PASS_H_INCLUDED

#include "geometry_render_pass.h"

#include "core/resources/texture.h"
#include "core/render/render_layer.h"

// The primary geometry pass
// use this class as a template for implementing other geometry passes
class GBufferPass : public GeometryRenderPass {

    friend class DeferredPass;

public:

    // overrides from RenderPass

    void onViewportResize(uint32_t width, uint32_t height) override;

    void setState() override;

    Texture* getGBufferDepth() {
        return &m_gBufferDepthTexture;
    }

    Texture* getGBufferNormals() {
        return &m_gBufferNormalViewSpaceTexture;
    }

    Texture* getGBufferAlbedoMetallic() {
        return &m_gBufferAlbedoMetallicTexture;
    }

    Texture* getGBufferEmissionRoughness() {
        return &m_gBufferEmissionRoughnessTexture;
    }

protected:

    // overrides from GeometryRenderPass

    bool loadShaders() override;

    void initRenderTargets() override;

    void cleanupRenderTargets() override;

    void bindMaterial(const Material* pMaterial, const Shader* pShader) override;

    void onBindShader(const Shader* pShader) override;

    InstanceListBuilder::filterPredicate getFilterPredicate() const override {
        return nullptr;
    }

    bool useNormalsMatrix() const override {
        return true;
    }

    bool useLastFrameMatrix() const override {
        return false;
    }

private:

    Texture m_gBufferNormalViewSpaceTexture;
    Texture m_gBufferAlbedoMetallicTexture;
    Texture m_gBufferEmissionRoughnessTexture;
    Texture m_gBufferDepthTexture;

    RenderLayer m_renderLayer;

    uint32_t m_viewportWidth, m_viewportHeight;

    const Texture* m_pBoundDiffuseTexture = nullptr;
    const Texture* m_pBoundNormalsTexture = nullptr;
    const Texture* m_pBoundEmissionTexture = nullptr;
    const Texture* m_pBoundMetallicRoughnessTexture = nullptr;

    bool m_useDiffuseTexture = false;
    bool m_useNormalsTexture = false;
    bool m_useEmissionTexture = false;
    bool m_useMetallicRoughnessTexture = false;

};

#endif // GBUFFER_PASS_H_INCLUDED
