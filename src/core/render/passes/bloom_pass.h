#ifndef BLOOM_PASS_H_INCLUDED
#define BLOOM_PASS_H_INCLUDED

#include <vector>

#include "core/render/render_pass.h"
#include "core/render/render_layer.h"
#include "core/render/shader.h"

class BloomPass : public RenderPass {

public:

    void init() override;

    void onViewportResize(uint32_t width, uint32_t height) override;

    void setState() override;

    void render() override;

    void cleanup() override;

    void setSceneRenderLayer(RenderLayer* pSceneRenderLayer, Texture* pSceneTexture);

    uint32_t numLevels;

private:

    RenderLayer m_renderLayer[2];
    std::vector<Texture> m_renderTextures;

    Shader m_hdrExtractShader;
    Shader m_filterShader;

    RenderLayer* m_pSceneRenderLayer;
    Texture* m_pSceneTexture;

    uint32_t m_viewportWidth, m_viewportHeight;

};

#endif // BLOOM_PASS_H_INCLUDED
