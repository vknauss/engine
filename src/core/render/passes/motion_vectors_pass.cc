#include "motion_vectors_pass.h"

#include "core/render/fullscreen_quad.h"

#include "core/render/render_debug.h"

void MotionVectorsPass::initForScheduler(JobScheduler* pScheduler) {
    if (pScheduler != m_pScheduler) {
        m_pScheduler = pScheduler;
        m_objectPass.initForScheduler(pScheduler);
    }
}

void MotionVectorsPass::init() {
    m_cameraMotionShader.linkShaderFiles("shaders/vertex_fs.glsl", "shaders/fragment_motion_c.glsl");

    TextureParameters motionTexParams = {};
    motionTexParams.numComponents = 2;
    motionTexParams.useEdgeClamping = true;
    motionTexParams.useFloatComponents = true;
    motionTexParams.useLinearFiltering = true;
    motionTexParams.bitsPerComponent = 16;

    RenderBufferParameters depthBufParam = {};

    m_motionBuffer.setParameters(motionTexParams);
    m_depthBuffer.setParameters(depthBufParam);

    m_renderLayer.setTextureAttachment(0, &m_motionBuffer);
    m_renderLayer.setRenderBufferAttachment(&m_depthBuffer);

    m_objectPass.init();
    m_objectPass.setRenderTarget(&m_renderLayer);
}

void MotionVectorsPass::onViewportResize(uint32_t width, uint32_t height) {
    m_viewportWidth = width;
    m_viewportHeight = height;

    TextureParameters texParam = m_motionBuffer.getParameters();
    texParam.width = width;
    texParam.height = height;
    m_motionBuffer.setParameters(texParam);
    m_motionBuffer.allocateData(nullptr);

    RenderBufferParameters rbParam = m_depthBuffer.getParameters();
    rbParam.width = width;
    rbParam.height = height;
    m_depthBuffer.setParameters(rbParam);
    m_depthBuffer.allocateData();

    m_renderLayer.setTextureAttachment(0, &m_motionBuffer);
    m_renderLayer.setRenderBufferAttachment(&m_depthBuffer);

    m_objectPass.onViewportResize(width, height);
}

void MotionVectorsPass::setState() {
    glDisable(GL_BLEND);
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);

    if (m_pBackgroundPass) {
        glEnable(GL_STENCIL_TEST);
        glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
        glStencilFunc(GL_EQUAL, 1, 0x01);  // lowest stencil bit =1 => geometry, ie depth buffer valid

        glCopyImageSubData(m_pBackgroundPass->m_motionBuffer.getHandle(), GL_TEXTURE_2D, 0, 0, 0, 0,
                           m_motionBuffer.getHandle(), GL_TEXTURE_2D, 0, 0, 0, 0,
                           m_viewportWidth, m_viewportHeight, 1);

        m_renderLayer.clearAttachment(GL_DEPTH_STENCIL_ATTACHMENT);
        m_renderLayer.setDepthTexture(m_pGBufferDepthTexture);
        m_renderLayer.setEnabledDrawTargets({0});
        m_renderLayer.validate();
        m_renderLayer.bind();
        glViewport(0, 0, m_viewportWidth, m_viewportHeight);
    } else {
        glDisable(GL_STENCIL_TEST);

        m_renderLayer.setEnabledDrawTargets({0});
        m_renderLayer.bind();
        glViewport(0, 0, m_viewportWidth, m_viewportHeight);

        glClearColor(0.0, 0.0, 0.0, 1.0);
        glClear(GL_COLOR_BUFFER_BIT);
    }
}

void MotionVectorsPass::render() {
    VKR_DEBUG_CALL(
    m_cameraMotionShader.bind();
    m_cameraMotionShader.setUniform("gBufferDepth", 0);
    m_cameraMotionShader.setUniform("lastViewProj", m_lastViewProj);
    m_cameraMotionShader.setUniform("inverseViewProj", m_viewProjInverse);
    m_pGBufferDepthTexture->bind(0);
    FullscreenQuad::draw();
    )

    VKR_DEBUG_CALL(
    if (m_pBackgroundPass) {
        m_renderLayer.clearAttachment(GL_DEPTH_STENCIL_ATTACHMENT);
        m_renderLayer.setRenderBufferAttachment(&m_depthBuffer);
    }
    )

    VKR_DEBUG_CALL(
    m_objectPass.updateInstanceBuffers();
    ) VKR_DEBUG_CALL(
    m_objectPass.setState();
    ) VKR_DEBUG_CALL(
    m_objectPass.render();
    )
}

void MotionVectorsPass::cleanup() {
    m_objectPass.cleanup();
    m_pScheduler = nullptr;
    m_pBackgroundPass = nullptr;
    m_pGBufferDepthTexture = nullptr;
}

void MotionVectorsPass::setBackgroundPass(BackgroundMotionVectorsPass* pBackgroundPass) {
    m_pBackgroundPass = pBackgroundPass;
}

void MotionVectorsPass::setGBufferDepthTexture(Texture* pGBufferDepthTexture) {
    m_pGBufferDepthTexture = pGBufferDepthTexture;
}

void MotionVectorsPass::setMatrices(const glm::mat4& lastViewProj, const glm::mat4& inverseViewProj) {
    m_lastViewProj = lastViewProj;
    m_viewProjInverse = inverseViewProj;
}

void MotionVectorsPass::preRenderJob(uintptr_t param) {
    PreRenderParam* pParam = reinterpret_cast<PreRenderParam*>(param);
    MotionVectorsPass* pPass = pParam->pPass;

    pPass->m_objectPassUpdateParam.pPass = &pPass->m_objectPass;
//    pPass->m_objectPassUpdateParam.pScene = pParam->pScene;
    pPass->m_objectPassUpdateParam.pGameWorld = pParam->pGameWorld;
    pPass->m_objectPassUpdateParam.pCuller = pParam->pCuller;
    pPass->m_objectPassUpdateParam.globalMatrix = pParam->cameraMatrix;
    pPass->m_objectPassUpdateParam.lastGlobalMatrix = pParam->lastCameraMatrix;
    pPass->m_objectPassUpdateParam.signalCounter = pParam->signalCounter;

    JobScheduler::JobDeclaration decl;
    decl.numSignalCounters = 1;
    decl.signalCounters[0] = pParam->signalCounter;
    decl.param = reinterpret_cast<uintptr_t>(&pPass->m_objectPassUpdateParam);
    decl.waitCounter = pParam->pCuller->getResultsReadyCounter();
    decl.pFunction = GeometryRenderPass::updateInstanceListsJob;

    pPass->m_pScheduler->enqueueJob(decl);
}
