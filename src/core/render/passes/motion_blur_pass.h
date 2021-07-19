#ifndef MOTION_BLUR_PASS_H_INCLUDED
#define MOTION_BLUR_PASS_H_INCLUDED

#include "core/render/render_pass.h"
#include "core/render/render_layer.h"
#include "core/render/shader.h"

class MotionBlurPass : public RenderPass {

public:

    void init() override;

    void onViewportResize(uint32_t width, uint32_t height) override;

    void setState() override;

    void render() override;

    void setMotionBuffer(Texture* pMotionBuffer);

    void setSceneTexture(Texture* pSceneTexture);

    void setGBufferDepth(Texture* pGBufferDepth);

    Texture* getRenderTexture() {
        return &m_renderTexture;
    }

    // Set to true if motion blur should always be rendered
    // Set to false if it will be combined in a separate pass, e.g. TAA
    bool copyToSceneTexture = false;

private:

    Shader m_shader;

    Texture m_renderTexture;

    Texture* m_pMotionBuffer;
    Texture* m_pSceneTexture;
    Texture* m_pGBufferDepth;

    RenderLayer m_renderLayer;

    uint32_t m_viewportWidth, m_viewportHeight;


};

#endif // MOTION_BLUR_PASS_H_INCLUDED
