#include "gbuffer_pass.h"


void GBufferPass::onViewportResize(uint32_t width, uint32_t height) {
    m_viewportWidth = width;
    m_viewportHeight = height;

    TextureParameters params = m_gBufferNormalViewSpaceTexture.getParameters();
    params.width = width;
    params.height = height;
    m_gBufferNormalViewSpaceTexture.setParameters(params);
    m_gBufferNormalViewSpaceTexture.allocateData(nullptr);

    params = m_gBufferAlbedoMetallicTexture.getParameters();
    params.width = width;
    params.height = height;
    m_gBufferAlbedoMetallicTexture.setParameters(params);
    m_gBufferAlbedoMetallicTexture.allocateData(nullptr);

    params = m_gBufferEmissionRoughnessTexture.getParameters();
    params.width = width;
    params.height = height;
    m_gBufferEmissionRoughnessTexture.setParameters(params);
    m_gBufferEmissionRoughnessTexture.allocateData(nullptr);

    params = m_gBufferDepthTexture.getParameters();
    params.width = width;
    params.height = height;
    m_gBufferDepthTexture.setParameters(params);
    m_gBufferDepthTexture.allocateData(nullptr);

    m_renderLayer.setDepthTexture(&m_gBufferDepthTexture);
    m_renderLayer.setTextureAttachment(0, &m_gBufferNormalViewSpaceTexture);
    m_renderLayer.setTextureAttachment(1, &m_gBufferAlbedoMetallicTexture);
    m_renderLayer.setTextureAttachment(2, &m_gBufferEmissionRoughnessTexture);

    m_renderLayer.validate();
}

void GBufferPass::setState() {
    glDisable(GL_BLEND);
    glDisable(GL_DEPTH_CLAMP);

    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);

    glEnable(GL_STENCIL_TEST);
    glStencilFunc(GL_ALWAYS, 1, 0x01);
    glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
    glStencilMask(0xFFFFFFFF);

    glDisable(GL_CULL_FACE);
    glCullFace(GL_BACK);

    m_renderLayer.setEnabledDrawTargets({0, 1, 2});
    m_renderLayer.bind();

    glViewport(0, 0, m_viewportWidth, m_viewportHeight);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
}

bool GBufferPass::loadShaders() {
    m_pDefaultShader = new Shader();
    m_pSkinnedShader = new Shader();

    m_pDefaultShader->linkShaderFiles("shaders/vertex.glsl", "shaders/fragment_gbuffer.glsl");
    m_pSkinnedShader->linkShaderFiles("shaders/vertex_skin.glsl", "shaders/fragment_gbuffer.glsl");

    return true;
}

void GBufferPass::initRenderTargets() {
    TextureParameters gBufferNormalTextureParameters = {};
    gBufferNormalTextureParameters.numComponents = 3;
    gBufferNormalTextureParameters.bitsPerComponent = 16;
    gBufferNormalTextureParameters.useEdgeClamping = true;
    gBufferNormalTextureParameters.useFloatComponents = false;
    gBufferNormalTextureParameters.bitsPerComponent = 16;

    m_gBufferNormalViewSpaceTexture.setParameters(gBufferNormalTextureParameters);

    TextureParameters gBufferColorTextureParameters = {};
    gBufferColorTextureParameters.numComponents = 4;
    gBufferColorTextureParameters.useEdgeClamping = true;
    gBufferColorTextureParameters.useFloatComponents = false;

    m_gBufferAlbedoMetallicTexture.setParameters(gBufferColorTextureParameters);

    TextureParameters gBufferEmissionTextureParameters = {};
    gBufferEmissionTextureParameters.numComponents = 4;
    gBufferEmissionTextureParameters.useEdgeClamping = true;
    gBufferEmissionTextureParameters.useFloatComponents = true;

    m_gBufferEmissionRoughnessTexture.setParameters(gBufferEmissionTextureParameters);

    TextureParameters gBufferDepthTexParams = {};
    gBufferDepthTexParams.useFloatComponents = true;
    gBufferDepthTexParams.bitsPerComponent = 32;
    gBufferDepthTexParams.useDepthComponent = true;
    gBufferDepthTexParams.useStencilComponent = true;
    gBufferDepthTexParams.useEdgeClamping = true;
    m_gBufferDepthTexture.setParameters(gBufferDepthTexParams);
}

void GBufferPass::cleanupRenderTargets() {
    // For now render targets are statically allocated
}

void GBufferPass::bindMaterial(const Material* pMaterial, const Shader* pShader) {
    if (pMaterial) {
        pShader->setUniform("color", pMaterial->getTintColor());
        pShader->setUniform("roughness", pMaterial->getRoughness());
        pShader->setUniform("metallic", pMaterial->getMetallic());
        pShader->setUniform("emission", pMaterial->getEmissionIntensity());
        pShader->setUniform("alpha", pMaterial->getAlpha());
        pShader->setUniform("alphaMaskThreshold", pMaterial->getAlphaMaskThreshold());

        if (pMaterial->getDiffuseTexture() != nullptr) {
            if (!m_useDiffuseTexture) {
                pShader->setUniform("useDiffuseTexture", 1);
                m_useDiffuseTexture = true;
            }
            if (pMaterial->getDiffuseTexture() != m_pBoundDiffuseTexture) {
                pMaterial->getDiffuseTexture()->bind(0);
                m_pBoundDiffuseTexture = pMaterial->getDiffuseTexture();
            }
        } else if (m_useDiffuseTexture){
            pShader->setUniform("useDiffuseTexture", 0);
            m_useDiffuseTexture = false;
        }

        if (pMaterial->getNormalsTexture() != nullptr) {
            if (!m_useNormalsTexture) {
                pShader->setUniform("useNormalsTexture", 1);
                m_useNormalsTexture = true;
            }
            if (pMaterial->getNormalsTexture() != m_pBoundNormalsTexture) {
                pMaterial->getNormalsTexture()->bind(1);
                m_pBoundNormalsTexture = pMaterial->getNormalsTexture();
            }
        } else if (m_useNormalsTexture) {
            pShader->setUniform("useNormalsTexture", 0);
            m_useNormalsTexture = false;
        }

        if (pMaterial->getEmissionTexture() != nullptr) {
            if (!m_useEmissionTexture) {
                pShader->setUniform("useEmissionTexture", 1);
                m_useEmissionTexture = true;
            }
            if (pMaterial->getEmissionTexture() != m_pBoundEmissionTexture) {
                pMaterial->getEmissionTexture()->bind(2);
                m_pBoundEmissionTexture = pMaterial->getEmissionTexture();
            }
        } else if (m_useEmissionTexture) {
            pShader->setUniform("useEmissionTexture", 0);
            m_useEmissionTexture = false;
        }

        if (pMaterial->getMetallicRoughnessTexture() != nullptr) {
            if (!m_useMetallicRoughnessTexture) {
                pShader->setUniform("useMetallicRoughnessTexture", 1);
                m_useMetallicRoughnessTexture = true;
            }
            if (pMaterial->getMetallicRoughnessTexture() != m_pBoundMetallicRoughnessTexture) {
                pMaterial->getMetallicRoughnessTexture()->bind(3);
                m_pBoundMetallicRoughnessTexture = pMaterial->getMetallicRoughnessTexture();
            }
        } else if (m_useMetallicRoughnessTexture) {
            pShader->setUniform("useMetallicRoughnessTexture", 0);
            m_useMetallicRoughnessTexture = false;
        }
    } else {
        if (m_useDiffuseTexture){
            pShader->setUniform("useDiffuseTexture", 0);
            m_useDiffuseTexture = false;
        }
        if (m_useNormalsTexture) {
            pShader->setUniform("useNormalsTexture", 0);
            m_useNormalsTexture = false;
        }
        if (m_useEmissionTexture) {
            pShader->setUniform("useEmissionTexture", 0);
            m_useEmissionTexture = false;
        }
        if (m_useMetallicRoughnessTexture) {
            pShader->setUniform("useMetallicRoughnessTexture", 0);
            m_useMetallicRoughnessTexture = false;
        }
        pShader->setUniform("color", glm::vec3(1.0, 0.0, 1.0));
        pShader->setUniform("metallic", 0.0f);
        pShader->setUniform("roughness", 1.0f);
        pShader->setUniform("emission", glm::vec3(0.0));
    }
}

void GBufferPass::onBindShader(const Shader* pShader) {
    pShader->setUniform("diffuseTexture", 0);
    pShader->setUniform("normalsTexture", 1);
    pShader->setUniform("emissionTexture", 2);
    pShader->setUniform("metallicRoughnessTexture", 3);
    pShader->setUniform("useDiffuseTexture", 0);
    pShader->setUniform("useNormalsTexture", 0);
    pShader->setUniform("useEmissionTexture", 0);
    pShader->setUniform("useMetallicRoughnessTexture", 0);

    m_pBoundDiffuseTexture = nullptr;
    m_pBoundNormalsTexture = nullptr;
    m_pBoundEmissionTexture = nullptr;
    m_pBoundMetallicRoughnessTexture = nullptr;

    m_useDiffuseTexture = false;
    m_useNormalsTexture = false;
    m_useEmissionTexture = false;
    m_useMetallicRoughnessTexture = false;
}

