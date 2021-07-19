#include "deferred_point_light_pass.h"

#include "core/util/mesh_builder.h"

void DeferredPointLightPass::init() {
    m_stencilVolumesShader.linkVertexShader("shaders/vertex_pt.glsl");
    m_deferredPointLightShader.linkShaderFiles("shaders/vertex_pt.glsl", "shaders/fragment_deferred_pl.glsl", "", "#version 400\n");
    m_deferredPointLightShaderShadow.linkShaderFiles("shaders/vertex_pt.glsl", "shaders/fragment_deferred_pl.glsl", "", "#version 400\n#define ENABLE_SHADOW\n");

    MeshData sphereMeshData(MeshBuilder().sphere(1.0f, 50, 25).moveMeshData());
    m_pointLightSphere.setVertexCount(sphereMeshData.vertices.size());
    m_pointLightSphere.createVertexAttribute(0, 3);
    m_pointLightSphere.allocateVertexBuffer();
    m_pointLightSphere.setVertexAttributeData(0, sphereMeshData.vertices.data());
    m_pointLightSphere.createIndexBuffer(sphereMeshData.indices.size(), sphereMeshData.indices.data());
}

void DeferredPointLightPass::onViewportResize(uint32_t width, uint32_t height) {
    m_viewportWidth = width;
    m_viewportHeight = height;
}

void DeferredPointLightPass::setState() {
    glCullFace(GL_FRONT);
    glStencilMask(0xFE);
}

void DeferredPointLightPass::render() {
    m_pointLightSphere.bind();
    for (size_t i = 0; i < m_numPointLights; ++i) {
        if (m_pPointShadowPass &&
            !m_pPointShadowPass->m_lightSpheresCuller.getCullResults()[i])
            continue;

        const PointLight& light = m_pPointLights[i];

        glm::mat4 mvp = viewProj *
                        glm::translate(glm::mat4(1.0f), light.getPosition()) *
                        glm::mat4(glm::mat3(light.getBoundingSphereRadius()));;

        // Set state for stencil render
        glDisable(GL_CULL_FACE);
        glEnable(GL_DEPTH_TEST);
        glStencilFunc(GL_ALWAYS, 0, 0xFE);
        glStencilOpSeparate(GL_BACK, GL_KEEP, GL_INCR_WRAP, GL_KEEP);
        glStencilOpSeparate(GL_FRONT, GL_KEEP, GL_DECR_WRAP, GL_KEEP);

        // Configure render layer for writing to depth-stencil attachment only
        m_pRenderLayer->setEnabledDrawTargets({});
        m_pRenderLayer->bind();

        // Render light bounds to stencil buffer
        m_stencilVolumesShader.bind();
        m_stencilVolumesShader.setUniform("modelViewProj", mvp);

        m_pointLightSphere.draw();

        // Set state for lighting pass

        glDisable(GL_DEPTH_TEST);
        glEnable(GL_CULL_FACE);
        glStencilFunc(GL_NOTEQUAL, 0, 0xFE);

        // Configure render layer for color writes
        m_pRenderLayer->setEnabledDrawTargets({0});
        m_pRenderLayer->bind();

        // Choose shader (unshadowed vs shadowed)
        int mapIndex = (m_pPointShadowPass) ? m_pPointShadowPass->m_lightShadowMapIndices[i] : -1;
        const Shader& plShader = (mapIndex < 0) ?
                                 m_deferredPointLightShader :
                                 m_deferredPointLightShaderShadow;

        plShader.bind();

        // Render lighting pass
        if (mapIndex >= 0) {
            plShader.setUniform("shadowMap", 4);
            m_pPointShadowPass->m_evsmCubeMaps[mapIndex].bind(4);
            plShader.setUniform("inverseView", viewInverse);
            plShader.setUniformArray("cubeFaceMatrices", 6, &m_pPointShadowPass->m_faceMatrices[mapIndex*6]);
            plShader.setUniform("lightBleedCorrectionBias", lightBleedCorrectionBias);
            plShader.setUniform("lightBleedCorrectionPower", lightBleedCorrectionPower);
            plShader.setUniform("enableEVSM", 1);
        }

        plShader.setUniform("gBufferDepth", 0);
        plShader.setUniform("gBufferNormalViewSpace", 1);
        plShader.setUniform("gBufferAlbedoMetallic", 2);
        plShader.setUniform("gBufferEmissionRoughness", 3);

        plShader.setUniform("inverseProjection", projectionInverse);
        plShader.setUniform("pixelSize", glm::vec2(1.0/m_viewportWidth, 1.0/m_viewportHeight));

        plShader.setUniform("minIntensity", 0.1f);
        plShader.setUniform("lightPositionViewSpace", glm::vec3(cameraViewMatrix * glm::vec4(light.getPosition(), 1.0)));
        plShader.setUniform("lightIntensity", light.getIntensity());
        plShader.setUniform("modelViewProj", mvp);

        m_pointLightSphere.draw();
    }
    glCullFace(GL_BACK);
}

void DeferredPointLightPass::cleanup() {
    m_pointLightSphere.cleanup();
    m_pPointShadowPass = nullptr;
    m_pRenderLayer = nullptr;

    m_pPointLights = nullptr;
    m_numPointLights = 0;
}

/*void DeferredPointLightPass::setPointLights(const std::vector<PointLight>& pointLights) {
    m_pointLights.assign(pointLights.begin(), pointLights.end());
}*/

void DeferredPointLightPass::setPointLights(const PointLight* pPointLights, size_t numPointLights) {
    //m_pointLights.assign(pointLights.begin(), pointLights.end());
    m_pPointLights = pPointLights;
    m_numPointLights = numPointLights;
}

void DeferredPointLightPass::setPointShadowPass(PointShadowPass* pPointShadowPass) {
    m_pPointShadowPass = pPointShadowPass;
}

void DeferredPointLightPass::setRenderLayer(RenderLayer* pRenderLayer) {
    m_pRenderLayer = pRenderLayer;
}
