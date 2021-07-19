#include "core/render/passes/background_motion_vectors_pass.h"

#include "core/render/fullscreen_quad.h"

void BackgroundMotionVectorsPass::init() {
    m_shader.linkShaderFiles("shaders/vertex_motion_fs.glsl", "shaders/fragment_motion_fs.glsl");

    TextureParameters params = {};
    params.numComponents = 2;
    params.useEdgeClamping = true;
    params.useFloatComponents = true;
    params.useLinearFiltering = true;
    params.bitsPerComponent = 16;

    m_motionBuffer.setParameters(params);
}

void BackgroundMotionVectorsPass::onViewportResize(uint32_t width, uint32_t height) {
    m_viewportWidth = width;
    m_viewportHeight = height;

    TextureParameters params = m_motionBuffer.getParameters();
    params.width = width;
    params.height = height;

    m_motionBuffer.setParameters(params);
    m_motionBuffer.allocateData(nullptr);

    m_renderLayer.setTextureAttachment(0, &m_motionBuffer);
}

void BackgroundMotionVectorsPass::setState() {
    glDisable(GL_BLEND);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_STENCIL_TEST);

    m_renderLayer.setEnabledDrawTargets({0});
    m_renderLayer.bind();

    glViewport(0, 0, m_viewportWidth, m_viewportHeight);
    glClearColor(0.0, 0.0, 0.0, 1.0);
    glClear(GL_COLOR_BUFFER_BIT);
}

void BackgroundMotionVectorsPass::render() {
    glm::mat4 reprojectionMatrix = m_projection * m_lastView * m_viewInverse;

    m_shader.bind();
    m_shader.setUniform("inverseProjection", m_projectionInverse);
    m_shader.setUniform("reprojection", reprojectionMatrix);

    FullscreenQuad::draw();
}

void BackgroundMotionVectorsPass::setMatrices(const glm::mat4& projectionNoJitter,
                                              const glm::mat4& inverseProjectionNoJitter,
                                              const glm::mat4& lastView,
                                              const glm::mat4& inverseView) {
    m_projection = projectionNoJitter;
    m_projectionInverse = inverseProjectionNoJitter;
    m_lastView = lastView;
    m_viewInverse = inverseView;
}
