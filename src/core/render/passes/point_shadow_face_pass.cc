#include "point_shadow_face_pass.h"

#include <GL/glew.h>

void PointShadowFacePass::setState() {
    m_pRenderLayer->setDepthTexture(m_pDepthCubeTexture, m_face);
    m_pRenderLayer->bind();

    glViewport(0, 0, m_textureSize, m_textureSize);
    glClear(GL_DEPTH_BUFFER_BIT);
}

void PointShadowFacePass::setRenderTarget(RenderLayer* pRenderLayer, Texture* pDepthCubeTexture) {
    m_pRenderLayer = pRenderLayer;
    m_pDepthCubeTexture = pDepthCubeTexture;
}

void PointShadowFacePass::setTextureSize(uint32_t textureSize) {
    m_textureSize = textureSize;
}

void PointShadowFacePass::setFace(uint32_t face) {
    m_face = face;
}
