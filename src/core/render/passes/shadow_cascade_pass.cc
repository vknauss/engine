#include "shadow_cascade_pass.h"

#include <GL/glew.h>

#include "core/render/render_debug.h"

void ShadowCascadePass::setState() {
    VKR_DEBUG_CALL(
    m_pRenderLayer->bind();
    m_pRenderLayer->setDepthTexture(m_pDepthArrayTexture, m_layer);)
    m_pRenderLayer->validate();


    VKR_DEBUG_CALL(
    m_pRenderLayer->bind();)

    VKR_DEBUG_CALL(
    glViewport(0, 0, m_textureSize, m_textureSize);)
    VKR_DEBUG_CALL(
    glClear(GL_DEPTH_BUFFER_BIT);)
}

void ShadowCascadePass::setTextureSize(uint32_t textureSize) {
    m_textureSize = textureSize;
}

void ShadowCascadePass::setLayer(uint32_t layer) {
    m_layer = layer;
}

void ShadowCascadePass::setRenderTarget(RenderLayer* pRenderLayer, Texture* pDepthArrayTexture) {
    m_pRenderLayer = pRenderLayer;
    m_pDepthArrayTexture = pDepthArrayTexture;
}
