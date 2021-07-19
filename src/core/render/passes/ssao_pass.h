#ifndef SSAO_PASS_H_INCLUDED
#define SSAO_PASS_H_INCLUDED

#include <vector>

#include <glm/glm.hpp>

#include "core/render/render_pass.h"
#include "core/render/render_layer.h"
#include "core/render/shader.h"

class SSAOPass : public RenderPass {

public:

    void init() override;

    void onViewportResize(uint32_t width, uint32_t height) override;

    void setState() override;

    void render() override;

    void setGBufferTextures(Texture* pGBufferDepth, Texture* pGBufferNormals);

    void setMatrices(const glm::mat4& projection, const glm::mat4& inverseProjection);

    Texture* getRenderTexture() {
        return &m_renderTexture;
    }

private:

    RenderLayer m_renderLayer;
    RenderLayer m_filterRenderLayer;

    Texture m_renderTexture;
    Texture m_filterTexture;
    Texture m_noiseTexture;

    Shader m_shader;
    Shader m_filterShader;

    std::vector<glm::vec3> m_kernel;

    glm::mat4 m_projection, m_projectionInverse;

    Texture* m_pGBufferDepth;
    Texture* m_pGBufferNormals;

    uint32_t m_renderTextureWidth, m_renderTextureHeight;

};

#endif // SSAO_PASS_H_INCLUDED
