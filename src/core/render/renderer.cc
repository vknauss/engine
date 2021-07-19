#include "renderer.h"

#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/string_cast.hpp>

#include "core/render/fullscreen_quad.h"

#include "core/render/render_debug.h"

Renderer::Renderer(JobScheduler* pScheduler) :
    m_pScheduler(pScheduler) {
}

Renderer::~Renderer() {
    if (m_initialized) cleanup();
}

void Renderer::init(uint32_t width, uint32_t height) {
    // Set pass parameters
    m_bloomPass.numLevels = 3;
    m_motionBlurPass.copyToSceneTexture = false;
    m_pointShadowPass.setMaxPointShadowMaps(5);
    m_pointShadowPass.setTextureSize(512);
    m_shadowMapPass.setNumCascades(4);
    m_shadowMapPass.setTextureSize(1024);
    m_shadowMapPass.setFilterTextureSize(512);
    m_shadowMapPass.setMaxDistance(40.0f);
    m_shadowMapPass.setCascadeScale(0.25f);
    m_shadowMapPass.setCascadeBlurSize(0.3f);

    // Initialize passes
    m_backgroundMotionVectorsPass.init();
    m_bloomPass.init();
    m_deferredPass.init();
    m_gBufferPass.init();
    m_motionBlurPass.init();
    m_motionVectorsPass.init();
    m_pointShadowPass.init();
    m_shadowMapPass.init();
    m_ssaoPass.init();
    m_taaPass.init();
    m_transparencyPass.init();
    m_transparencyCompositePass.init();
    m_volumetricCloudsPass.init();

    // Initialize jobs
    m_frustumCuller.initForScheduler(m_pScheduler);

    m_gBufferPass.initForScheduler(m_pScheduler);
    m_motionVectorsPass.initForScheduler(m_pScheduler);
    m_pointShadowPass.initForScheduler(m_pScheduler);
    m_shadowMapPass.initForScheduler(m_pScheduler);
    m_transparencyPass.initForScheduler(m_pScheduler);

    // Set pass dependencies
    m_bloomPass.setSceneRenderLayer(m_deferredPass.getSceneRenderLayer(),
                                    m_deferredPass.getSceneTexture());

    m_deferredPass.setGBufferPass(&m_gBufferPass);
    m_deferredPass.setPointShadowPass(&m_pointShadowPass);
    m_deferredPass.setShadowMapPass(&m_shadowMapPass);
    m_deferredPass.setSSAOTexture(m_ssaoPass.getRenderTexture());

    m_motionBlurPass.setGBufferDepth(m_gBufferPass.getGBufferDepth());
    m_motionBlurPass.setMotionBuffer(m_motionVectorsPass.getMotionBuffer());
    m_motionBlurPass.setSceneTexture(m_deferredPass.getSceneTexture());

    m_motionVectorsPass.setBackgroundPass(&m_backgroundMotionVectorsPass);
    m_motionVectorsPass.setGBufferDepthTexture(m_gBufferPass.getGBufferDepth());

    m_ssaoPass.setGBufferTextures(m_gBufferPass.getGBufferDepth(),
                                  m_gBufferPass.getGBufferNormals());

    m_taaPass.setMotionBlurTexture(m_motionBlurPass.getRenderTexture());
    m_taaPass.setMotionBuffer(m_motionVectorsPass.getMotionBuffer());
    m_taaPass.setSceneRenderLayer(m_deferredPass.getSceneRenderLayer());
    m_taaPass.setSceneTexture(m_deferredPass.getSceneTexture());

    m_transparencyPass.setSceneDepthBuffer(m_deferredPass.getSceneRenderBuffer());

    m_transparencyCompositePass.setOutputRenderLayer(m_deferredPass.getSceneRenderLayer());
    m_transparencyCompositePass.setTransparencyRenderTextures(m_transparencyPass.getAccumTexture(),
                                                              m_transparencyPass.getRevealageTexture());

    m_volumetricCloudsPass.setMotionBuffer(m_backgroundMotionVectorsPass.getMotionBuffer());
    m_volumetricCloudsPass.setOutputRenderTarget(m_deferredPass.getSceneRenderLayer());

    // Finish initializing fullscreen passes
    setViewport(width, height);

    // OpenGL context settings
    // Will be overwritten but just a basis
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_MULTISAMPLE);
    glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);

    m_initialized = true;
}

void Renderer::setRenderToTexture(bool enabled) {
    if (enabled && !m_pRenderTexture) {
        m_pRenderTexture = new Texture;
        TextureParameters param = {};
        param.numComponents = 4;
        param.useEdgeClamping = false;
        param.useLinearFiltering = true;
       // param.useMipmapFiltering = true;
        //param.useAnisotropicFiltering = true;
        if (m_viewportInitialized) {
            param.width = m_viewportWidth;
            param.height = m_viewportHeight;
        }
        m_pRenderTexture->setParameters(param);
        if (m_viewportInitialized) {
            m_pRenderTexture->allocateData(nullptr);
            m_renderToTextureLayer.setTextureAttachment(0, m_pRenderTexture);
        }
    } else if (!enabled && m_pRenderTexture) {
        delete m_pRenderTexture;
        m_pRenderTexture = nullptr;
        m_renderToTextureLayer.clearAttachment(GL_COLOR_ATTACHMENT0);
    }
}

void Renderer::cleanup() {
    m_backgroundMotionVectorsPass.cleanup();
    m_bloomPass.cleanup();
    m_deferredPass.cleanup();
    m_gBufferPass.cleanup();
    m_motionBlurPass.cleanup();
    m_motionVectorsPass.cleanup();
    m_pointShadowPass.cleanup();
    m_shadowMapPass.cleanup();
    m_ssaoPass.cleanup();
    m_taaPass.cleanup();
    m_transparencyPass.cleanup();
    m_transparencyCompositePass.cleanup();
    m_volumetricCloudsPass.cleanup();

    if (m_pRenderTexture) {
        delete m_pRenderTexture;
        m_pRenderTexture = nullptr;
        m_renderToTextureLayer.clearAttachment(GL_COLOR_ATTACHMENT0);
    }

    m_initialized = false;
}


void Renderer::setViewport(uint32_t width, uint32_t height) {
    if (m_viewportInitialized &&
        m_viewportWidth == width &&
        m_viewportHeight == height)
            return;

    m_viewportWidth = width;
    m_viewportHeight = height;

    m_backgroundMotionVectorsPass.onViewportResize(width, height);
    m_bloomPass.onViewportResize(width, height);
    m_deferredPass.onViewportResize(width, height);
    m_gBufferPass.onViewportResize(width, height);
    m_motionBlurPass.onViewportResize(width, height);
    m_motionVectorsPass.onViewportResize(width, height);
    m_pointShadowPass.onViewportResize(width, height);
    m_shadowMapPass.onViewportResize(width, height);
    m_ssaoPass.onViewportResize(width, height);
    m_taaPass.onViewportResize(width, height);
    m_transparencyPass.onViewportResize(width, height);
    m_transparencyCompositePass.onViewportResize(width, height);
    m_volumetricCloudsPass.onViewportResize(width, height);

    if (m_pRenderTexture) {
        TextureParameters param = m_pRenderTexture->getParameters();
        param.width = width;
        param.height = height;
        m_pRenderTexture->setParameters(param);
        m_pRenderTexture->allocateData(nullptr);
        m_renderToTextureLayer.setTextureAttachment(0, m_pRenderTexture);
    }

    m_viewportInitialized = true;
}

void Renderer::renderJob(uintptr_t param) {
    RendererJobParam* pParam = reinterpret_cast<RendererJobParam*>(param);

    pParam->pWindow->acquireContext();

    pParam->pRenderer->render();

    if (pParam->pAfterSceneRenderCallback) {
        pParam->pAfterSceneRenderCallback();
    }

    pParam->pWindow->releaseContext();
}

void Renderer::render() {
    VKR_DEBUG_CALL(
    m_gBufferPass.updateInstanceBuffers();
    m_gBufferPass.setState();
    m_gBufferPass.render();
    )

    VKR_DEBUG_CALL(
    m_shadowMapPass.setState();
    m_shadowMapPass.render();
    )

    VKR_DEBUG_CALL(
    m_pointShadowPass.setState();
    m_pointShadowPass.render();
    )

    VKR_DEBUG_CALL(
    m_ssaoPass.setState();
    m_ssaoPass.render();
    )

    VKR_DEBUG_CALL(
    m_backgroundMotionVectorsPass.setState();
    m_backgroundMotionVectorsPass.render();
    )

    VKR_DEBUG_CALL(
    m_deferredPass.setState();
    m_deferredPass.render();
    )

    VKR_DEBUG_CALL(
    m_volumetricCloudsPass.setState();
    m_volumetricCloudsPass.render();
    )

    VKR_DEBUG_CALL(
    m_motionVectorsPass.setState();)
    VKR_DEBUG_CALL(
    m_motionVectorsPass.render();
    )

    VKR_DEBUG_CALL(
    m_transparencyPass.updateInstanceBuffers();
    m_transparencyPass.setState();
    m_transparencyPass.render();
    )

    VKR_DEBUG_CALL(
    m_transparencyCompositePass.setState();
    m_transparencyCompositePass.render();
    )

    VKR_DEBUG_CALL(
    m_motionBlurPass.setState();
    m_motionBlurPass.render();
    )

    VKR_DEBUG_CALL(
    m_taaPass.setState();
    m_taaPass.render();
    )

    VKR_DEBUG_CALL(
    m_bloomPass.setState();
    m_bloomPass.render();
    )

    VKR_DEBUG_CALL(
    if (m_pRenderTexture) {
        m_renderToTextureLayer.setEnabledDrawTargets({0});
        m_renderToTextureLayer.bind();
    } else {
        RenderLayer::unbind();
    }
    )

    VKR_DEBUG_CALL(
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_STENCIL_TEST);

    glViewport(0, 0, m_viewportWidth, m_viewportHeight);
    glClear(GL_COLOR_BUFFER_BIT);
    )

    FullscreenQuad::DrawTexturedParam param;
    param.gamma = true;
    param.toneMap = true;
    param.exposure = 0.8f;

    VKR_DEBUG_CALL(
    FullscreenQuad::drawTextured(m_deferredPass.getSceneTexture(), param);
    )

    RenderLayer::unbind();
}

void Renderer::preRenderJob(uintptr_t param) {
    RendererJobParam* pParam = reinterpret_cast<RendererJobParam*>(param);

    Renderer* pRenderer = pParam->pRenderer;
//    const Scene* pScene = pParam->pScene;
    const GameWorld* pGameWorld = pParam->pGameWorld;
    const Camera* pCamera = pParam->pCamera;
    JobScheduler* pScheduler = pParam->pScheduler;

    assert(pRenderer->m_initialized);
    assert(pRenderer->m_viewportInitialized);
    assert(pCamera);

    //pRenderer->computeMatrices(pScene->getActiveCamera());
    pRenderer->computeMatrices(pCamera);

//    pRenderer->updatePasses(pScene);
    auto pointLightsView = pGameWorld->getRegistry().view<const PointLight>();
    pRenderer->updatePasses(pCamera, pParam->pDirectionalLight, pParam->ambientLightIntensity,
                            pointLightsView.empty() ? nullptr : *pointLightsView.raw(), pointLightsView.size());

    // Dispatch culling job
    /*pRenderer->m_cullSceneParam.frustumMatrix = pRenderer->m_viewProj;
    pRenderer->m_cullSceneParam.pCuller = &pRenderer->m_frustumCuller;
    pRenderer->m_cullSceneParam.pScene = pScene;*/
    pRenderer->m_cullParam.frustumMatrix = pRenderer->m_viewProj;
    pRenderer->m_cullParam.pCuller = &pRenderer->m_frustumCuller;
    pRenderer->m_cullParam.pGameWorld = pGameWorld;

    JobScheduler::JobDeclaration cullDecl;
    cullDecl.numSignalCounters = 1;
    cullDecl.signalCounters[0] = pRenderer->m_frustumCuller.getResultsReadyCounter();
    cullDecl.param = reinterpret_cast<uintptr_t>(&pRenderer->m_cullParam);
    cullDecl.pFunction = FrustumCuller::cullEntitySpheresJob;

    pScheduler->enqueueJob(cullDecl);

    JobScheduler::JobDeclaration decl;

    // Update point shadows pass
    pRenderer->m_pointShadowsPreRenderParam.pPass = &pRenderer->m_pointShadowPass;
//    pRenderer->m_pointShadowsPreRenderParam.pScene = pScene;
    pRenderer->m_pointShadowsPreRenderParam.pGameWorld = pGameWorld;
    pRenderer->m_pointShadowsPreRenderParam.signalCounter = pParam->signalCounterHandle;

    decl = JobScheduler::JobDeclaration();
    decl.numSignalCounters = 1;
    decl.signalCounters[0] = pParam->signalCounterHandle;
    decl.param = reinterpret_cast<uintptr_t>(&pRenderer->m_pointShadowsPreRenderParam);
    decl.pFunction = PointShadowPass::preRenderJob;

    pScheduler->enqueueJob(decl);

    // Update directional shadows pass
    pRenderer->m_shadowMapPreRenderParam.pPass = &pRenderer->m_shadowMapPass;
    //pRenderer->m_shadowMapPreRenderParam.pScene = pScene;
    pRenderer->m_shadowMapPreRenderParam.pGameWorld = pGameWorld;
    pRenderer->m_shadowMapPreRenderParam.pCamera = pCamera;
    pRenderer->m_shadowMapPreRenderParam.signalCounter = pParam->signalCounterHandle;

    decl = JobScheduler::JobDeclaration();
    decl.numSignalCounters = 1;
    decl.signalCounters[0] = pParam->signalCounterHandle;
    decl.param = reinterpret_cast<uintptr_t>(&pRenderer->m_shadowMapPreRenderParam);
    decl.pFunction = ShadowMapPass::preRenderJob;

    pScheduler->enqueueJob(decl);

    // Update Motion Vectors Pass
    pRenderer->m_motionVectorsPreRenderParam.cameraMatrix = pRenderer->m_viewProj;
    pRenderer->m_motionVectorsPreRenderParam.lastCameraMatrix = pRenderer->m_lastViewProj;
    pRenderer->m_motionVectorsPreRenderParam.pCuller = &pRenderer->m_frustumCuller;
    pRenderer->m_motionVectorsPreRenderParam.pPass = &pRenderer->m_motionVectorsPass;
    //pRenderer->m_motionVectorsPreRenderParam.pScene = pScene;
    pRenderer->m_motionVectorsPreRenderParam.pGameWorld = pGameWorld;
    pRenderer->m_motionVectorsPreRenderParam.signalCounter = pParam->signalCounterHandle;

    decl = JobScheduler::JobDeclaration();
    decl.numSignalCounters = 1;
    decl.signalCounters[0] = pParam->signalCounterHandle;
    decl.waitCounter = cullDecl.signalCounters[0];
    decl.param = reinterpret_cast<uintptr_t>(&pRenderer->m_motionVectorsPreRenderParam);
    decl.pFunction = MotionVectorsPass::preRenderJob;

    pScheduler->enqueueJob(decl);

    // Update GBuffer Pass
    pRenderer->m_gBufferUpdateParam.globalMatrix = pRenderer->m_viewProj;
    pRenderer->m_gBufferUpdateParam.normalsMatrix = pRenderer->m_viewNormals;
    pRenderer->m_gBufferUpdateParam.pCuller = &pRenderer->m_frustumCuller;
    pRenderer->m_gBufferUpdateParam.pPass = &pRenderer->m_gBufferPass;
    //pRenderer->m_gBufferUpdateParam.pScene = pScene;
    pRenderer->m_gBufferUpdateParam.pGameWorld = pGameWorld;
    pRenderer->m_gBufferUpdateParam.signalCounter = pParam->signalCounterHandle;

    decl = JobScheduler::JobDeclaration();
    decl.numSignalCounters = 1;
    decl.signalCounters[0] = pParam->signalCounterHandle;
    decl.waitCounter = cullDecl.signalCounters[0];
    decl.param = reinterpret_cast<uintptr_t>(&pRenderer->m_gBufferUpdateParam);
    decl.pFunction = GeometryRenderPass::updateInstanceListsJob;

    pScheduler->enqueueJob(decl);

    // Update Transparency Pass
    pRenderer->m_transparencyUpdateParam.globalMatrix = pRenderer->m_viewProj;
    pRenderer->m_transparencyUpdateParam.normalsMatrix = pRenderer->m_viewNormals;
    pRenderer->m_transparencyUpdateParam.pCuller = &pRenderer->m_frustumCuller;
    pRenderer->m_transparencyUpdateParam.pPass = &pRenderer->m_transparencyPass;
    //pRenderer->m_transparencyUpdateParam.pScene = pScene;
    pRenderer->m_transparencyUpdateParam.pGameWorld = pGameWorld;
    pRenderer->m_transparencyUpdateParam.signalCounter = pParam->signalCounterHandle;

    decl = JobScheduler::JobDeclaration();
    decl.numSignalCounters = 1;
    decl.signalCounters[0] = pParam->signalCounterHandle;
    decl.waitCounter = cullDecl.signalCounters[0];
    decl.param = reinterpret_cast<uintptr_t>(&pRenderer->m_transparencyUpdateParam);
    decl.pFunction = GeometryRenderPass::updateInstanceListsJob;

    pScheduler->enqueueJob(decl);
}

//void Renderer::updatePasses(const Scene* pScene) {
void Renderer::updatePasses(const Camera* pCamera,
                            const DirectionalLight* pDirectionalLight,
                            const glm::vec3& ambientLightIntensity,
                            const PointLight* const pPointLights,
                            size_t numPointLights) {

    const glm::vec3& lightDirection = pDirectionalLight->getDirection();

    m_backgroundMotionVectorsPass.setMatrices(m_projectionNoJitter,
                                              m_projectionNoJitterInverse,
                                              m_lastView,
                                              m_viewInverse);

    m_deferredPass.setAmbientLight(ambientLightIntensity, 2.0f);
    m_deferredPass.setDirectionalLight(pDirectionalLight->getIntensity(),
                                       lightDirection);
    m_deferredPass.setLightBleedCorrection(0.0, 1.0);
    //m_deferredPass.setPointLights(pScene->getPointLights());
    m_deferredPass.setPointLights(pPointLights, numPointLights);
    m_deferredPass.setMatrices(m_viewProj, m_cameraViewMatrix,
                               m_projectionInverse, m_viewInverse);

    m_motionVectorsPass.setMatrices(m_lastViewProj, m_inverseViewProj);

    m_pointShadowPass.setCameraFrustumMatrix(m_viewProj);
    m_pointShadowPass.setCameraViewMatrix(m_cameraViewMatrix);
    m_pointShadowPass.setPointLights(pPointLights, numPointLights);

    glm::mat4 lightViewMatrix = glm::lookAt(glm::vec3(0),
                                            glm::vec3(0) + lightDirection,
                                            (glm::abs(lightDirection.y) < 0.99) ?
                                                glm::vec3(0, 1, 0) :
                                                glm::vec3(0, 0, 1));
    m_shadowMapPass.setMatrices(m_viewInverse, lightViewMatrix);

    m_ssaoPass.setMatrices(m_cameraProjectionMatrix, m_projectionInverse);

    m_transparencyPass.setInverseProjectionMatrix(m_projectionInverse);
    m_transparencyPass.setLightDirectionViewSpace(glm::normalize(glm::vec3(m_cameraViewMatrix *
                                                            glm::vec4(lightDirection, 0.0))));
    m_transparencyPass.setLightIntensity(pDirectionalLight->getIntensity());
    m_transparencyPass.setAmbientLight(ambientLightIntensity);

    m_volumetricCloudsPass.setCamera(pCamera);
    m_volumetricCloudsPass.setDirectionalLight(pDirectionalLight->getIntensity(),
                                               lightDirection);
    m_volumetricCloudsPass.setInverseViewMatrix(m_viewInverse);
}

void Renderer::computeMatrices(const Camera* pCamera) {
    m_lastView = m_cameraViewMatrix;
    m_lastViewProj = m_viewProj;

    m_cameraViewMatrix = pCamera->calculateViewMatrix();
    m_projectionNoJitter = pCamera->calculateProjectionMatrix();

    m_cameraProjectionMatrix =
        glm::translate(glm::mat4(1.0), glm::vec3(m_taaPass.cJitter, 0.0)) *
        m_projectionNoJitter;
    //m_cameraProjectionMatrix = m_projectionNoJitter;

    m_viewProj = m_cameraProjectionMatrix * m_cameraViewMatrix;
    m_inverseViewProj = glm::inverse(m_viewProj);

    m_viewInverse = glm::inverse(m_cameraViewMatrix);
    m_viewNormals = glm::transpose(m_viewInverse);

    m_projectionInverse = glm::inverse(m_cameraProjectionMatrix);
    m_projectionNoJitterInverse = glm::inverse(m_projectionNoJitter);
}
