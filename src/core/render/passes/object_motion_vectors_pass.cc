#include "object_motion_vectors_pass.h"

#include "core/render/render_debug.h"

void ObjectMotionVectorsPass::onViewportResize(uint32_t width, uint32_t height) {
    m_viewportWidth = width;
    m_viewportHeight = height;
}

void ObjectMotionVectorsPass::setState() {
    VKR_DEBUG_CALL(
    glDisable(GL_STENCIL_TEST);

    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);

    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    )
    VKR_DEBUG_CALL(
    m_pRenderLayer->setEnabledDrawTargets({0});
    m_pRenderLayer->bind();
    )
    VKR_DEBUG_CALL(
    glViewport(0, 0, m_viewportWidth, m_viewportHeight);
    ) VKR_DEBUG_CALL(
    glClear(GL_DEPTH_BUFFER_BIT);
    )
}

void ObjectMotionVectorsPass::setRenderTarget(RenderLayer* pRenderLayer) {
    m_pRenderLayer = pRenderLayer;
}

bool ObjectMotionVectorsPass::loadShaders() {
    m_pDefaultShader = new Shader();
    m_pSkinnedShader = new Shader();

    m_pDefaultShader->linkShaderFiles("shaders/vertex_motion_o.glsl", "shaders/fragment_motion_o.glsl");
    m_pSkinnedShader->linkShaderFiles("shaders/vertex_motion_o_skin.glsl", "shaders/fragment_motion_o.glsl");

    return true;
}
