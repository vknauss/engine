#ifndef TRANSPARENCY_COMPOSITE_PASS_H_INCLUDED
#define TRANSPARENCY_COMPOSITE_PASS_H_INCLUDED

#include "core/render/render_pass.h"
#include "core/render/render_layer.h"
#include "core/render/shader.h"

class TransparencyCompositePass : public RenderPass {

public:

    void init() override;

    void onViewportResize(uint32_t width, uint32_t height) override;

    void setState() override;

    void render() override;

    void cleanup() override;

    void setOutputRenderLayer(RenderLayer* pOutputRenderLayer);

    void setTransparencyRenderTextures(Texture* pAccumTexture, Texture* pRevealageTexture);

private:

    Shader m_shader;

    RenderLayer* m_pOutputRenderLayer = nullptr;

    Texture* m_pAccumTexture = nullptr;
    Texture* m_pRevealageTexture = nullptr;

    uint32_t m_viewportWidth, m_viewportHeight;

};

#endif // TRANSPARENCY_COMPOSITE_PASS_H_INCLUDED
