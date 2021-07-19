#include "core/render/passes/motion_blur_pass.h"

#include "core/render/fullscreen_quad.h"

void MotionBlurPass::init() {
    m_shader.linkShaderFiles("shaders/vertex_fs.glsl", "shaders/fragment_motion_blur.glsl");

    TextureParameters param = {};
    param.numComponents = 4;
    param.useFloatComponents = true; // HDR
    param.useLinearFiltering = true; // Jitter
    param.useEdgeClamping = true;  // Duh

    m_renderTexture.setParameters(param);
}

void MotionBlurPass::onViewportResize(uint32_t width, uint32_t height) {
    m_viewportWidth = width;
    m_viewportHeight = height;

    TextureParameters param = m_renderTexture.getParameters();
    param.width = width;
    param.height = height;

    m_renderTexture.setParameters(param);
    m_renderTexture.allocateData(nullptr);
    m_renderLayer.setTextureAttachment(0, &m_renderTexture);
}

void MotionBlurPass::setState() {
    glDisable(GL_BLEND);
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glDisable(GL_STENCIL_TEST);

    m_renderLayer.setEnabledDrawTargets({0});
    m_renderLayer.bind();
    m_renderLayer.validate();
    glViewport(0, 0, m_viewportWidth, m_viewportHeight);
    glClear(GL_COLOR_BUFFER_BIT);
}

void MotionBlurPass::render() {
    m_shader.bind();
    m_shader.setUniform("sceneColor", 0);
    m_shader.setUniform("motionBuffer", 1);
    m_shader.setUniform("depthBuffer", 2);

    m_pSceneTexture->bind(0);
    m_pMotionBuffer->bind(1);
    m_pGBufferDepth->bind(2);

    FullscreenQuad::draw();

    if (copyToSceneTexture) {
        glCopyImageSubData(m_renderTexture.getHandle(),  GL_TEXTURE_2D, 0, 0, 0, 0,
                           m_pSceneTexture->getHandle(), GL_TEXTURE_2D, 0, 0, 0, 0,
                           m_viewportWidth, m_viewportHeight, 1);
    }
}

void MotionBlurPass::setMotionBuffer(Texture* pMotionBuffer) {
    m_pMotionBuffer = pMotionBuffer;
}

void MotionBlurPass::setSceneTexture(Texture* pSceneTexture) {
    m_pSceneTexture = pSceneTexture;
}

void MotionBlurPass::setGBufferDepth(Texture* pGBufferDepth) {
    m_pGBufferDepth = pGBufferDepth;
}
