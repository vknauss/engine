#include "transparency_pass.h"

#include "core/render/render_debug.h"

void TransparencyPass::onViewportResize(uint32_t width, uint32_t height) {
    m_viewportWidth = width;
    m_viewportHeight = height;

    TextureParameters accumTexParam = m_accumTexture.getParameters();
    TextureParameters revealageTexParam = m_revealageTexture.getParameters();

    accumTexParam.width  = revealageTexParam.width  = width;
    accumTexParam.height = revealageTexParam.height = height;

    m_accumTexture.setParameters(accumTexParam);
    m_revealageTexture.setParameters(revealageTexParam);

    m_accumTexture.allocateData(nullptr);
    m_revealageTexture.allocateData(nullptr);

    m_renderLayer.setTextureAttachment(0, &m_accumTexture);
    m_renderLayer.setTextureAttachment(1, &m_revealageTexture);

    m_renderLayer.setRenderBufferAttachment(m_pDepthRenderBuffer);
}

void TransparencyPass::setState() {
    glDisable(GL_STENCIL_TEST);
    glDisable(GL_DEPTH_CLAMP);

    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);

    glEnable(GL_BLEND);

    glDisable(GL_CULL_FACE);

    // I don't know how to avoid clearing both separately...
    m_renderLayer.setEnabledDrawTargets({0});
    m_renderLayer.bind();
    glViewport(0, 0, m_viewportWidth, m_viewportHeight);
    glClearColor(0, 0, 0, 0);
    glClear(GL_COLOR_BUFFER_BIT);

    m_renderLayer.setEnabledDrawTargets({1});
    m_renderLayer.bind();
    glViewport(0, 0, m_viewportWidth, m_viewportHeight);
    glClearColor(1, 0, 0, 0);
    glClear(GL_COLOR_BUFFER_BIT);

    m_renderLayer.setEnabledDrawTargets({0, 1});
    m_renderLayer.bind();

    glBlendFunci(0, GL_ONE, GL_ONE);
    glBlendFunci(1, GL_ZERO, GL_ONE_MINUS_SRC_COLOR);
}

void TransparencyPass::setSceneDepthBuffer(RenderBuffer* pDepthRenderBuffer) {
    m_pDepthRenderBuffer = pDepthRenderBuffer;
}

void TransparencyPass::setInverseProjectionMatrix(const glm::mat4& inverseProjection) {
    m_projectionInverse = inverseProjection;
}

void TransparencyPass::setLightDirectionViewSpace(const glm::vec3& lightDirectionViewSpace) {
    m_lightDirectionViewSpace = lightDirectionViewSpace;
}

void TransparencyPass::setLightIntensity(const glm::vec3& lightIntensity) {
    m_lightIntensity = lightIntensity;
}

void TransparencyPass::setAmbientLight(const glm::vec3& ambientLight) {
    m_ambientLight = ambientLight;
}

bool TransparencyPass::loadShaders() {
    m_pDefaultShader = new Shader;
    m_pSkinnedShader = new Shader;

    m_pDefaultShader->linkShaderFiles("shaders/vertex.glsl", "shaders/fragment_transparency.glsl");
    m_pSkinnedShader->linkShaderFiles("shaders/vertex_skin.glsl", "shaders/fragment_transparency.glsl");

    return true;
}

void TransparencyPass::initRenderTargets() {
    TextureParameters accumTexParam = {};
    accumTexParam.bitsPerComponent = 16;
    accumTexParam.numComponents = 4;
    accumTexParam.useFloatComponents = true;

    TextureParameters revealageTexParam = {};
    revealageTexParam.bitsPerComponent = 8;
    revealageTexParam.numComponents = 1;

    m_accumTexture.setParameters(accumTexParam);
    m_revealageTexture.setParameters(revealageTexParam);
}

void TransparencyPass::cleanupRenderTargets() {

}

void TransparencyPass::bindMaterial(const Material* pMaterial, const Shader* pShader) {
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

void TransparencyPass::onBindShader(const Shader* pShader) {
    pShader->setUniform("diffuseTexture", 0);
    pShader->setUniform("normalsTexture", 1);
    pShader->setUniform("emissionTexture", 2);
    pShader->setUniform("metallicRoughnessTexture", 3);
    pShader->setUniform("useDiffuseTexture", 0);
    pShader->setUniform("useNormalsTexture", 0);
    pShader->setUniform("useEmissionTexture", 0);
    pShader->setUniform("useMetallicRoughnessTexture", 0);


    pShader->setUniform("inverseProjection", m_projectionInverse);
    pShader->setUniform("lightDirection", m_lightDirectionViewSpace);
    pShader->setUniform("lightIntensity", m_lightIntensity);
    pShader->setUniform("ambientLight", m_ambientLight);

    m_pBoundDiffuseTexture = nullptr;
    m_pBoundNormalsTexture = nullptr;
    m_pBoundEmissionTexture = nullptr;
    m_pBoundMetallicRoughnessTexture = nullptr;

    m_useDiffuseTexture = false;
    m_useNormalsTexture = false;
    m_useEmissionTexture = false;
    m_useMetallicRoughnessTexture = false;
}

