#include "core/render/passes/transparency_composite_pass.h"

#include "core/render/fullscreen_quad.h"

#include "core/render/render_debug.h"

void TransparencyCompositePass::init() {
    m_shader.linkShaderFiles("shaders/vertex_fs.glsl", "shaders/fragment_composite_transparency.glsl");
}

void TransparencyCompositePass::onViewportResize(uint32_t width, uint32_t height) {
    m_viewportWidth = width;
    m_viewportHeight = height;
}

void TransparencyCompositePass::setState() {
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glDisable(GL_STENCIL_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

void TransparencyCompositePass::render() {

    m_pOutputRenderLayer->setEnabledDrawTargets({0});
    m_pOutputRenderLayer->bind();


    glViewport(0, 0, m_viewportWidth, m_viewportHeight);


    m_shader.bind();
    m_shader.setUniform("accumTexture", 0);
    m_shader.setUniform("revealageTexture", 1);


    m_pAccumTexture->bind(0);
    m_pRevealageTexture->bind(1);


    FullscreenQuad::draw();

}

void TransparencyCompositePass::cleanup() {
    m_pOutputRenderLayer = nullptr;
    m_pAccumTexture = nullptr;
    m_pRevealageTexture = nullptr;
}

void TransparencyCompositePass::setOutputRenderLayer(RenderLayer* pOutputRenderLayer) {
    m_pOutputRenderLayer = pOutputRenderLayer;
}

void TransparencyCompositePass::setTransparencyRenderTextures(Texture* pAccumTexture, Texture* pRevealageTexture) {
    m_pAccumTexture = pAccumTexture;
    m_pRevealageTexture = pRevealageTexture;
}
