#include "deferred_pass.h"

#include "core/render/fullscreen_quad.h"

#include "core/render/render_debug.h"

void DeferredPass::init() {
    m_deferredUnlitShader.linkShaderFiles("shaders/vertex_fs.glsl", "shaders/fragment_deferred_ae.glsl", "", "#version 400\n");

    TextureParameters sceneTextureParameters = {};
    sceneTextureParameters.arrayLayers = 1;
    sceneTextureParameters.numComponents = 4;
    sceneTextureParameters.useFloatComponents = true;  // needed for HDR rendering
    sceneTextureParameters.useLinearFiltering = true;
    sceneTextureParameters.useEdgeClamping = true;
    m_sceneTexture.setParameters(sceneTextureParameters);
    m_renderLayer.setTextureAttachment(0, &m_sceneTexture);

    RenderBufferParameters sceneRBParameters = {};
    sceneRBParameters.hasStencilComponent = true;
    sceneRBParameters.useFloat32 = true;
    sceneRBParameters.samples = 1;
    m_depthStencilRenderBuffer.setParameters(sceneRBParameters);

    m_directionalLightPass.init();
    m_pointLightPass.init();
    m_pointLightPass.setRenderLayer(&m_renderLayer);
}

void DeferredPass::onViewportResize(uint32_t width, uint32_t height) {
    m_viewportWidth = width;
    m_viewportHeight = height;

    TextureParameters sceneTextureParameters = m_sceneTexture.getParameters();
    sceneTextureParameters.width = width;
    sceneTextureParameters.height = height;

    m_sceneTexture.setParameters(sceneTextureParameters);
    m_sceneTexture.allocateData(nullptr);

    RenderBufferParameters sceneRBParameters = m_depthStencilRenderBuffer.getParameters();
    sceneRBParameters.width = width;
    sceneRBParameters.height = height;

    m_depthStencilRenderBuffer.setParameters(sceneRBParameters);
    m_depthStencilRenderBuffer.allocateData();

    m_renderLayer.setRenderBufferAttachment(&m_depthStencilRenderBuffer);
    m_renderLayer.setTextureAttachment(0, &m_sceneTexture);

    m_directionalLightPass.onViewportResize(width, height);
    m_pointLightPass.onViewportResize(width, height);
}

void DeferredPass::setState() {
    VKR_DEBUG_CALL(
    // Copy GBuffer depth into scene depth RB
    glCopyImageSubData(m_pGBufferPass->m_gBufferDepthTexture.getHandle(), GL_TEXTURE_2D, 0, 0, 0, 0,
                       m_depthStencilRenderBuffer.getHandle(), GL_RENDERBUFFER, 0, 0, 0, 0,
                       m_viewportWidth, m_viewportHeight, 1);


    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);

    glDisable(GL_DEPTH_CLAMP);

    glEnable(GL_STENCIL_TEST);
    glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);

    // additive blending
    glEnable(GL_BLEND);
    glBlendFuncSeparate(GL_ONE, GL_ONE, GL_ONE, GL_ZERO);

    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);

    m_renderLayer.setEnabledDrawTargets({0});
    m_renderLayer.validate();
    m_renderLayer.bind();
    glViewport(0, 0, m_viewportWidth, m_viewportHeight);
    glClearColor(0, 0, 0, 0);
    glClear(GL_COLOR_BUFFER_BIT);
    )

}

void DeferredPass::render() {
    VKR_DEBUG_CALL(

    m_pGBufferPass->m_gBufferDepthTexture.bind(0);
    m_pGBufferPass->m_gBufferNormalViewSpaceTexture.bind(1);
    m_pGBufferPass->m_gBufferAlbedoMetallicTexture.bind(2);
    m_pGBufferPass->m_gBufferEmissionRoughnessTexture.bind(3);
    )

    VKR_DEBUG_CALL(
    m_directionalLightPass.setState();)
    VKR_DEBUG_CALL(
    m_directionalLightPass.render();
    )
    VKR_DEBUG_CALL(
    m_pointLightPass.setState();
    m_pointLightPass.render();
    )
    VKR_DEBUG_CALL(

    // Ambient+Emission
    // May split out into another pass, but obviously it's dummy simple so probably not
    glStencilFunc(GL_NOTEQUAL, 0, 0x01);
    glStencilMask(0xFFFF);


    m_deferredUnlitShader.bind();
    m_deferredUnlitShader.setUniform("gBufferAlbedoMetallic", 2);
    m_deferredUnlitShader.setUniform("gBufferEmissionRoughness", 3);

    if (m_pSSAOTexture) {
        m_deferredUnlitShader.setUniform("enableSSAO", 1);
        m_deferredUnlitShader.setUniform("ssaoMap", 4);
        m_pSSAOTexture->bind(4);
    } else {
        m_deferredUnlitShader.setUniform("enableSSAO", 0);
    }

    m_deferredUnlitShader.setUniform("ambientPower", m_ambientPower);
    m_deferredUnlitShader.setUniform("ambientIntensity", m_ambientLightIntensity);

    FullscreenQuad::draw();
    )
}

void DeferredPass::cleanup() {
    m_directionalLightPass.cleanup();
    m_pointLightPass.cleanup();
    m_pGBufferPass = nullptr;
    m_pSSAOTexture = nullptr;
}

void DeferredPass::setGBufferPass(GBufferPass* pGBufferPass) {
    m_pGBufferPass = pGBufferPass;
}

void DeferredPass::setShadowMapPass(ShadowMapPass* pShadowMapPass) {
    m_directionalLightPass.setShadowMapPass(pShadowMapPass);
}

void DeferredPass::setPointShadowPass(PointShadowPass* pPointShadowPass) {
    m_pointLightPass.setPointShadowPass(pPointShadowPass);
}

void DeferredPass::setSSAOTexture(Texture* pSSAOTexture) {
    m_pSSAOTexture = pSSAOTexture;
}

void DeferredPass::setLightBleedCorrection(float bias, float power) {
    m_directionalLightPass.lightBleedCorrectionBias = bias;
    m_directionalLightPass.lightBleedCorrectionPower = power;
    m_pointLightPass.lightBleedCorrectionBias = bias;
    m_pointLightPass.lightBleedCorrectionPower = power;
}

void DeferredPass::setAmbientLight(const glm::vec3& intensity, float power) {
    m_ambientLightIntensity = intensity;
    m_ambientPower = power;
}

void DeferredPass::setDirectionalLight(const glm::vec3& intensity, const glm::vec3& direction) {
    m_directionalLightPass.lightDirection = direction;
    m_directionalLightPass.lightIntensity = intensity;
}

/*void DeferredPass::setPointLights(const std::vector<PointLight>& pointLights) {
    m_pointLightPass.setPointLights(pointLights);
}*/

void DeferredPass::setPointLights(const PointLight* pPointLights, size_t numPointLights) {
    m_pointLightPass.setPointLights(pPointLights, numPointLights);
}

void DeferredPass::setMatrices(const glm::mat4& viewProjection, const glm::mat4& view, const glm::mat4& projectionInverse, const glm::mat4& viewInverse) {
    m_directionalLightPass.projectionInverse = projectionInverse;
    m_directionalLightPass.cameraViewMatrix = view;
    m_pointLightPass.projectionInverse = projectionInverse;
    m_pointLightPass.viewInverse = viewInverse;
    m_pointLightPass.viewProj = viewProjection;
    m_pointLightPass.cameraViewMatrix = view;
}
