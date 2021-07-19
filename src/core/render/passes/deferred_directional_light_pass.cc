#include "deferred_directional_light_pass.h"

#include "deferred_pass.h"
#include "core/render/fullscreen_quad.h"

#include <glm/gtx/string_cast.hpp>

#include "core/render/render_debug.h"

void DeferredDirectionalLightPass::init() {
    m_shader.linkShaderFiles("shaders/vertex_fs.glsl", "shaders/fragment_deferred.glsl", "", "#version 400\n");
}

void DeferredDirectionalLightPass::setState() {
    glStencilFunc(GL_NOTEQUAL, 0, 0x01);
}

void DeferredDirectionalLightPass::render() {
    VKR_DEBUG_CALL(
    m_shader.bind();
    glm::vec3 lightDirectionViewSpace = glm::normalize(glm::vec3(cameraViewMatrix * glm::vec4(lightDirection, 0.0)));
    m_shader.setUniform("lightDirectionViewSpace", lightDirectionViewSpace);
    m_shader.setUniform("lightIntensity", lightIntensity);
    )

    VKR_DEBUG_CALL(
    if (m_pShadowMapPass) {
        m_shader.setUniform("enableShadows", 1);
        m_shader.setUniform("enableEVSM", 1);  // TODO: remove this from shader
        m_shader.setUniform("lightBleedCorrectionBias", lightBleedCorrectionBias);
        m_shader.setUniform("lightBleedCorrectionPower", lightBleedCorrectionPower);
        m_shader.setUniform("numShadowCascades", m_pShadowMapPass->m_numCascades);
        m_shader.setUniformArray("shadowCascadeMatrices",
                                 m_pShadowMapPass->m_numCascades,
                                 m_pShadowMapPass->m_viewCascadeMatrices.data());
        m_shader.setUniformArray("cascadeSplitDepths",
                                 m_pShadowMapPass->m_numCascades,
                                 m_pShadowMapPass->m_cascadeSplitDepths.data());
        m_shader.setUniformArray("cascadeBlurRanges",
                                 m_pShadowMapPass->m_numCascades,
                                 m_pShadowMapPass->m_cascadeBlurRanges.data());

        m_shader.setUniform("shadowMap", 4);
        m_pShadowMapPass->m_evsmArrayTexture.bind(4);
    } else {
        m_shader.setUniform("enableShadows", 0);
    }
    )

    VKR_DEBUG_CALL(

    m_shader.setUniform("inverseProjection", projectionInverse);

    m_shader.setUniform("gBufferDepth", 0);
    m_shader.setUniform("gBufferNormalViewSpace", 1);
    m_shader.setUniform("gBufferAlbedoMetallic", 2);
    m_shader.setUniform("gBufferEmissionRoughness", 3);
    )

    VKR_DEBUG_CALL(
    FullscreenQuad::draw();
    )
}

void DeferredDirectionalLightPass::setShadowMapPass(ShadowMapPass* pShadowMapPass) {
    m_pShadowMapPass = pShadowMapPass;
}
