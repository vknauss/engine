#ifndef TAA_PASS_H_INCLUDED
#define TAA_PASS_H_INCLUDED

#include <vector>

#include "core/render/render_pass.h"
#include "core/render/render_layer.h"
#include "core/render/shader.h"

class TAAPass : public RenderPass  {

public:

    void init() override;

    void onViewportResize(uint32_t width, uint32_t height) override;

    void setState() override;

    void render() override;

    void cleanup() override;

    void setMotionBlurTexture(Texture* pMotionBlurTexture);

    void setMotionBuffer(Texture* pMotionBuffer);

    void setSceneRenderLayer(RenderLayer* pSceneRenderLayer);
    void setSceneTexture(Texture* pSceneTexture);

    std::vector<glm::vec2> jitterSamples;
    glm::vec2 cJitter;

private:

    RenderLayer m_renderLayer;

    Texture m_historyTextures[2];

    Shader m_shader;
    Shader m_motionBlurCompositeShader;

    Texture* m_pMotionBlurTexture = nullptr;
    Texture* m_pSceneTexture;
    Texture* m_pMotionBuffer;

    RenderLayer* m_pSceneRenderLayer;

    uint32_t m_viewportWidth, m_viewportHeight;

    uint32_t m_cHistoryIndex = 0;
    uint32_t m_cJitterIndex = 0;

    bool m_historyValid = false;

};

#endif // TAA_PASS_H_INCLUDED
