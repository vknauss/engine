#include "point_shadow_pass.h"

#include <algorithm>
#include <numeric>

#include "core/render/fullscreen_quad.h"

void PointShadowPass::initForScheduler(JobScheduler* pScheduler) {
    if (pScheduler != m_pScheduler) {
        m_pScheduler = pScheduler;
        for (auto i = 0u; i < 6*m_maxPointShadowMaps; ++i) {
            m_frustumCullers[i].initForScheduler(pScheduler);
            m_facePasses[i].initForScheduler(pScheduler);
        }
    }
}

void PointShadowPass::init() {
    m_depthOnlyShader.linkVertexShader("shaders/vertex_depth.glsl");
    m_depthOnlyShaderSkinned.linkVertexShader("shaders/vertex_depth_skin.glsl");
    m_depthToVarianceShader.linkShaderFiles("shaders/vertex_fs.glsl", "shaders/fragment_evsm_cube.glsl");

    m_depthCubeMaps.resize(m_maxPointShadowMaps);
    m_evsmCubeMaps.resize(m_maxPointShadowMaps);

    for (auto i = 0u; i < m_maxPointShadowMaps; ++i) {
        TextureParameters depthTexParam = {};
        depthTexParam.cubemap = true;
        depthTexParam.useDepthComponent = true;
        depthTexParam.useEdgeClamping = true;
        depthTexParam.width = m_textureSize;
        depthTexParam.height = m_textureSize;

        m_depthCubeMaps[i].setParameters(depthTexParam);
        m_depthCubeMaps[i].allocateData(nullptr);

        TextureParameters evsmTexParam = {};
        evsmTexParam.cubemap = true;
        evsmTexParam.useFloatComponents = true;
        evsmTexParam.bitsPerComponent = 32;
        evsmTexParam.useLinearFiltering = true;
        evsmTexParam.useEdgeClamping = true;
        evsmTexParam.numComponents = 4;
        evsmTexParam.width = m_textureSize;
        evsmTexParam.height = m_textureSize;

        m_evsmCubeMaps[i].setParameters(evsmTexParam);
        m_evsmCubeMaps[i].allocateData(nullptr);
    }

    m_lightIndices.resize(m_maxPointShadowMaps);
    m_lightKeys.resize(m_maxPointShadowMaps);

    m_inUsePointShadowMaps = 0;

    m_frustumCullers.resize(6 * m_maxPointShadowMaps);
    m_frustumCullerJobParams.resize(6 * m_maxPointShadowMaps);

    m_facePasses.resize(6 * m_maxPointShadowMaps);
    m_facePassUpdateParams.resize(6 * m_maxPointShadowMaps);

    m_faceMatrices.resize(6 * m_maxPointShadowMaps);

    for (auto i = 0u; i < 6 * m_maxPointShadowMaps; ++i) {
        m_facePasses[i].setRenderTarget(&m_depthRenderLayer, &m_depthCubeMaps[i/6]);
        m_facePasses[i].setTextureSize(m_textureSize);
        m_facePasses[i].setFace((uint32_t) (i%6));
        m_facePasses[i].setShaders(&m_depthOnlyShader, &m_depthOnlyShaderSkinned);
        m_facePasses[i].init();
    }
}

void PointShadowPass::setState() {
    glDisable(GL_BLEND);
    glDisable(GL_STENCIL_TEST);

    glEnable(GL_DEPTH_CLAMP);

    glDepthMask(GL_TRUE);
    glEnable(GL_DEPTH_TEST);

    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);

    m_depthRenderLayer.setEnabledDrawTargets({});
}

void PointShadowPass::render() {
    static const glm::vec3 dir[] = {
        {1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}
    };
    static const glm::vec3 up[] = {
        {0, -1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}, {0, -1, 0}, {0, -1, 0}
    };
    static const glm::vec3 right[] = {
        {0, 0, -1}, {0, 0, 1}, {1, 0, 0}, {1, 0, 0}, {1, 0, 0}, {-1, 0, 0}
    };

    if (m_inUsePointShadowMaps > 0) {
        for (auto i = 0u; i < 6 * m_inUsePointShadowMaps; ++i) {
            m_facePasses[i].updateInstanceBuffers();
            m_facePasses[i].setState();
            m_facePasses[i].render();
        }

        glDisable(GL_DEPTH_CLAMP);

        // Convert depth to variance
        glDepthMask(GL_FALSE);
        glDisable(GL_DEPTH_TEST);

        m_depthToVarianceShader.bind();
        m_depthToVarianceShader.setUniform("enableEVSM", 1);
        m_depthToVarianceShader.setUniform("depthCubemap", 0);

        for(uint32_t i = 0; i < m_inUsePointShadowMaps; ++i) {
            m_depthCubeMaps[i].bind(0);

            for(uint32_t j = 0; j < 6; ++j) {
                m_varianceRenderLayer.setTextureAttachment(0, &m_evsmCubeMaps[i], j);
                m_varianceRenderLayer.setEnabledDrawTargets({0});
                m_varianceRenderLayer.bind();
                glClear(GL_COLOR_BUFFER_BIT);

                m_depthToVarianceShader.setUniform("faceDirection", dir[j]);
                m_depthToVarianceShader.setUniform("faceUp", up[j]);
                m_depthToVarianceShader.setUniform("faceRight", right[j]);

                FullscreenQuad::draw();
            }
        }
    }
}

void PointShadowPass::cleanup() {
    for (PointShadowFacePass& facePass : m_facePasses) {
        facePass.cleanup();
    }
    m_facePasses.clear();
    m_frustumCullers.clear();
    m_facePassUpdateParams.clear();
    m_frustumCullerJobParams.clear();
    m_faceMatrices.clear();
    m_depthCubeMaps.clear();
    m_evsmCubeMaps.clear();

    m_maxPointShadowMaps = 0;
    m_inUsePointShadowMaps = 0;

    m_pScheduler = nullptr;

    m_numPointLights = 0;
    m_pPointLights = nullptr;
}

void PointShadowPass::setMaxPointShadowMaps(uint32_t maxPointShadowMaps) {
    m_maxPointShadowMaps = maxPointShadowMaps;
}

void PointShadowPass::setTextureSize(uint32_t textureSize) {
    m_textureSize = textureSize;
}

void PointShadowPass::setCameraViewMatrix(const glm::mat4& cameraViewMatrix) {
    m_cameraViewMatrix = cameraViewMatrix;
}

void PointShadowPass::setCameraFrustumMatrix(const glm::mat4& cameraFrustumMatrix) {
    m_cameraFrustumMatrix = cameraFrustumMatrix;
}

void PointShadowPass::setPointLights(const PointLight* pPointLights, size_t numPointLights) {
    m_pPointLights = pPointLights;
    m_numPointLights = numPointLights;
}

void PointShadowPass::preRenderJob(uintptr_t param) {
    PreRenderParam* pParam = reinterpret_cast<PreRenderParam*>(param);
    PointShadowPass* pPass = pParam->pPass;

//    const std::vector<PointLight>& lights = pParam->pScene->getPointLights();
   // auto lightsView = pParam->pGameWorld->getRegistry().view<const PointLight>();
    //auto iLightsView = lightsView.each();

    // Cull the lights' bounding spheres against the main camera frustum
    // to determine which should be considered
    //std::vector<BoundingSphere> boundingSpheres(lightsView.size());

    size_t numVisibleLights = 0;
    pPass->m_inUsePointShadowMaps = 0;
    const PointLight* pLights = pPass->m_pPointLights;
    size_t numLights = pPass->m_numPointLights;
    if (numLights > 0) {
        std::vector<BoundingSphere> boundingSpheres(numLights);
        std::transform(pLights, pLights+numLights, boundingSpheres.begin(),
            [] (const PointLight& light) {
                return BoundingSphere{light.getPosition(), light.getBoundingSphereRadius()};
            });

        numVisibleLights = pPass->m_lightSpheresCuller.cullSpheres(
            boundingSpheres.data(), boundingSpheres.size(), pPass->m_cameraFrustumMatrix);

        pPass->m_lightShadowMapIndices.assign(numLights, -1);
    }

    if (numVisibleLights > 0) {
        // Sort the lights based on a key which approximates the importance of each light
        std::vector<float> keys(numLights);
        std::transform(pLights, pLights+numLights,
            pPass->m_lightSpheresCuller.getCullResults().begin(), keys.begin(),
            [&pPass] (const PointLight& light, bool cullResult) {
                if (!cullResult || !light.isShadowMapEnabled()) return -1.0f;
                glm::vec4 viewPos = pPass->m_cameraViewMatrix * glm::vec4(light.getPosition(), 1.0f);
                float key = light.getBoundingSphereRadius();
                key = key * key / glm::length(glm::vec3(viewPos));
                return key;
            });
        std::vector<uint32_t> indices(keys.size());
        std::iota(indices.begin(), indices.end(), (uint32_t) 0u);
        std::sort(indices.begin(), indices.end(),
            [&keys] (uint32_t i0, uint32_t i1) {
                return keys[i0] > keys[i1];
            });

        for (auto i = 0u; i < std::min((uint32_t)indices.size(), pPass->m_maxPointShadowMaps); ++i) {
            const PointLight& light = pLights[indices[i]];
            if (!light.isShadowMapEnabled()) break;
            pPass->m_lightIndices[i] = indices[i];
            pPass->m_lightShadowMapIndices[indices[i]] = (int) i;
            ++pPass->m_inUsePointShadowMaps;
        }

        static const glm::vec3 directions[] = {
            {1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}
        };
        static const glm::vec3 upDirs[] = {
            {0, -1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}, {0, -1, 0}, {0, -1, 0}
        };

        for (auto i = 0u; i < pPass->m_inUsePointShadowMaps; ++i) {
            uint32_t lightIndex = pPass->m_lightIndices[i];

            glm::mat4 proj = glm::perspective((float) M_PI/2.0f, 1.0f, 0.1f, pLights[lightIndex].getBoundingSphereRadius());
            glm::vec3 lightPos = pLights[lightIndex].getPosition();
            for(auto j = 0u; j < 6u; ++j) {
                pPass->m_faceMatrices[i*6u+j] = proj * glm::lookAt(lightPos, lightPos+directions[j], upDirs[j]);
            }
        }
    }

    if (pPass->m_inUsePointShadowMaps > 0) {
        std::vector<JobScheduler::JobDeclaration> cullDecls(6 * pPass->m_inUsePointShadowMaps);
        for (auto i = 0u; i < cullDecls.size(); ++i) {
            pPass->m_frustumCullerJobParams[i].frustumMatrix = pPass->m_faceMatrices[i];
            pPass->m_frustumCullerJobParams[i].pCuller       = &pPass->m_frustumCullers[i];
            //pPass->m_frustumCullerJobParams[i].pScene        = pParam->pScene;
            pPass->m_frustumCullerJobParams[i].pGameWorld    = pParam->pGameWorld;

            cullDecls[i].numSignalCounters = 1;
            cullDecls[i].signalCounters[0] = pPass->m_frustumCullers[i].getResultsReadyCounter();
            cullDecls[i].param = reinterpret_cast<uintptr_t>(&pPass->m_frustumCullerJobParams[i]);
            //cullDecls[i].pFunction = FrustumCuller::cullSceneRenderablesJob;
            cullDecls[i].pFunction = FrustumCuller::cullEntitySpheresJob;
        }
        pPass->m_pScheduler->enqueueJobs((uint32_t) cullDecls.size(), cullDecls.data());

        std::vector<JobScheduler::JobDeclaration> updateDecls(6 * pPass->m_inUsePointShadowMaps);
        for (auto i = 0u; i < updateDecls.size(); ++i) {
            pPass->m_facePassUpdateParams[i].globalMatrix  = pPass->m_faceMatrices[i];
            pPass->m_facePassUpdateParams[i].pCuller       = &pPass->m_frustumCullers[i];
            pPass->m_facePassUpdateParams[i].pPass         = &pPass->m_facePasses[i];
            //pPass->m_facePassUpdateParams[i].pScene        = pParam->pScene;
            pPass->m_facePassUpdateParams[i].pGameWorld    = pParam->pGameWorld;
            pPass->m_facePassUpdateParams[i].signalCounter = pParam->signalCounter;

            updateDecls[i].numSignalCounters = 1;
            updateDecls[i].signalCounters[0] = pParam->signalCounter;
            updateDecls[i].param = reinterpret_cast<uintptr_t>(&pPass->m_facePassUpdateParams[i]);
            updateDecls[i].pFunction = GeometryRenderPass::updateInstanceListsJob;
            updateDecls[i].waitCounter = pPass->m_frustumCullers[i].getResultsReadyCounter();
        }
        pPass->m_pScheduler->enqueueJobs((uint32_t) updateDecls.size(), updateDecls.data());
    }
}
