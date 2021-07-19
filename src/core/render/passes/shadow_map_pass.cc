#include "shadow_map_pass.h"

#include "core/render/fullscreen_quad.h"
#include "core/util/math_util.h"

#include "core/render/render_debug.h"

void ShadowMapPass::initForScheduler(JobScheduler* pScheduler) {
    if (m_pScheduler != pScheduler) {
        m_pScheduler = pScheduler;
        for (auto i = 0u; i < m_numCascades; ++i) {
            m_cascadeFrustumCullers[i].initForScheduler(pScheduler);
            m_cascadePasses[i].initForScheduler(pScheduler);
        }
    }
}

/*static void printTextureParameters(const TextureParameters& parameters) {
    std::cout <<
    "width" << parameters.width << " " << &parameters.width << std::endl <<
    "height" << parameters.height<< " " << &parameters.height << std::endl <<
    "arrayLayers" << parameters.arrayLayers<< " " << &parameters.arrayLayers << std::endl <<
    "numComponents" << parameters.numComponents<< " " << &parameters.numComponents << std::endl <<
    "bitsPerComponent" << parameters.bitsPerComponent<< " " << &parameters.bitsPerComponent << std::endl <<
    "samples" << parameters.samples<< " " << &parameters.samples << std::endl <<
    "cubemap" << parameters.cubemap<< " " << &parameters.cubemap << std::endl <<
    "isBGR" << parameters.isBGR<< " " << &parameters.isBGR << std::endl <<
    "useFloatComponents" << parameters.useFloatComponents<< " " << &parameters.useFloatComponents << std::endl <<
    "useLinearFiltering" << parameters.useLinearFiltering<< " " << &parameters.useLinearFiltering << std::endl <<
    "useMipmapFiltering" << parameters.useMipmapFiltering<< " " << &parameters.useMipmapFiltering << std::endl <<
    "useAnisotropicFiltering" << parameters.useAnisotropicFiltering<< " " << &parameters.useAnisotropicFiltering << std::endl <<
    "useEdgeClamping" << parameters.useEdgeClamping<< " " << &parameters.useEdgeClamping << std::endl <<
    "useDepthComponent" << parameters.useDepthComponent<< " " << &parameters.useDepthComponent << std::endl <<
    "useStencilComponent" << parameters.useStencilComponent<< " " << &parameters.useStencilComponent << std::endl <<
    "is3D" << parameters.is3D<< " " << &parameters.is3D << std::endl;
}*/

void ShadowMapPass::init() {
    // Load Shaders
    m_depthOnlyShader.linkVertexShader("shaders/vertex_depth.glsl");
    m_depthOnlyShaderSkinned.linkVertexShader("shaders/vertex_depth_skin.glsl");
    m_depthToVarianceShader.linkShaderFiles("shaders/vertex_fs.glsl", "shaders/fragment_evsm.glsl");

    // 5-tap gaussian: [0.06136, 0.24477, 0.38774, 0.24477, 0.06136]
    // half: [0.38774, 0.24477, 0.06136]
    // combine weights: [0.38774, 0.24477 + 0.06136] = [0.38774, 0.30613]
    // offsets: [0.0, lerp(1.0, 2.0, 0.06136 / 0.30613)] = [0.0, 1.20043772253]
    const char* filterHeader =
        "#version 330\n"
        "const int WIDTH = 2;\n"
        "const float WEIGHTS[2] = float[2](0.38774, 0.30613);"
        "const float OFFSETS[2] = float[2](0.0, 1.20043772253);";

    m_filterShader.linkShaderFiles("shaders/vertex_fs.glsl", "shaders/fragment_filter_stub.glsl", "", filterHeader);


    // Init Render Targets

    // Depth
    TextureParameters depthTextureParameters = {};
    depthTextureParameters.arrayLayers = m_numCascades;
    depthTextureParameters.useDepthComponent = true;
    depthTextureParameters.useFloatComponents = false;
    depthTextureParameters.bitsPerComponent = 1000;
    depthTextureParameters.useEdgeClamping = true;
    depthTextureParameters.useLinearFiltering = true;
    depthTextureParameters.width = m_textureSize;
    depthTextureParameters.height = m_textureSize;
    depthTextureParameters.useStencilComponent = false;
    m_depthArrayTexture.setParameters(depthTextureParameters);
    m_depthArrayTexture.allocateData(nullptr);

    // Variance (EVSM)
    TextureParameters shadowMapArrayTextureParameters = {};
    shadowMapArrayTextureParameters.numComponents = 4;
    shadowMapArrayTextureParameters.arrayLayers = m_numCascades;
    shadowMapArrayTextureParameters.width = m_textureSize;
    shadowMapArrayTextureParameters.height = m_textureSize;
    shadowMapArrayTextureParameters.useEdgeClamping = true;
    shadowMapArrayTextureParameters.useFloatComponents = true;
    shadowMapArrayTextureParameters.bitsPerComponent = 32;
    shadowMapArrayTextureParameters.useLinearFiltering = true;
    shadowMapArrayTextureParameters.useMipmapFiltering = false;
    shadowMapArrayTextureParameters.useAnisotropicFiltering = false;  // mipmapping and anisotropic I couldn't get to work in the deferred renderer :(
    m_evsmArrayTexture.setParameters(shadowMapArrayTextureParameters);
    m_evsmArrayTexture.allocateData(nullptr);

    for(uint32_t i = 0; i < m_numCascades; ++i) {
        //m_depthRenderLayer.setTextureAttachment(i, &m_evsmArrayTexture, i);
        m_varianceRenderLayer.setTextureAttachment(i, &m_evsmArrayTexture, i);
    }

    // Filtering
    TextureParameters shadowMapFilterTextureParameters = {};
    shadowMapFilterTextureParameters.numComponents = 4;
    shadowMapFilterTextureParameters.arrayLayers = 1;
    shadowMapFilterTextureParameters.width = m_filterTextureSize;
    shadowMapFilterTextureParameters.height = m_filterTextureSize;
    shadowMapFilterTextureParameters.useEdgeClamping = true;
    shadowMapFilterTextureParameters.useFloatComponents = true;
    shadowMapFilterTextureParameters.bitsPerComponent = 32;
    shadowMapFilterTextureParameters.useLinearFiltering = true;
    shadowMapFilterTextureParameters.useMipmapFiltering = false;
    m_filterTextures[0].setParameters(shadowMapFilterTextureParameters);
    m_filterTextures[1].setParameters(shadowMapFilterTextureParameters);
    m_filterTextures[0].allocateData(nullptr);
    m_filterTextures[1].allocateData(nullptr);
    m_filterRenderLayer.setTextureAttachment(0, &m_filterTextures[0]);
    m_filterRenderLayer.setTextureAttachment(1, &m_filterTextures[1]);

    // Allocate per-cascade members
    m_cascadePasses.resize(m_numCascades);
    m_cascadeFrustumCullers.resize(m_numCascades);
    m_frustumCullerJobParams.resize(m_numCascades);
    m_cascadePassUpdateParams.resize(m_numCascades);
    m_cascadeMatrices.resize(m_numCascades);
    m_viewCascadeMatrices.resize(m_numCascades);
    m_cascadeSplitDepths.resize(m_numCascades);
    m_cascadeBlurRanges.resize(m_numCascades);

    // Initialize cascade sub-passes
    for (auto i = 0u; i < m_numCascades; ++i) {
        m_cascadePasses[i].setShaders(&m_depthOnlyShader, &m_depthOnlyShaderSkinned);
        m_cascadePasses[i].setRenderTarget(&m_varianceRenderLayer, &m_depthArrayTexture);
        m_cascadePasses[i].setTextureSize(m_textureSize);
        m_cascadePasses[i].setLayer((uint32_t) i);
        m_cascadePasses[i].init();
    }
}

void ShadowMapPass::setState() {
    VKR_DEBUG_CALL(
    glEnable(GL_BLEND);
    glDisable(GL_STENCIL_TEST);
    glDisable(GL_DEPTH_CLAMP);

    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);

    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);

    m_varianceRenderLayer.setEnabledDrawTargets({});
    )
}

void ShadowMapPass::render() {
    for (ShadowCascadePass& pass : m_cascadePasses) {
        VKR_DEBUG_CALL( pass.updateInstanceBuffers(); )
        VKR_DEBUG_CALL( pass.setState(); )
        VKR_DEBUG_CALL( pass.render(); )
    }

    glDisable(GL_BLEND);

    // Convert depth texture to variance shadow map

    VKR_DEBUG_CALL(
    glDepthMask(GL_FALSE);
    glDisable(GL_DEPTH_TEST);

    m_depthToVarianceShader.bind();
    m_depthToVarianceShader.setUniform("enableEVSM", 1);
    m_depthToVarianceShader.setUniform("depthTextureArray", 0);

    m_depthArrayTexture.bind(0);

    for (auto i = 0u; i < m_numCascades; ++i) {
        m_varianceRenderLayer.setEnabledDrawTargets({i});
        m_varianceRenderLayer.bind();

        glViewport(0, 0, m_textureSize, m_textureSize);
        glClear(GL_COLOR_BUFFER_BIT);

        m_depthToVarianceShader.setUniform("arrayLayer", i);

        FullscreenQuad::draw();
    }
    )

    // Blur the variance texture for smoother shadow edges

    VKR_DEBUG_CALL(
    m_filterShader.bind();
    m_filterShader.setUniform("tex_sampler", 0);

    for (auto i = 0u; i < m_numCascades; ++i) {
        if (m_filterTextureSize != m_textureSize) {
            // Downsample the cascade array layer into the filter texture object
            m_varianceRenderLayer.setEnabledReadTarget(i);
            m_filterRenderLayer.setEnabledDrawTargets({1});

            m_varianceRenderLayer.bind(GL_READ_FRAMEBUFFER);
            m_filterRenderLayer.bind(GL_DRAW_FRAMEBUFFER);

            glBlitFramebuffer(0, 0, m_textureSize, m_textureSize,
                              0, 0, m_filterTextureSize, m_filterTextureSize,
                              GL_COLOR_BUFFER_BIT, GL_LINEAR);
        } else {
            // Do a straight copy which is likely faster than a blit with interpolation
            glCopyImageSubData(m_evsmArrayTexture.getHandle(), GL_TEXTURE_2D_ARRAY, 0, 0, 0, i,
                               m_filterTextures[1].getHandle(), GL_TEXTURE_2D, 0, 0, 0, 0,
                               m_textureSize, m_textureSize, 1);
        }


        // First pass filters horizontally
        float offsetScale = glm::pow(1.0f - m_cascadeScale, i);  // we should shrink the blur radius for further away maps to keep visual blurring relatively constant
        m_filterShader.setUniform("coordOffset", glm::vec2(offsetScale/m_filterTextureSize, 0.0));

        m_filterRenderLayer.setEnabledDrawTargets({0});
        m_filterRenderLayer.bind();
        glViewport(0, 0, m_filterTextureSize, m_filterTextureSize);
        glClear(GL_COLOR_BUFFER_BIT);

        m_filterTextures[1].bind(0);

        FullscreenQuad::draw();

        // Second pass filters vertically
        m_filterShader.setUniform("coordOffset", glm::vec2(0.0, offsetScale/m_filterTextureSize));

        m_filterRenderLayer.setEnabledDrawTargets({1});
        m_filterRenderLayer.bind();
        glViewport(0, 0, m_filterTextureSize, m_filterTextureSize);
        glClear(GL_COLOR_BUFFER_BIT);

        m_filterTextures[0].bind(0);

        FullscreenQuad::draw();

        if (m_filterTextureSize != m_textureSize) {
            // upsample filtered texture to shadow render layer
            m_filterRenderLayer.setEnabledReadTarget(1);
            m_varianceRenderLayer.setEnabledDrawTargets({i});

            m_filterRenderLayer.bind(GL_READ_FRAMEBUFFER);
            m_varianceRenderLayer.bind(GL_DRAW_FRAMEBUFFER);

            glBlitFramebuffer(0, 0, m_filterTextureSize, m_filterTextureSize,
                              0, 0, m_textureSize, m_textureSize,
                              GL_COLOR_BUFFER_BIT, GL_LINEAR);
        } else {
            // Do a straight copy which is likely faster than a blit with interpolation
            glCopyImageSubData(m_filterTextures[1].getHandle(), GL_TEXTURE_2D, 0, 0, 0, 0,
                               m_evsmArrayTexture.getHandle(), GL_TEXTURE_2D_ARRAY, 0, 0, 0, i,
                               m_textureSize, m_textureSize, 1);
        }
    }
    )
}

void ShadowMapPass::cleanup() {
    for (ShadowCascadePass& pass : m_cascadePasses) {
        pass.cleanup();
    }

    m_cascadePasses.clear();
    m_cascadePassUpdateParams.clear();
    m_cascadeFrustumCullers.clear();
    m_cascadeBlurRanges.clear();
    m_cascadeSplitDepths.clear();
    m_cascadeMatrices.clear();
    m_viewCascadeMatrices.clear();

    m_numCascades = 0;

    m_pScheduler = nullptr;
}

void ShadowMapPass::setNumCascades(uint32_t numCascades) {
    m_numCascades = numCascades;
}

void ShadowMapPass::setTextureSize(uint32_t textureSize) {
    m_textureSize = textureSize;
}

void ShadowMapPass::setFilterTextureSize(uint32_t filterTextureSize) {
    m_filterTextureSize = filterTextureSize;
}

void ShadowMapPass::setMaxDistance(float maxDistance) {
    m_maxDistance = maxDistance;
}

void ShadowMapPass::setCascadeScale(float scale) {
    m_cascadeScale = scale;
}

void ShadowMapPass::setCascadeBlurSize(float blurSize) {
    m_cascadeBlurSize = blurSize;
}

void ShadowMapPass::setMatrices(const glm::mat4& cameraViewInverse, const glm::mat4& lightViewMatrix) {
    m_cameraViewInverse = cameraViewInverse;
    m_lightViewMatrix = lightViewMatrix;
}

void ShadowMapPass::preRenderJob(uintptr_t param) {
    PreRenderParam* pParam = reinterpret_cast<PreRenderParam*>(param);

    ShadowMapPass* pPass = pParam->pPass;

    //pPass->computeMatrices(pParam->pScene);

    glm::vec3 AABBMin(std::numeric_limits<float>::max()),
              AABBMax(std::numeric_limits<float>::min());

    pParam->pGameWorld->getRegistry().view<const Component::Renderable, const BoundingSphere>().each(
        [&] (const auto& r, const auto& b) {
            AABBMin = glm::min(AABBMin, b.position - b.radius);
            AABBMax = glm::max(AABBMax, b.position + b.radius);
        });

    pPass->computeMatrices(AABBMin, AABBMax, pParam->pCamera);

    std::vector<JobScheduler::JobDeclaration> cullDecls(pPass->m_numCascades);
    for (auto i = 0u; i < cullDecls.size(); ++i) {
        pPass->m_frustumCullerJobParams[i].frustumMatrix = pPass->m_cascadeMatrices[i];
        pPass->m_frustumCullerJobParams[i].pCuller       = &pPass->m_cascadeFrustumCullers[i];
        //pPass->m_frustumCullerJobParams[i].pScene        = pParam->pScene;
        pPass->m_frustumCullerJobParams[i].pGameWorld    = pParam->pGameWorld;

        cullDecls[i].numSignalCounters = 1;
        cullDecls[i].signalCounters[0] = pPass->m_cascadeFrustumCullers[i].getResultsReadyCounter(); // pPass->m_pScheduler->getCounterByID("SMFC"); //
        cullDecls[i].param = reinterpret_cast<uintptr_t>(&pPass->m_frustumCullerJobParams[i]);
        cullDecls[i].pFunction = FrustumCuller::cullEntitySpheresJob;

        //pPass->m_pScheduler->enqueueJob(cullDecls[i]);
    }
    pPass->m_pScheduler->enqueueJobs((uint32_t) cullDecls.size(), cullDecls.data());

    std::vector<JobScheduler::JobDeclaration> updateDecls(pPass->m_numCascades);
    for (auto i = 0u; i < updateDecls.size(); ++i) {
        pPass->m_cascadePassUpdateParams[i].globalMatrix  = pPass->m_cascadeMatrices[i];
        pPass->m_cascadePassUpdateParams[i].pCuller       = &pPass->m_cascadeFrustumCullers[i];
        pPass->m_cascadePassUpdateParams[i].pPass         = &pPass->m_cascadePasses[i];
        //pPass->m_cascadePassUpdateParams[i].pScene        = pParam->pScene;
        pPass->m_cascadePassUpdateParams[i].pGameWorld    = pParam->pGameWorld;
        pPass->m_cascadePassUpdateParams[i].signalCounter = pParam->signalCounter;

        updateDecls[i].numSignalCounters = 1;
        updateDecls[i].signalCounters[0] = pParam->signalCounter;
        updateDecls[i].param = reinterpret_cast<uintptr_t>(&pPass->m_cascadePassUpdateParams[i]);
        updateDecls[i].pFunction = GeometryRenderPass::updateInstanceListsJob;
        updateDecls[i].waitCounter = pPass->m_cascadeFrustumCullers[i].getResultsReadyCounter();  // pPass->m_pScheduler->getCounterByID("SMFC"); //


        //pPass->m_pScheduler->enqueueJob(updateDecls[i]);
    }
    pPass->m_pScheduler->enqueueJobs((uint32_t) updateDecls.size(), updateDecls.data());
}

void ShadowMapPass::computeMatrices(const glm::vec3& sceneAABBMin, const glm::vec3& sceneAABBMax, const Camera* pCamera) {
//void ShadowMapPass::computeMatrices(const Scene* pScene) {

    m_viewToLightMatrix = m_lightViewMatrix * m_cameraViewInverse;

    glm::vec3 wsSceneAABBPositions[] = {
        {sceneAABBMin.x, sceneAABBMin.y, sceneAABBMin.z},  // 0 0 0
        {sceneAABBMax.x, sceneAABBMin.y, sceneAABBMin.z},  // 1 0 0
        {sceneAABBMin.x, sceneAABBMax.y, sceneAABBMin.z},  // 0 1 0
        {sceneAABBMax.x, sceneAABBMax.y, sceneAABBMin.z},  // 1 1 0
        {sceneAABBMin.x, sceneAABBMin.y, sceneAABBMax.z},  // 0 0 1
        {sceneAABBMax.x, sceneAABBMin.y, sceneAABBMax.z},  // 1 0 1
        {sceneAABBMin.x, sceneAABBMax.y, sceneAABBMax.z},  // 0 1 1
        {sceneAABBMax.x, sceneAABBMax.y, sceneAABBMax.z}   // 1 1 1
    };
    glm::vec3 lsSceneAABBPositions[8];
    for (int i = 0; i < 8; ++i) {
        lsSceneAABBPositions[i] = glm::vec3(m_lightViewMatrix * glm::vec4(wsSceneAABBPositions[i], 1.0));
    }

    float cameraZRange = m_maxDistance - pCamera->getNearPlane(); //pCamera->getFarPlane() - pCamera->getNearPlane();
    float fovy = pCamera->getFOV();
    float aspect = pCamera->getAspectRatio();

    for (auto i = 0u; i < m_numCascades; ++i) {
        float intervalEnd  = pCamera->getNearPlane() + glm::pow(m_cascadeScale, m_numCascades-(i+1)) * cameraZRange;
        float intervalStart = (i == 0) ? pCamera->getNearPlane() : m_cascadeSplitDepths[i-1];

        m_cascadeBlurRanges[i] = m_cascadeBlurSize * (intervalEnd-intervalStart);
        m_cascadeSplitDepths[i] = intervalEnd;

        if (i > 0) intervalStart -= m_cascadeBlurRanges[i-1];  // make sure entire blur range of previous cascade is covered by this map

        glm::vec3 lightBoxExtentsMin, lightBoxExtentsMax;

        // compute bounding sphere of frustum
        // fitting the cascade to this sphere should reduce edge-swimming when camera orientation changes
        // This computation from Eric Zhang, https://lxjk.github.io/2017/04/15/Calculate-Minimal-Bounding-Sphere-of-Frustum.html, accessed 2020

        glm::vec3 sPos;
        float sRad;

        float k2 = std::tan(fovy/2.0f);
        k2 *= k2;
        k2 += 1 + aspect * aspect;
        if (k2 >= (intervalEnd - intervalStart) / (intervalEnd + intervalStart)) {
            sPos = glm::vec3(0, 0, -intervalEnd);
            sRad = intervalEnd * std::sqrt(k2);
        } else {
            float sumrange = intervalEnd + intervalStart;
            float difrange = intervalEnd - intervalStart;

            sPos = glm::vec3(0, 0, -0.5f * sumrange * (1 + k2));
            sRad = 0.5f * std::sqrt(difrange * difrange + 2 * k2 * (intervalEnd*intervalEnd + intervalStart*intervalStart) + sumrange * sumrange * k2 * k2);
        }
        // sPos and sRad define the bounding sphere relative to the camera. We need to put this into light-local space
        sPos = glm::vec3(m_lightViewMatrix * m_cameraViewInverse * glm::vec4(sPos, 1.0f));

        // set orthographic bounds to texel-size increments
        // prevents flickering shadow edges, from MSDN 'Common Techniques to Improve Shadow Depth Maps'
        // based on implementation by Bryan Law-Smith
        // http://longforgottenblog.blogspot.com/2014/12/rendering-post-stable-cascaded-shadow.html
        float worldUnitsPerTexel = sRad * 2.0f / (float) m_filterTextureSize;

        lightBoxExtentsMin = worldUnitsPerTexel * glm::floor((sPos - glm::vec3(sRad)) / worldUnitsPerTexel);
        lightBoxExtentsMax = lightBoxExtentsMin + glm::floor(2.0f * sRad / worldUnitsPerTexel) * worldUnitsPerTexel;

        // the near plane is fixed to the scene AABB so that potential occluders outside the cascade don't get clipped when rendering
        float nearPlane = -lightBoxExtentsMax.z;
        math_util::getClippedNearPlane(lightBoxExtentsMin.x, lightBoxExtentsMax.x, lightBoxExtentsMin.y, lightBoxExtentsMax.y, lsSceneAABBPositions, &nearPlane);

        m_cascadeMatrices[i] = glm::ortho(lightBoxExtentsMin.x, lightBoxExtentsMax.x, lightBoxExtentsMin.y, lightBoxExtentsMax.y, nearPlane, -lightBoxExtentsMin.z);
        m_viewCascadeMatrices[i] = m_cascadeMatrices[i] * m_viewToLightMatrix;
        m_cascadeMatrices[i] *= m_lightViewMatrix;
    }
}
