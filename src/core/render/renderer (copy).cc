#include "renderer.h"

#include <algorithm>
#include <numeric>

#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/random.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <glm/gtc/noise.hpp>

#include "core/util/math_util.h"
#include "core/util/mesh_builder.h"

#define VKRENDER_DEBUG
#ifdef VKRENDER_DEBUG
#include <iostream>
#include <sstream>

#define VKR_DEBUG_PRINT(X) { std::stringstream sstr; sstr << X << std::endl; std::cout << sstr.str(); std::flush(std::cout); }
#define VKR_DEBUG_CHECK_ERROR(X) X; checkError(#X);

//#define VKRENDER_DEBUG_PRINT_RUNTIME_ENABLED
#ifdef VKRENDER_DEBUG_PRINT_RUNTIME_ENABLED
#include <GLFW/glfw3.h>
#define VKR_DEBUG_PRINT_RUNTIME(X) { uint64_t ticks = glfwGetTimerValue(); X; VKR_DEBUG_PRINT("Call " << #X << " runtime: " << (1000. * (glfwGetTimerValue()-ticks)/glfwGetTimerFrequency()) << " ms") }
#else
#define VKR_DEBUG_PRINT_RUNTIME(X) X;
#endif // VKRENDER_DEBUG_PRINT_RUNTIME_ENABLED

#define VKR_DEBUG_CALL(X) VKR_DEBUG_PRINT_RUNTIME(X); checkError(#X);

void checkError(const std::string& callString) {
    GLenum err = glGetError();
    if (err) {
        std::string str = "GL Error after call " + callString + ": ";
        switch(err) {
        case GL_INVALID_ENUM:
            str += "GL_INVALID_ENUM";
            break;
        case GL_INVALID_VALUE:
            str += "GL_INVALID_VALUE";
            break;

        case GL_INVALID_OPERATION:
            str += "GL_INVALID_OPERATION";
            break;

        case GL_INVALID_FRAMEBUFFER_OPERATION:
            str += "GL_INVALID_FRAMEBUFFER_OPERATION";
            break;

        case GL_OUT_OF_MEMORY:
            str += "GL_OUT_OF_MEMORY";
            break;

        case GL_STACK_UNDERFLOW:
            str += "GL_STACK_UNDERFLOW";
            break;

        case GL_STACK_OVERFLOW:
            str += "GL_STACK_OVERFLOW";
            break;
        default :
            str += "Unknown error " + std::to_string(err);
            break;
        }
        VKR_DEBUG_PRINT(str)
    }
}
#else

#define VKR_DEBUG_PRINT(X)
#define VKR_DEBUG_CHECK_ERROR(X) X;
#define VKR_DEBUG_CALL(X) X;

#endif // VKRENDER_DEBUG


Renderer::Renderer(JobScheduler* pScheduler) : m_pScheduler(pScheduler) {
}

Renderer::~Renderer() {
}

void Renderer::init(const RendererParameters& parameters) {
    m_parameters = parameters;

    m_fullScreenFilterShader.linkShaderFiles("shaders/vertex_fs.glsl", "shaders/fragment_filter.glsl");
    m_horizontalFilterShader.linkShaderFiles("shaders/vertex_fs.glsl", "shaders/fragment_filter_horizontal.glsl");
    m_fullScreenShader.linkShaderFiles("shaders/vertex_fs.glsl", "shaders/fragment_fstex.glsl");
    m_passthroughShader.linkVertexShader("shaders/vertex_pt.glsl");

    initDeferredPass();
    initBackground();
    initFullScreenQuad();
    initGBuffer();
    initShadowMap();
    initPointLightShadowMaps();
    initSSAO();
    initBloom();
    initMotionVectors();
    initTAA();
    initMotionBlur();
    initTransparency();


    setViewport(m_parameters.initialWidth, m_parameters.initialHeight);

    // TODO: move this block
    {
        MeshData sphereMeshData(MeshBuilder().sphere(1.0f, 50, 25).moveMeshData());

        m_pointLightSphere.setVertexCount(sphereMeshData.vertices.size());
        m_pointLightSphere.createVertexAttribute(0, 3);
        m_pointLightSphere.allocateVertexBuffer();
        m_pointLightSphere.setVertexAttributeData(0, sphereMeshData.vertices.data());
        m_pointLightSphere.createIndexBuffer(sphereMeshData.indices.size(), sphereMeshData.indices.data());
    }

    // OpenGL context settings
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_MULTISAMPLE);
    glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);

    m_listBuildersCounter = m_pScheduler->getCounterByID("list_build"); //m_pScheduler->getFreeCounter();
    m_frustumCullCounter = m_pScheduler->getCounterByID("frustum_cull"); //m_pScheduler->getFreeCounter();

    m_isInitialized = true;
}

void Renderer::cleanup() {
    // Bloom textures
    for (size_t i = 0; i < m_bloomTextures.size(); ++i) {
        if (m_bloomTextures[i]) delete m_bloomTextures[i];
    }
    m_bloomTextures.clear();

    // Point light shadow maps
    for (size_t i = 0; i < m_pointLightShadowMaps.size(); ++i) {
        if (m_pointLightShadowMaps[i]) delete m_pointLightShadowMaps[i];
        if (m_pointLightShadowDepthMaps[i]) delete m_pointLightShadowDepthMaps[i];
    }
    m_pointLightShadowMaps.clear();
    m_pointLightShadowDepthMaps.clear();

    // Call bucket instance buffers
    if (m_defaultCallBucket.buffersInitialized) {
        glDeleteBuffers(1, &m_defaultCallBucket.instanceTransformBuffer);
        m_defaultCallBucket.buffersInitialized = false;
    }
    if (m_skinnedCallBucket.buffersInitialized) {
        glDeleteBuffers(1, &m_skinnedCallBucket.instanceTransformBuffer);
        m_skinnedCallBucket.buffersInitialized = false;
    }
    if (m_motionVectorsCallBucket.buffersInitialized) {
        glDeleteBuffers(1, &m_motionVectorsCallBucket.instanceTransformBuffer);
        m_motionVectorsCallBucket.buffersInitialized = false;
    }
    if (m_motionVectorsCallBucketSkinned.buffersInitialized) {
        glDeleteBuffers(1, &m_motionVectorsCallBucketSkinned.instanceTransformBuffer);
        m_motionVectorsCallBucketSkinned.buffersInitialized = false;
    }
    if (m_transparencyCallBucket.buffersInitialized) {
        glDeleteBuffers(1, &m_transparencyCallBucket.instanceTransformBuffer);
        m_transparencyCallBucket.buffersInitialized = false;
    }
    if (m_transparencyCallBucketSkinned.buffersInitialized) {
        glDeleteBuffers(1, &m_transparencyCallBucketSkinned.instanceTransformBuffer);
        m_transparencyCallBucketSkinned.buffersInitialized = false;
    }
    for (size_t i = 0; i < m_shadowMapCallBuckets.size(); ++i) {
        if (m_shadowMapCallBuckets[i].buffersInitialized) {
            glDeleteBuffers(1, &m_shadowMapCallBuckets[i].instanceTransformBuffer);
            m_shadowMapCallBuckets[i].buffersInitialized = false;
        }
        if (m_skinnedShadowMapCallBuckets[i].buffersInitialized) {
            glDeleteBuffers(1, &m_skinnedShadowMapCallBuckets[i].instanceTransformBuffer);
            m_skinnedShadowMapCallBuckets[i].buffersInitialized = false;
        }
    }
    m_shadowMapCallBuckets.clear();
    m_skinnedShadowMapCallBuckets.clear();

    for (size_t i = 0; i < m_pointLightShadowCallBuckets.size(); ++i) {
        if (m_pointLightShadowCallBuckets[i].buffersInitialized) {
            glDeleteBuffers(1, &m_pointLightShadowCallBuckets[i].instanceTransformBuffer);
            m_pointLightShadowCallBuckets[i].buffersInitialized = false;
        }
        if (m_pointLightShadowCallBucketsSkinned[i].buffersInitialized) {
            glDeleteBuffers(1, &m_pointLightShadowCallBucketsSkinned[i].instanceTransformBuffer);
            m_pointLightShadowCallBucketsSkinned[i].buffersInitialized = false;
        }
    }
    m_pointLightShadowCallBucketsSkinned.clear();
    m_pointLightShadowCallBuckets.clear();

    m_isInitialized = false;
}


void Renderer::setViewport(uint32_t width, uint32_t height) {
    m_viewportWidth = width;
    m_viewportHeight = height;

    // Resize scene render texture
    TextureParameters sceneTextureParameters = m_sceneTexture.getParameters();
    sceneTextureParameters.width = m_viewportWidth;
    sceneTextureParameters.height = m_viewportHeight;

    m_sceneTexture.setParameters(sceneTextureParameters);
    m_sceneTexture.allocateData(nullptr);

    RenderBufferParameters sceneRBParameters = m_sceneDepthStencilRenderBuffer.getParameters();
    sceneRBParameters.width = m_viewportWidth;
    sceneRBParameters.height = m_viewportHeight;

    m_sceneDepthStencilRenderBuffer.setParameters(sceneRBParameters);
    m_sceneDepthStencilRenderBuffer.allocateData();

    m_sceneRenderLayer.setRenderBufferAttachment(&m_sceneDepthStencilRenderBuffer);
    m_sceneRenderLayer.setTextureAttachment(0, &m_sceneTexture);

    // Resize HDR bloom textures
    if (m_parameters.numBloomLevels > 0) {
        TextureParameters bloomTextureParameters = m_bloomTextures[0]->getParameters();
        bloomTextureParameters.width = m_viewportWidth;
        bloomTextureParameters.height = m_viewportHeight;
        for(uint32_t i = 0; i < m_parameters.numBloomLevels; ++i) {
            m_bloomTextures[2*i  ]->setParameters(bloomTextureParameters);
            m_bloomTextures[2*i+1]->setParameters(bloomTextureParameters);
            m_bloomTextures[2*i  ]->allocateData(nullptr);
            m_bloomTextures[2*i+1]->allocateData(nullptr);
            if(bloomTextureParameters.width  > 1) bloomTextureParameters.width  /= 2;
            if(bloomTextureParameters.height > 1) bloomTextureParameters.height /= 2;
        }
    }

    // Resize GBuffer textures
/*    TextureParameters gBufferTextureParameters = m_gBufferPositionViewSpaceTexture.getParameters();
    gBufferTextureParameters.width = m_viewportWidth;
    gBufferTextureParameters.height = m_viewportHeight;
    m_gBufferPositionViewSpaceTexture.setParameters(gBufferTextureParameters);
    m_gBufferPositionViewSpaceTexture.allocateData(nullptr);*/

    TextureParameters gBufferTextureParameters = m_gBufferNormalViewSpaceTexture.getParameters();
    gBufferTextureParameters.width = m_viewportWidth;
    gBufferTextureParameters.height = m_viewportHeight;
    m_gBufferNormalViewSpaceTexture.setParameters(gBufferTextureParameters);
    m_gBufferNormalViewSpaceTexture.allocateData(nullptr);

    gBufferTextureParameters = m_gBufferAlbedoMetallicTexture.getParameters();
    gBufferTextureParameters.width = m_viewportWidth;
    gBufferTextureParameters.height = m_viewportHeight;
    m_gBufferAlbedoMetallicTexture.setParameters(gBufferTextureParameters);
    m_gBufferAlbedoMetallicTexture.allocateData(nullptr);

    gBufferTextureParameters = m_gBufferEmissionRoughnessTexture.getParameters();
    gBufferTextureParameters.width = m_viewportWidth;
    gBufferTextureParameters.height = m_viewportHeight;
    m_gBufferEmissionRoughnessTexture.setParameters(gBufferTextureParameters);
    m_gBufferEmissionRoughnessTexture.allocateData(nullptr);

    gBufferTextureParameters = m_gBufferDepthTexture.getParameters();
    gBufferTextureParameters.width = m_viewportWidth;
    gBufferTextureParameters.height = m_viewportHeight;
    m_gBufferDepthTexture.setParameters(gBufferTextureParameters);
    m_gBufferDepthTexture.allocateData(nullptr);

    /*RenderBufferParameters gBufferDepthRBParameters = m_gBufferDepthRenderBuffer.getParameters();
    gBufferDepthRBParameters.width = m_viewportWidth;
    gBufferDepthRBParameters.height = m_viewportHeight;
    m_gBufferDepthRenderBuffer.setParameters(gBufferDepthRBParameters);
    m_gBufferDepthRenderBuffer.allocateData();*/

    //m_gBufferRenderLayer.setRenderBufferAttachment(&m_gBufferDepthRenderBuffer);
    //m_gBufferRenderLayer.setTextureAttachment(4, &m_gBufferDepthTexture);
    m_gBufferRenderLayer.setDepthTexture(&m_gBufferDepthTexture);
    //m_gBufferRenderLayer.setTextureAttachment(0, &m_gBufferPositionViewSpaceTexture);
    m_gBufferRenderLayer.setTextureAttachment(0, &m_gBufferNormalViewSpaceTexture);
    m_gBufferRenderLayer.setTextureAttachment(1, &m_gBufferAlbedoMetallicTexture);
    m_gBufferRenderLayer.setTextureAttachment(2, &m_gBufferEmissionRoughnessTexture);

    m_gBufferRenderLayer.validate();

    // resize SSAO texture
    TextureParameters ssaoTextureParameters = m_ssaoTargetTexture.getParameters();
    ssaoTextureParameters.width = m_viewportWidth;
    ssaoTextureParameters.height = m_viewportHeight;
    m_ssaoTargetTexture.setParameters(ssaoTextureParameters);
    m_ssaoTargetTexture.allocateData(nullptr);
    m_ssaoRenderLayer.setTextureAttachment(0, &m_ssaoTargetTexture);

    m_ssaoFilterTexture.setParameters(ssaoTextureParameters);
    m_ssaoFilterTexture.allocateData(nullptr);
    m_ssaoFilterRenderLayer.setTextureAttachment(0, &m_ssaoFilterTexture);
    m_ssaoFilterRenderLayer.setTextureAttachment(1, &m_ssaoTargetTexture);

    // resize motion vectors layer
    TextureParameters motionTexParams = m_motionBuffer.getParameters();
    motionTexParams.width = m_viewportWidth;
    motionTexParams.height = m_viewportHeight;
    m_motionBuffer.setParameters(motionTexParams);
    m_motionBuffer.allocateData(nullptr);

    RenderBufferParameters motionRBParams = m_motionVectorsDepthBuffer.getParameters();
    motionRBParams.width = m_viewportWidth;
    motionRBParams.height = m_viewportHeight;
    m_motionVectorsDepthBuffer.setParameters(motionRBParams);
    m_motionVectorsDepthBuffer.allocateData();

    m_motionRenderLayer.setTextureAttachment(0, &m_motionBuffer);
    m_motionRenderLayer.setRenderBufferAttachment(&m_motionVectorsDepthBuffer);

    // resize motion blur texture
    TextureParameters motionBlurTexParam = m_motionBlurTexture.getParameters();
    motionBlurTexParam.width = m_viewportWidth;
    motionBlurTexParam.height = m_viewportHeight;
    m_motionBlurTexture.setParameters(motionBlurTexParam);
    m_motionBlurTexture.allocateData(nullptr);
    m_motionBlurRenderLayer.setTextureAttachment(0, &m_motionBlurTexture);

    // resize history buffer textures
    TextureParameters historyTexParams = m_historyBuffer[0].getParameters();
    historyTexParams.width = m_viewportWidth;
    historyTexParams.height = m_viewportHeight;
    m_historyBuffer[0].setParameters(historyTexParams);
    m_historyBuffer[0].allocateData(nullptr);
    m_historyBuffer[1].setParameters(historyTexParams);
    m_historyBuffer[1].allocateData(nullptr);
    m_historyRenderLayer.setTextureAttachment(0, &m_historyBuffer[0]);
    m_historyRenderLayer.setTextureAttachment(1, &m_historyBuffer[1]);


    // resize transparency textures
    TextureParameters accumTexParam = m_transparencyAccumTexture.getParameters();
    TextureParameters revealageTexParam = m_transparencyRevealageTexture.getParameters();
    accumTexParam.width  = revealageTexParam.width  = m_viewportWidth;
    accumTexParam.height = revealageTexParam.height = m_viewportHeight;
    m_transparencyAccumTexture.setParameters(accumTexParam);
    m_transparencyRevealageTexture.setParameters(revealageTexParam);
    m_transparencyAccumTexture.allocateData(nullptr);
    m_transparencyRevealageTexture.allocateData(nullptr);
    m_transparencyRenderLayer.setTextureAttachment(0, &m_transparencyAccumTexture);
    m_transparencyRenderLayer.setTextureAttachment(1, &m_transparencyRevealageTexture);
    m_transparencyRenderLayer.setRenderBufferAttachment(&m_sceneDepthStencilRenderBuffer);

    // resize cloud render target
    m_cloudsTextureWidth  = (m_viewportWidth %4u==0u) ? m_viewportWidth /4u : m_viewportWidth /4u + 1u;
    m_cloudsTextureHeight = (m_viewportHeight%4u==0u) ? m_viewportHeight/4u : m_viewportHeight/4u + 1u;
    TextureParameters cloudsTexParam = m_cloudsRenderTexture.getParameters();
    cloudsTexParam.width = m_cloudsTextureWidth;
    cloudsTexParam.height = m_cloudsTextureHeight;
    m_cloudsRenderTexture.setParameters(cloudsTexParam);
    m_cloudsRenderTexture.allocateData(nullptr);
    m_cloudsRenderLayer.setTextureAttachment(0, &m_cloudsRenderTexture);

    cloudsTexParam = m_cloudsHistoryTexture[0].getParameters();
    cloudsTexParam.width = m_viewportWidth;
    cloudsTexParam.height = m_viewportHeight;
    m_cloudsHistoryTexture[0].setParameters(cloudsTexParam);
    m_cloudsHistoryTexture[1].setParameters(cloudsTexParam);
    m_cloudsHistoryTexture[0].allocateData(nullptr);
    m_cloudsHistoryTexture[1].allocateData(nullptr);
    m_cloudsAccumRenderLayer.setTextureAttachment(0, &m_cloudsHistoryTexture[0]);
    m_cloudsAccumRenderLayer.setTextureAttachment(1, &m_cloudsHistoryTexture[1]);


    m_isViewportInitialized = true;
    m_historyValid = false;
    m_cloudHistoryValid = false;
}

void Renderer::setDirectionalLighting(const glm::vec3& direction, const glm::vec3& intensity) {
    m_lightDirection = glm::normalize(direction);
    m_lightIntensity = intensity;
    m_directionalLightEnabled = (glm::max(intensity.x, glm::max(intensity.y, intensity.z)) > 0.0);
}

void Renderer::addPointLight(const glm::vec3& position, const glm::vec3& intensity, bool shadowMap) {
    m_pointLightPositions.push_back(position);
    m_pointLightIntensities.push_back(intensity);
    m_pointLightBoundingSphereRadii.push_back(glm::sqrt(glm::max(glm::max(intensity.r, intensity.g), intensity.b) / m_parameters.pointLightMinIntensity - 1.0f) + 0.1f);

    if(shadowMap && m_parameters.maxPointLightShadowMaps != 0) {
        // compute priority for receiving a shadow map using a key of the light's bounding radius squared
        // over the view-space depth of the light
        // this approximates the visual impact of the light on the final image, give shadow maps to more important lights
        glm::vec4 viewPos = m_cameraViewMatrix * glm::vec4(position, 1.0);
        float key = m_pointLightBoundingSphereRadii.back();
        key *= key / glm::length(viewPos);

        if(m_parameters.maxPointLightShadowMaps < 0 || m_inUsePointLightShadowMaps < m_parameters.maxPointLightShadowMaps) {  // we don't need to steal one
//            if(m_inUsePointLightShadowMaps == m_numPointLightShadowMaps) {  // we do need to make a new one
//                addPointLightShadowMap();
//            }
            m_pointLightShadowMapLightIndices[m_inUsePointLightShadowMaps] = static_cast<uint32_t>(m_pointLightShadowMapIndices.size());
            m_pointLightShadowMapLightKeys[m_inUsePointLightShadowMaps] = key;
            m_pointLightShadowMapIndices.push_back(static_cast<int>(m_inUsePointLightShadowMaps));
            ++m_inUsePointLightShadowMaps;
        } else {  // we do need to steal one (if possible)
            float minKey = key;
            int minInd = -1;
            for(uint32_t i = 0; i < m_parameters.maxPointLightShadowMaps; ++i) {
                if(m_pointLightShadowMapLightKeys[i] < minKey) {
                    minKey = m_pointLightShadowMapLightKeys[i];
                    minInd = static_cast<int>(i);
                }
            }
            if(minInd > -1) {
                m_pointLightShadowMapIndices[m_pointLightShadowMapLightIndices[minInd]] = -1;
                m_pointLightShadowMapLightIndices[minInd] = static_cast<uint32_t>(m_pointLightShadowMapIndices.size());
                m_pointLightShadowMapLightKeys[minInd] = key;
                m_pointLightShadowMapIndices.push_back(minInd);
            } else {
                m_pointLightShadowMapIndices.push_back(-1);
            }
        }
    } else {
        m_pointLightShadowMapIndices.push_back(-1);
    }
}

void Renderer::clearPointLights() {
    m_pointLightPositions.clear();
    m_pointLightIntensities.clear();
    m_pointLightBoundingSphereRadii.clear();
    m_pointLightShadowMapIndices.clear();
    m_pointLightShadowMapLightIndices.assign(m_pointLightShadowMapLightIndices.size(), 0);
    m_pointLightShadowMapLightKeys.assign(m_pointLightShadowMapLightKeys.size(), 0.0f);
    m_inUsePointLightShadowMaps = 0;
}

/*
int Renderer::computeSphereCulling(const glm::vec3& position, float radius, const std::vector<Renderable>& renderablesIn, std::vector<uint32_t>& toRenderIndices) {
    int numPassed = 0;
    for(size_t i = 0; i < renderablesIn.size(); ++i) {
        glm::mat4 wtf = renderablesIn[i].getWorldTransform();
        glm::vec3 spos = glm::vec3(wtf[3]);
        float srad = renderablesIn[i].getModel()->getBoundingSphereRadius() * std::sqrt(std::max({glm::length2(glm::vec3(wtf[0])), glm::length2(glm::vec3(wtf[1])), glm::length2(glm::vec3(wtf[2]))}));
        spos -= position;
        srad += radius;
        if(glm::dot(spos, spos) <= srad*srad) {
            toRenderIndices[numPassed++] = i;
        }
    }

    return numPassed;
}

int Renderer::computeSphereCulling(const glm::vec3& position, float radius, const std::vector<Renderable>& renderablesIn, std::vector<bool>& cullResults) {
    int numPassed = 0;
    for(size_t i = 0; i < renderablesIn.size(); ++i) {
        glm::mat4 wtf = renderablesIn[i].getWorldTransform();
        glm::vec3 spos = glm::vec3(wtf[3]);
        float srad = renderablesIn[i].getModel()->getBoundingSphereRadius() * std::sqrt(std::max({glm::length2(glm::vec3(wtf[0])), glm::length2(glm::vec3(wtf[1])), glm::length2(glm::vec3(wtf[2]))}));
        spos -= position;
        srad += radius;
        if(glm::dot(spos, spos) <= srad*srad) {
            cullResults[i] = true;
            numPassed++;
        } else {
            cullResults[i] = false;
        }
    }

    return numPassed;
}*/


void Renderer::fillCallBucketJob(uintptr_t param) {
    FillCallBucketParam* pParam = reinterpret_cast<FillCallBucketParam*>(param);

    CallBucket& bucket = *pParam->bucket;
    const std::vector<InstanceList>& instanceLists = *pParam->instanceLists;
    size_t numInstances = pParam->numInstances;
    glm::mat4 globalMatrix = pParam->globalMatrix;
    bool useNormalsMatrix = pParam->useNormalsMatrix;
    bool hasSkinningMatrices = pParam->hasSkinningMatrices;

    if (bucket.callInfos.size() < instanceLists.size()) {
        bucket.callInfos.resize(instanceLists.size());
        bucket.usageFlags.resize(instanceLists.size());
    }
    bucket.usageFlags.assign(bucket.usageFlags.size(), false);

    size_t transformSize = (useNormalsMatrix ? 32 : 16) + (pParam->useLastFrameMatrix ? 16 : 0);
    size_t numInstanceTransforms;

    if (!hasSkinningMatrices) {
        numInstanceTransforms = numInstances;
    } else {
        numInstanceTransforms = std::accumulate(instanceLists.begin(), instanceLists.end(), size_t(0),
            [] (size_t acc, const InstanceList& il) {
                return acc + il.getModel()->getSkeletonDescription()->getNumJoints() * il.getNumInstances(); });
    }

    bucket.instanceTransformFloats.resize(numInstanceTransforms * transformSize);
    bucket.numInstances = numInstances;

    size_t transformBufferOffset = 0;
    size_t floatBufferOffset = 0;
    for (size_t i = 0; i < instanceLists.size(); ++i) {
        const Model* pModel = instanceLists[i].getModel();

        CallHeader header;
        header.pMesh = pModel->getMesh();
        header.pMaterial = pModel->getMaterial();
        header.pSkeletonDesc = pModel->getSkeletonDescription();

        CallInfo callInfo;
        callInfo.header = header;
        callInfo.transformBufferOffset = transformBufferOffset;
        callInfo.numInstances = instanceLists[i].getNumInstances();

        bucket.callInfos[i] = callInfo;
        bucket.usageFlags[i] = true;

        for (size_t j = 0; j < instanceLists[i].getNumInstances(); ++j) {
            glm::mat4 worldMatrix = instanceLists[i].getInstanceTransforms()[j];
            glm::mat4 worldGlobalMatrix = globalMatrix * worldMatrix;
            glm::mat4 worldGlobalNormalsMatrix;
            glm::mat4 lastWorldGlobalMatrix;
            if (pParam->useNormalsMatrix) worldGlobalNormalsMatrix = pParam->normalsMatrix * glm::inverseTranspose(worldMatrix);
            if (pParam->useLastFrameMatrix) lastWorldGlobalMatrix = pParam->lastGlobalMatrix * instanceLists[i].getLastInstanceTransforms()[j];

            if (!hasSkinningMatrices) {
                memcpy(&bucket.instanceTransformFloats[floatBufferOffset], &worldGlobalMatrix[0][0], 16*sizeof(float));
                floatBufferOffset += 16;
                if (useNormalsMatrix) {
                    memcpy(&bucket.instanceTransformFloats[floatBufferOffset], &worldGlobalNormalsMatrix[0][0], 16*sizeof(float));
                    floatBufferOffset += 16;
                }
                if (pParam->useLastFrameMatrix) {
                    memcpy(&bucket.instanceTransformFloats[floatBufferOffset], &lastWorldGlobalMatrix[0][0], 16*sizeof(float));
                    floatBufferOffset += 16;
                }
            } else {
                const Skeleton* pSkeleton = instanceLists[i].getInstanceSkeletons()[j];
                //assert(pSkeleton);

                size_t numJoints = header.pSkeletonDesc->getNumJoints();
                assert(pSkeleton->getSkinningMatrices().size() == numJoints);
                std::vector<glm::mat4> skinningMatrices((useNormalsMatrix ? 2 * numJoints : numJoints) + (pParam->useLastFrameMatrix ? numJoints : 0));
                if (!useNormalsMatrix && !pParam->useLastFrameMatrix) {
                    std::transform(pSkeleton->getSkinningMatrices().begin(), pSkeleton->getSkinningMatrices().end(),
                               skinningMatrices.begin(),
                               [&] (const glm::mat4& m) { return worldGlobalMatrix * m; });
                } else {
                    if (pParam->useLastFrameMatrix) {
                        if (useNormalsMatrix) {
                            for (size_t k = 0; k < numJoints; ++k) {
                                glm::mat4 skinningMatrix = pSkeleton->getSkinningMatrices()[k];
                                skinningMatrices[3*k] = worldGlobalMatrix * skinningMatrix;
                                skinningMatrices[3*k+1] = worldGlobalNormalsMatrix * glm::inverseTranspose(skinningMatrix);
                                skinningMatrices[3*k+2] = lastWorldGlobalMatrix * pSkeleton->getLastSkinningMatrices()[k];
                            }
                        } else {
                            for (size_t k = 0; k < numJoints; ++k) {
                                skinningMatrices[2*k] = worldGlobalMatrix * pSkeleton->getSkinningMatrices()[k];
                                skinningMatrices[2*k+1] = lastWorldGlobalMatrix * pSkeleton->getLastSkinningMatrices()[k];
                            }
                        }
                    } else {
                        for (size_t k = 0; k < numJoints; ++k) {
                            glm::mat4 skinningMatrix = pSkeleton->getSkinningMatrices()[k];
                            skinningMatrices[2*k] = worldGlobalMatrix * skinningMatrix;
                            skinningMatrices[2*k+1] = worldGlobalNormalsMatrix * glm::inverseTranspose(skinningMatrix);
                        }
                    }
                }

                size_t numFloats = 16 * skinningMatrices.size();
                memcpy(&bucket.instanceTransformFloats[floatBufferOffset], skinningMatrices.data(), numFloats * sizeof(float));
                floatBufferOffset += numFloats;
            }
        }
        if (!hasSkinningMatrices) {
            transformBufferOffset += instanceLists[i].getNumInstances();
        } else {
            transformBufferOffset += instanceLists[i].getNumInstances() * header.pSkeletonDesc->getNumJoints();
        }
    }
}

void Renderer::fillCallBucketInstanceBuffer(CallBucket& bucket) {
    if(!bucket.buffersInitialized) {
        glGenBuffers(1, &(bucket.instanceTransformBuffer));
        bucket.buffersInitialized = true;
    }
    if (bucket.numInstances == 0) return;

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, bucket.instanceTransformBuffer);
    glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(float)*bucket.instanceTransformFloats.size(), bucket.instanceTransformFloats.data(), GL_STREAM_DRAW);
}

void Renderer::updateInstanceBuffers() {
    if (m_defaultCallBucket.numInstances > 0) {
        fillCallBucketInstanceBuffer(m_defaultCallBucket);
    }

    if (m_skinnedCallBucket.numInstances > 0) {
        fillCallBucketInstanceBuffer(m_skinnedCallBucket);
    }

    if (m_motionVectorsCallBucket.numInstances > 0) {
        fillCallBucketInstanceBuffer(m_motionVectorsCallBucket);
    }

    if (m_motionVectorsCallBucketSkinned.numInstances > 0) {
        fillCallBucketInstanceBuffer(m_motionVectorsCallBucketSkinned);
    }

    if (m_transparencyCallBucket.numInstances > 0) {
        fillCallBucketInstanceBuffer(m_transparencyCallBucket);
    }

    if (m_transparencyCallBucketSkinned.numInstances > 0) {
        fillCallBucketInstanceBuffer(m_transparencyCallBucketSkinned);
    }

    for (size_t i = 0; i < 6 * m_inUsePointLightShadowMaps; ++i) {
        if (m_pointLightShadowCallBuckets[i].numInstances > 0) {
            fillCallBucketInstanceBuffer(m_pointLightShadowCallBuckets[i]);
        }

        if (m_pointLightShadowCallBucketsSkinned[i].numInstances > 0) {
            fillCallBucketInstanceBuffer(m_pointLightShadowCallBucketsSkinned[i]);
        }
    }

    for (size_t i = 0; i < m_parameters.shadowMapNumCascades; ++i) {
        if (m_shadowMapCallBuckets[i].numInstances > 0) {
            fillCallBucketInstanceBuffer(m_shadowMapCallBuckets[i]);
        }

        if (m_skinnedShadowMapCallBuckets[i].numInstances > 0) {
            fillCallBucketInstanceBuffer(m_skinnedShadowMapCallBuckets[i]);
        }
    }
}

void Renderer::frustumCullJob(uintptr_t param) {
    FrustumCullParam* pParam = reinterpret_cast<FrustumCullParam*>(param);

    pParam->pCuller->cullSceneRenderables(pParam->pScene, pParam->frustumMatrix);
}


void Renderer::buildInstanceListsJob(uintptr_t param) {
    BuildInstanceListsParam* pParam = reinterpret_cast<BuildInstanceListsParam*>(param);

    if (pParam->pCuller->getNumToRender() > 0) {
        pParam->pListBuilder->buildInstanceLists(pParam->pScene, pParam->pCuller->getCullResults(), pParam->frustumMatrix, pParam->predicate);
    } else {
        pParam->pListBuilder->clearInstanceLists();
    }
}

void Renderer::render() {
    VKR_DEBUG_CALL(renderGBuffer())

    if (m_parameters.enableShadows && m_directionalShadowsEnabled) {
        VKR_DEBUG_CALL(renderShadowMap())

        if(m_parameters.enableShadowFiltering) {
            VKR_DEBUG_CALL(filterShadowMap())
        }
    }

    if (m_parameters.enableShadows && m_parameters.enablePointLights && m_inUsePointLightShadowMaps > 0) {
        VKR_DEBUG_CALL(renderPointLightShadows())
    }

    if (m_parameters.enableSSAO) {
        VKR_DEBUG_CALL(renderSSAOMap())
    }

    if (m_parameters.enableBackgroundLayer) {
        // these are needed regardless of taa/motion blur since the background layer is always temporally upsampled
        renderBackgroundMotionVectors();
    }

    VKR_DEBUG_CALL(renderDeferredPass())

    if (m_parameters.enableBackgroundLayer) {
        VKR_DEBUG_CALL(renderBackground());
    }

    if (m_parameters.enableTAA || m_parameters.enableMotionBlur) {
        VKR_DEBUG_CALL(renderMotionVectors());
    }

    VKR_DEBUG_CALL(renderTransparency());

    if (m_parameters.enableMotionBlur) {
        VKR_DEBUG_CALL(renderMotionBlur());
    }

    if (m_parameters.enableTAA) {
        VKR_DEBUG_CALL(renderTAA())
    }

    if (m_parameters.enableBloom && m_parameters.numBloomLevels > 0) {
        VKR_DEBUG_CALL(renderBloom())
    }

    VKR_DEBUG_CALL(renderCompositePass())

    ++m_frameCount;
}

bool motionVecsPredicate(const Model* pModel) { return pModel->getMaterial() && pModel->getMaterial()->useObjectMotionVectors(); }
//bool opaquePredicate(const Model* pModel) { return !pModel->getMaterial()->isTransparencyEnabled(); }
bool transparencyPredicate(const Model* pModel) { return pModel->getMaterial() && pModel->getMaterial()->isTransparencyEnabled(); }
bool shadowPredicate(const Model* pModel) { return !pModel->getMaterial() || pModel->getMaterial()->isShadowCastingEnabled(); }

void Renderer::dispatchFrustumCullerJobs(uintptr_t param) {
    RendererJobParam* pParam = reinterpret_cast<RendererJobParam*>(param);

    Renderer* pRenderer = pParam->pRenderer;

    std::vector<JobScheduler::JobDeclaration> decls(1 + pRenderer->m_parameters.shadowMapNumCascades + 6*pRenderer->m_parameters.maxPointLightShadowMaps);
    pRenderer->m_frustumCullParams.resize(decls.size());
    uint32_t numDecls = 0;

    // Default Camera frustum (shared by geometry and motion buffers passes)
    {
        pRenderer->m_frustumCullParams[numDecls].pCuller = &pRenderer->m_defaultFrustumCuller;
        pRenderer->m_frustumCullParams[numDecls].frustumMatrix = pRenderer->m_viewProj;
        pRenderer->m_frustumCullParams[numDecls].pScene = pParam->pScene;

        decls[numDecls].numSignalCounters = 1;
        decls[numDecls].signalCounters[0] = pRenderer->m_frustumCullCounter;
        decls[numDecls].param = reinterpret_cast<uintptr_t>(&pRenderer->m_frustumCullParams[numDecls]);
        decls[numDecls].pFunction = frustumCullJob;
        ++numDecls;
    }

    if (pRenderer->m_inUsePointLightShadowMaps > 0) {
        for (uint32_t i = 0; i < 6 * pRenderer->m_inUsePointLightShadowMaps; ++i) {
            pRenderer->m_frustumCullParams[numDecls].frustumMatrix = pRenderer->m_pointLightShadowMapMatrices[i];
            pRenderer->m_frustumCullParams[numDecls].pCuller = &pRenderer->m_pointShadowFrustumCullers[i];
            pRenderer->m_frustumCullParams[numDecls].pScene = pParam->pScene;

            decls[numDecls].numSignalCounters = 1;
            decls[numDecls].signalCounters[0] = pRenderer->m_frustumCullCounter;
            decls[numDecls].param = reinterpret_cast<uintptr_t>(&pRenderer->m_frustumCullParams[numDecls]);
            decls[numDecls].pFunction = frustumCullJob;
            ++numDecls;
        }
    }

    if (pRenderer->m_directionalShadowsEnabled) {
        for(uint32_t i = 0; i < pRenderer->m_parameters.shadowMapNumCascades; ++i) {
            pRenderer->m_frustumCullParams[numDecls].frustumMatrix = pRenderer->m_shadowMapCascadeMatrices[i];
            pRenderer->m_frustumCullParams[numDecls].pCuller = &pRenderer->m_shadowMapFrustumCullers[i];
            pRenderer->m_frustumCullParams[numDecls].pScene = pParam->pScene;

            decls[numDecls].numSignalCounters = 1;
            decls[numDecls].signalCounters[0] = pRenderer->m_frustumCullCounter;
            decls[numDecls].param = reinterpret_cast<uintptr_t>(&pRenderer->m_frustumCullParams[numDecls]);
            decls[numDecls].pFunction = frustumCullJob;
            ++numDecls;
        }
    }

    pParam->pScheduler->enqueueJobs(numDecls, decls.data());

}

void Renderer::dispatchListBuilderJobs(uintptr_t param) {
    RendererJobParam* pParam = reinterpret_cast<RendererJobParam*>(param);

    Renderer* pRenderer = pParam->pRenderer;

    // 1 default + 1 per directional shadow cascade + 1 per cube face per point light (6 per PL) + 1 motion vectors + 1 transparency
    std::vector<JobScheduler::JobDeclaration> decls(1 + pRenderer->m_parameters.shadowMapNumCascades + 6*pRenderer->m_parameters.maxPointLightShadowMaps + 2);
    uint32_t numDecls = 0;

    {
        pRenderer->m_buildDefaultListsParam.filterNonShadowCasters = false;
        pRenderer->m_buildDefaultListsParam.frustumMatrix = pRenderer->m_viewProj;
        pRenderer->m_buildDefaultListsParam.pCuller = &pRenderer->m_defaultFrustumCuller;
        pRenderer->m_buildDefaultListsParam.pListBuilder = &pRenderer->m_defaultListBuilder;
        pRenderer->m_buildDefaultListsParam.pScene = pParam->pScene;
        pRenderer->m_buildDefaultListsParam.predicate = nullptr;

        decls[numDecls].numSignalCounters = 1;
        decls[numDecls].signalCounters[0] = pRenderer->m_listBuildersCounter;
        decls[numDecls].param = reinterpret_cast<uintptr_t>(&pRenderer->m_buildDefaultListsParam);
        decls[numDecls].pFunction = buildInstanceListsJob;
        ++numDecls;
    }

    {
        pRenderer->m_buildMotionVectorsListsParam.filterNonShadowCasters = false;
        pRenderer->m_buildMotionVectorsListsParam.predicate = motionVecsPredicate;
        pRenderer->m_buildMotionVectorsListsParam.frustumMatrix = pRenderer->m_cameraProjectionMatrix * pRenderer->m_cameraViewMatrix;
        pRenderer->m_buildMotionVectorsListsParam.pCuller = &pRenderer->m_defaultFrustumCuller;
        pRenderer->m_buildMotionVectorsListsParam.pListBuilder = &pRenderer->m_motionVectorsListBuilder;
        pRenderer->m_buildMotionVectorsListsParam.pScene = pParam->pScene;

        decls[numDecls].numSignalCounters = 1;
        decls[numDecls].signalCounters[0] = pRenderer->m_listBuildersCounter;
        decls[numDecls].param = reinterpret_cast<uintptr_t>(&pRenderer->m_buildMotionVectorsListsParam);
        decls[numDecls].pFunction = buildInstanceListsJob;
        ++numDecls;
    }

    {
        pRenderer->m_buildTransparencyListsParam.filterNonShadowCasters = false;
        pRenderer->m_buildTransparencyListsParam.predicate = transparencyPredicate;
        pRenderer->m_buildTransparencyListsParam.frustumMatrix = pRenderer->m_cameraProjectionMatrix * pRenderer->m_cameraViewMatrix;
        pRenderer->m_buildTransparencyListsParam.pCuller = &pRenderer->m_defaultFrustumCuller;
        pRenderer->m_buildTransparencyListsParam.pListBuilder = &pRenderer->m_transparencyListBuilder;
        pRenderer->m_buildTransparencyListsParam.pScene = pParam->pScene;

        decls[numDecls].numSignalCounters = 1;
        decls[numDecls].signalCounters[0] = pRenderer->m_listBuildersCounter;
        decls[numDecls].param = reinterpret_cast<uintptr_t>(&pRenderer->m_buildTransparencyListsParam);
        decls[numDecls].pFunction = buildInstanceListsJob;
        ++numDecls;
    }

    if (pRenderer->m_inUsePointLightShadowMaps > 0) {
        for (uint32_t i = 0; i < 6 * pRenderer->m_inUsePointLightShadowMaps; ++i) {
            pRenderer->m_buildPointShadowMapListsParams[i].filterNonShadowCasters = true;
            pRenderer->m_buildPointShadowMapListsParams[i].frustumMatrix = pRenderer->m_pointLightShadowMapMatrices[i];
            pRenderer->m_buildPointShadowMapListsParams[i].pCuller = &pRenderer->m_pointShadowFrustumCullers[i];
            pRenderer->m_buildPointShadowMapListsParams[i].pListBuilder = &pRenderer->m_pointShadowListBuilders[i];
            pRenderer->m_buildPointShadowMapListsParams[i].pScene = pParam->pScene;
            pRenderer->m_buildPointShadowMapListsParams[i].predicate = shadowPredicate;

            decls[numDecls].numSignalCounters = 1;
            decls[numDecls].signalCounters[0] = pRenderer->m_listBuildersCounter;
            decls[numDecls].param = reinterpret_cast<uintptr_t>(&pRenderer->m_buildPointShadowMapListsParams[i]);
            decls[numDecls].pFunction = buildInstanceListsJob;
            ++numDecls;
        }
    }

    if(pRenderer->m_directionalShadowsEnabled) {
        for(uint32_t i = 0; i < pRenderer->m_parameters.shadowMapNumCascades; ++i) {
            pRenderer->m_buildShadowMapListsParams[i].filterNonShadowCasters = true;
            pRenderer->m_buildShadowMapListsParams[i].frustumMatrix = pRenderer->m_shadowMapCascadeMatrices[i];
            pRenderer->m_buildShadowMapListsParams[i].pCuller = &pRenderer->m_shadowMapFrustumCullers[i];
            pRenderer->m_buildShadowMapListsParams[i].pListBuilder = &pRenderer->m_shadowMapListBuilders[i];
            pRenderer->m_buildShadowMapListsParams[i].pScene = pParam->pScene;
            pRenderer->m_buildShadowMapListsParams[i].predicate = shadowPredicate;

            decls[numDecls].numSignalCounters = 1;
            decls[numDecls].signalCounters[0] = pRenderer->m_listBuildersCounter;
            decls[numDecls].param = reinterpret_cast<uintptr_t>(&pRenderer->m_buildShadowMapListsParams[i]);
            decls[numDecls].pFunction = buildInstanceListsJob;
            ++numDecls;
        }
    }

    pParam->pScheduler->enqueueJobs(numDecls, decls.data());
}

void Renderer::dispatchCallBucketJobs(uintptr_t param) {
    RendererJobParam* pParam = reinterpret_cast<RendererJobParam*>(param);

    Renderer* pRenderer = pParam->pRenderer;

    std::vector<JobScheduler::JobDeclaration> decls(2*(1 + pRenderer->m_parameters.shadowMapNumCascades + 6*pRenderer->m_parameters.maxPointLightShadowMaps + 2));
    uint32_t numDecls = 0;

    if (pRenderer->m_defaultListBuilder.hasNonSkinnedInstances()) {
        // Default (Non-skinned geometry) Call bucket param
        pRenderer->m_fillDefaultCallBucketParam.bucket = &pRenderer->m_defaultCallBucket;
        pRenderer->m_fillDefaultCallBucketParam.globalMatrix = pRenderer->m_viewProj;
        pRenderer->m_fillDefaultCallBucketParam.normalsMatrix = pRenderer->m_viewNormals;
        pRenderer->m_fillDefaultCallBucketParam.instanceLists = &pRenderer->m_defaultListBuilder.getNonSkinnedInstanceLists();
        pRenderer->m_fillDefaultCallBucketParam.numInstances = pRenderer->m_defaultListBuilder.getNumNonSkinnedInstances();
        pRenderer->m_fillDefaultCallBucketParam.useNormalsMatrix = true;
        pRenderer->m_fillDefaultCallBucketParam.hasSkinningMatrices = false;
        pRenderer->m_fillDefaultCallBucketParam.useLastFrameMatrix = false;

        // Default call bucket job declaration
        decls[numDecls].numSignalCounters = 1;
        decls[numDecls].signalCounters[0] = pParam->signalCounterHandle;
        decls[numDecls].param = reinterpret_cast<uintptr_t>(&pRenderer->m_fillDefaultCallBucketParam);
        decls[numDecls].pFunction = fillCallBucketJob;

        ++numDecls;
    } else {
        pRenderer->m_defaultCallBucket.numInstances = 0;
    }

    if (pRenderer->m_defaultListBuilder.hasSkinnedInstances()) {
        // Skinned geometry call bucket param
        pRenderer->m_fillSkinnedCallBucketParam.bucket = &pRenderer->m_skinnedCallBucket;
        pRenderer->m_fillSkinnedCallBucketParam.globalMatrix = pRenderer->m_viewProj;
        pRenderer->m_fillSkinnedCallBucketParam.normalsMatrix = pRenderer->m_viewNormals;
        pRenderer->m_fillSkinnedCallBucketParam.instanceLists = &pRenderer->m_defaultListBuilder.getSkinnedInstanceLists();
        pRenderer->m_fillSkinnedCallBucketParam.numInstances = pRenderer->m_defaultListBuilder.getNumSkinnedInstances();
        pRenderer->m_fillSkinnedCallBucketParam.useNormalsMatrix = true;
        pRenderer->m_fillSkinnedCallBucketParam.hasSkinningMatrices = true;
        pRenderer->m_fillSkinnedCallBucketParam.useLastFrameMatrix = false;

        // Skinned call bucket job declaration
        decls[numDecls].numSignalCounters = 1;
        decls[numDecls].signalCounters[0] = pParam->signalCounterHandle;
        decls[numDecls].param = reinterpret_cast<uintptr_t>(&pRenderer->m_fillSkinnedCallBucketParam);
        decls[numDecls].pFunction = fillCallBucketJob;

        ++numDecls;
    } else {
        pRenderer->m_skinnedCallBucket.numInstances = 0;
    }

    if (pRenderer->m_motionVectorsListBuilder.hasSkinnedInstances()) {
        // Skinned geometry motion vectors call bucket param
        pRenderer->m_fillSkinnedMotionVectorsCallBucketParam.bucket = &pRenderer->m_motionVectorsCallBucketSkinned;
        pRenderer->m_fillSkinnedMotionVectorsCallBucketParam.globalMatrix = pRenderer->m_viewProj;
        pRenderer->m_fillSkinnedMotionVectorsCallBucketParam.lastGlobalMatrix = pRenderer->m_lastViewProj;
        pRenderer->m_fillSkinnedMotionVectorsCallBucketParam.instanceLists = &pRenderer->m_motionVectorsListBuilder.getSkinnedInstanceLists();
        pRenderer->m_fillSkinnedMotionVectorsCallBucketParam.numInstances = pRenderer->m_motionVectorsListBuilder.getNumSkinnedInstances();
        pRenderer->m_fillSkinnedMotionVectorsCallBucketParam.useNormalsMatrix = false;
        pRenderer->m_fillSkinnedMotionVectorsCallBucketParam.hasSkinningMatrices = true;
        pRenderer->m_fillSkinnedMotionVectorsCallBucketParam.useLastFrameMatrix = true;

        // Skinned call bucket job declaration
        decls[numDecls].numSignalCounters = 1;
        decls[numDecls].signalCounters[0] = pParam->signalCounterHandle;
        decls[numDecls].param = reinterpret_cast<uintptr_t>(&pRenderer->m_fillSkinnedMotionVectorsCallBucketParam);
        decls[numDecls].pFunction = fillCallBucketJob;

        ++numDecls;
    } else {
        pRenderer->m_motionVectorsCallBucketSkinned.numInstances = 0;
    }

    if (pRenderer->m_motionVectorsListBuilder.hasNonSkinnedInstances()) {
        // Default (Non-skinned geometry) motion vectors Call bucket param
        pRenderer->m_fillMotionVectorsCallBucketParam.bucket = &pRenderer->m_motionVectorsCallBucket;
        pRenderer->m_fillMotionVectorsCallBucketParam.globalMatrix = pRenderer->m_viewProj;
        pRenderer->m_fillMotionVectorsCallBucketParam.lastGlobalMatrix = pRenderer->m_lastViewProj;
        pRenderer->m_fillMotionVectorsCallBucketParam.instanceLists = &pRenderer->m_motionVectorsListBuilder.getNonSkinnedInstanceLists();
        pRenderer->m_fillMotionVectorsCallBucketParam.numInstances = pRenderer->m_motionVectorsListBuilder.getNumNonSkinnedInstances();
        pRenderer->m_fillMotionVectorsCallBucketParam.useNormalsMatrix = false;
        pRenderer->m_fillMotionVectorsCallBucketParam.hasSkinningMatrices = false;
        pRenderer->m_fillMotionVectorsCallBucketParam.useLastFrameMatrix = true;

        // Default call bucket job declaration
        decls[numDecls].numSignalCounters = 1;
        decls[numDecls].signalCounters[0] = pParam->signalCounterHandle;
        decls[numDecls].param = reinterpret_cast<uintptr_t>(&pRenderer->m_fillMotionVectorsCallBucketParam);
        decls[numDecls].pFunction = fillCallBucketJob;

        ++numDecls;
    } else {
        pRenderer->m_motionVectorsCallBucket.numInstances = 0;
    }

    if (pRenderer->m_transparencyListBuilder.hasSkinnedInstances()) {
        // Skinned geometry transparency call bucket param
        pRenderer->m_fillSkinnedTransparencyCallBucketParam.bucket = &pRenderer->m_transparencyCallBucketSkinned;
        pRenderer->m_fillSkinnedTransparencyCallBucketParam.globalMatrix = pRenderer->m_viewProj;
        pRenderer->m_fillSkinnedTransparencyCallBucketParam.normalsMatrix = pRenderer->m_viewNormals;
        pRenderer->m_fillSkinnedTransparencyCallBucketParam.instanceLists = &pRenderer->m_transparencyListBuilder.getSkinnedInstanceLists();
        pRenderer->m_fillSkinnedTransparencyCallBucketParam.numInstances = pRenderer->m_transparencyListBuilder.getNumSkinnedInstances();
        pRenderer->m_fillSkinnedTransparencyCallBucketParam.useNormalsMatrix = true;
        pRenderer->m_fillSkinnedTransparencyCallBucketParam.hasSkinningMatrices = true;
        pRenderer->m_fillSkinnedTransparencyCallBucketParam.useLastFrameMatrix = false;

        // Skinned call bucket job declaration
        decls[numDecls].numSignalCounters = 1;
        decls[numDecls].signalCounters[0] = pParam->signalCounterHandle;
        decls[numDecls].param = reinterpret_cast<uintptr_t>(&pRenderer->m_fillSkinnedTransparencyCallBucketParam);
        decls[numDecls].pFunction = fillCallBucketJob;

        ++numDecls;
    } else {
        pRenderer->m_transparencyCallBucketSkinned.numInstances = 0;
    }

    if (pRenderer->m_transparencyListBuilder.hasNonSkinnedInstances()) {
        // Default (Non-skinned geometry) transparency Call bucket param
        pRenderer->m_fillTransparencyCallBucketParam.bucket = &pRenderer->m_transparencyCallBucket;
        pRenderer->m_fillTransparencyCallBucketParam.globalMatrix = pRenderer->m_viewProj;
        pRenderer->m_fillTransparencyCallBucketParam.normalsMatrix = pRenderer->m_viewNormals;
        pRenderer->m_fillTransparencyCallBucketParam.instanceLists = &pRenderer->m_transparencyListBuilder.getNonSkinnedInstanceLists();
        pRenderer->m_fillTransparencyCallBucketParam.numInstances = pRenderer->m_transparencyListBuilder.getNumNonSkinnedInstances();
        pRenderer->m_fillTransparencyCallBucketParam.useNormalsMatrix = true;
        pRenderer->m_fillTransparencyCallBucketParam.hasSkinningMatrices = false;
        pRenderer->m_fillTransparencyCallBucketParam.useLastFrameMatrix = false;

        // Default call bucket job declaration
        decls[numDecls].numSignalCounters = 1;
        decls[numDecls].signalCounters[0] = pParam->signalCounterHandle;
        decls[numDecls].param = reinterpret_cast<uintptr_t>(&pRenderer->m_fillTransparencyCallBucketParam);
        decls[numDecls].pFunction = fillCallBucketJob;

        ++numDecls;
    } else {
        pRenderer->m_transparencyCallBucket.numInstances = 0;
    }

    // Point Lights
    // 12 Buckets per shadow-enabled light (one skinned and one non-skinned per cube face)
    for(uint32_t i = 0; i < 6 * pRenderer->m_inUsePointLightShadowMaps; ++i) {
        if (pRenderer->m_pointShadowListBuilders[i].hasNonSkinnedInstances()) {
            pRenderer->m_fillPointShadowCallBucketParams[i].bucket = &pRenderer->m_pointLightShadowCallBuckets[i];
            pRenderer->m_fillPointShadowCallBucketParams[i].globalMatrix = pRenderer->m_pointLightShadowMapMatrices[i];
            pRenderer->m_fillPointShadowCallBucketParams[i].instanceLists = &pRenderer->m_pointShadowListBuilders[i].getNonSkinnedInstanceLists();
            pRenderer->m_fillPointShadowCallBucketParams[i].numInstances = pRenderer->m_pointShadowListBuilders[i].getNumNonSkinnedInstances();
            pRenderer->m_fillPointShadowCallBucketParams[i].useNormalsMatrix = false;
            pRenderer->m_fillPointShadowCallBucketParams[i].hasSkinningMatrices = false;
            pRenderer->m_fillPointShadowCallBucketParams[i].useLastFrameMatrix = false;

            decls[numDecls].numSignalCounters = 1;
            decls[numDecls].signalCounters[0] = pParam->signalCounterHandle;
            decls[numDecls].param = reinterpret_cast<uintptr_t>(&pRenderer->m_fillPointShadowCallBucketParams[i]);
            decls[numDecls].pFunction = fillCallBucketJob;

            ++numDecls;
        } else {
            pRenderer->m_pointLightShadowCallBuckets[i].numInstances = 0;
        }

        if (pRenderer->m_pointShadowListBuilders[i].hasSkinnedInstances()) {
            pRenderer->m_fillSkinnedPointShadowCallBucketParams[i].bucket = &pRenderer->m_pointLightShadowCallBucketsSkinned[i];
            pRenderer->m_fillSkinnedPointShadowCallBucketParams[i].globalMatrix = pRenderer->m_pointLightShadowMapMatrices[i];
            pRenderer->m_fillSkinnedPointShadowCallBucketParams[i].instanceLists = &pRenderer->m_pointShadowListBuilders[i].getSkinnedInstanceLists();
            pRenderer->m_fillSkinnedPointShadowCallBucketParams[i].numInstances = pRenderer->m_pointShadowListBuilders[i].getNumSkinnedInstances();
            pRenderer->m_fillSkinnedPointShadowCallBucketParams[i].useNormalsMatrix = false;
            pRenderer->m_fillSkinnedPointShadowCallBucketParams[i].hasSkinningMatrices = true;
            pRenderer->m_fillSkinnedPointShadowCallBucketParams[i].useLastFrameMatrix = false;

            decls[numDecls].numSignalCounters = 1;
            decls[numDecls].signalCounters[0] = pParam->signalCounterHandle;
            decls[numDecls].param = reinterpret_cast<uintptr_t>(&pRenderer->m_fillSkinnedPointShadowCallBucketParams[i]);
            decls[numDecls].pFunction = fillCallBucketJob;

            ++numDecls;
        } else {
            pRenderer->m_pointLightShadowCallBucketsSkinned[i].numInstances = 0;
        }
    }

    // Directional light (cascaded) shadow maps
    if(pRenderer->m_directionalShadowsEnabled) {
        // Create job declarations and parameter structs for each shadow cascade level
        for(uint32_t i = 0; i < pRenderer->m_parameters.shadowMapNumCascades; ++i) {
            if (pRenderer->m_shadowMapListBuilders[i].hasNonSkinnedInstances()) {
                pRenderer->m_fillShadowMapCallBucketParams[i].bucket = &pRenderer->m_shadowMapCallBuckets[i];
                pRenderer->m_fillShadowMapCallBucketParams[i].globalMatrix = pRenderer->m_shadowMapCascadeMatrices[i];;
                pRenderer->m_fillShadowMapCallBucketParams[i].instanceLists = &pRenderer->m_shadowMapListBuilders[i].getNonSkinnedInstanceLists();
                pRenderer->m_fillShadowMapCallBucketParams[i].numInstances = pRenderer->m_shadowMapListBuilders[i].getNumNonSkinnedInstances();
                pRenderer->m_fillShadowMapCallBucketParams[i].useNormalsMatrix = false;
                pRenderer->m_fillShadowMapCallBucketParams[i].hasSkinningMatrices = false;
                pRenderer->m_fillShadowMapCallBucketParams[i].useLastFrameMatrix = false;

                decls[numDecls].numSignalCounters = 1;
                decls[numDecls].signalCounters[0] = pParam->signalCounterHandle;
                decls[numDecls].param = reinterpret_cast<uintptr_t>(&pRenderer->m_fillShadowMapCallBucketParams[i]);
                decls[numDecls].pFunction = fillCallBucketJob;

                ++numDecls;
            } else {
                pRenderer->m_shadowMapCallBuckets[i].numInstances = 0;
            }

            if (pRenderer->m_shadowMapListBuilders[i].hasSkinnedInstances()) {
                pRenderer->m_fillSkinnedShadowMapCallBucketParams[i].bucket = &pRenderer->m_skinnedShadowMapCallBuckets[i];
                pRenderer->m_fillSkinnedShadowMapCallBucketParams[i].globalMatrix = pRenderer->m_shadowMapCascadeMatrices[i];;
                pRenderer->m_fillSkinnedShadowMapCallBucketParams[i].instanceLists = &pRenderer->m_shadowMapListBuilders[i].getSkinnedInstanceLists();
                pRenderer->m_fillSkinnedShadowMapCallBucketParams[i].numInstances = pRenderer->m_shadowMapListBuilders[i].getNumSkinnedInstances();
                pRenderer->m_fillSkinnedShadowMapCallBucketParams[i].useNormalsMatrix = false;
                pRenderer->m_fillSkinnedShadowMapCallBucketParams[i].hasSkinningMatrices = true;
                pRenderer->m_fillSkinnedShadowMapCallBucketParams[i].useLastFrameMatrix = false;

                decls[numDecls].numSignalCounters = 1;
                decls[numDecls].signalCounters[0] = pParam->signalCounterHandle;
                decls[numDecls].param = reinterpret_cast<uintptr_t>(&pRenderer->m_fillSkinnedShadowMapCallBucketParams[i]);
                decls[numDecls].pFunction = fillCallBucketJob;

                ++numDecls;
            } else {
                pRenderer->m_skinnedShadowMapCallBuckets[i].numInstances = 0;
            }
        }
    }

    // Enqueue call bucket jobs as a batch
    pParam->pScheduler->enqueueJobs(numDecls, decls.data());
}

void Renderer::preRenderJob(uintptr_t param) {
    RendererJobParam* pParam = reinterpret_cast<RendererJobParam*>(param);

    Renderer* pRenderer = pParam->pRenderer;
    const Scene* pScene = pParam->pScene;
    JobScheduler* pScheduler = pParam->pScheduler;

    assert(pRenderer->m_isInitialized);
    assert(pScene->getActiveCamera());

    // Do some setup first before enqueueing the various sub-dispatch jobs

    pRenderer->setDirectionalLighting(pScene->m_directionalLight.getDirection(), pScene->m_directionalLight.getIntensity());
    if (pRenderer->m_parameters.enableShadows && pScene->m_directionalLight.isShadowMapEnabled()) {
        pRenderer->m_directionalShadowsEnabled = pRenderer->m_directionalLightEnabled;
    } else {
        pRenderer->m_directionalShadowsEnabled = false;
    }
    pRenderer->m_ambientLightIntensity = pScene->getAmbientLightIntensity();

    pRenderer->computeMatrices(pScene->getActiveCamera());


    // Prepare point lights
    if (pRenderer->m_parameters.enablePointLights) {
        pRenderer->clearPointLights();

        // Determine which point lights we need to draw based on their bounding spheres
        if (pRenderer->m_pointLightBoundingSpheres.size() < pScene->m_pointLights.size()) {
            pRenderer->m_pointLightBoundingSpheres.resize(pScene->m_pointLights.size());
        }

        for (size_t i = 0; i < pScene->m_pointLights.size(); ++i) {
            const PointLight& pointLight = pScene->m_pointLights[i];
            pRenderer->m_pointLightBoundingSpheres[i] = {pointLight.getPosition(), pointLight.getBoundingSphereRadius()};
        }

        pRenderer->m_pointLightFrustumCuller.cullSpheres(
            pRenderer->m_pointLightBoundingSpheres.data(),
            pScene->m_pointLights.size(),
            pRenderer->m_cameraProjectionMatrix * pRenderer->m_cameraViewMatrix);

        for (size_t i = 0; i < pScene->m_pointLights.size(); ++i) {
            const PointLight& pointLight = pScene->m_pointLights[i];
            if (pRenderer->m_pointLightFrustumCuller.getCullResults()[i]) {
                pRenderer->addPointLight(
                    pointLight.getPosition(),
                    pointLight.getIntensity(),
                    (pRenderer->m_parameters.enableShadows && pRenderer->m_parameters.maxPointLightShadowMaps > 0) ?
                        pointLight.isShadowMapEnabled() :
                        false);
            }
        }

        if (pRenderer->m_inUsePointLightShadowMaps > 0) {
            pRenderer->computePointLightShadowMatrices();
        }
    }

    // Directional light (cascaded) shadow maps
    if (pRenderer->m_directionalShadowsEnabled) {
        // Compute shadow map bounding volume and split light frustum into cascades
        glm::vec3 sceneAABBMin, sceneAABBMax;
        pScene->computeAABB(sceneAABBMin, sceneAABBMax);
        pRenderer->computeShadowMapMatrices(sceneAABBMin, sceneAABBMax, pScene->getActiveCamera());
    }

    // Dispatch culling building jobs
    JobScheduler::JobDeclaration dispatchCullDecl;
    dispatchCullDecl.param = param;
    dispatchCullDecl.pFunction = dispatchFrustumCullerJobs;
    dispatchCullDecl.numSignalCounters = 1;
    dispatchCullDecl.signalCounters[0] = pRenderer->m_frustumCullCounter;
    pScheduler->enqueueJob(dispatchCullDecl);

    // Dispatch List building jobs
    JobScheduler::JobDeclaration dispatchListBuildersDecl;
    dispatchListBuildersDecl.param = param;
    dispatchListBuildersDecl.pFunction = dispatchListBuilderJobs;
    dispatchListBuildersDecl.waitCounter = pRenderer->m_frustumCullCounter;
    dispatchListBuildersDecl.numSignalCounters = 1;
    dispatchListBuildersDecl.signalCounters[0] = pRenderer->m_listBuildersCounter;
    pScheduler->enqueueJob(dispatchListBuildersDecl);

    // Dispatch Fill-CallBucket jobs
    JobScheduler::JobDeclaration dispatchCallBucketsDecl;
    dispatchCallBucketsDecl.param = param;
    dispatchCallBucketsDecl.pFunction = dispatchCallBucketJobs;
    dispatchCallBucketsDecl.waitCounter = pRenderer->m_listBuildersCounter;  // wait until list builder jobs return
    dispatchCallBucketsDecl.numSignalCounters = 1;
    // Once this job (and its children) are completed it will be okay to call the render job
    dispatchCallBucketsDecl.signalCounters[0] = pParam->signalCounterHandle;
    pScheduler->enqueueJob(dispatchCallBucketsDecl);
}

void Renderer::renderJob(uintptr_t param) {
    RendererJobParam* pParam = reinterpret_cast<RendererJobParam*>(param);

    pParam->pWindow->acquireContext();

    // This call requires the GL context, so it might as well go here
    // In the future I may investigate using mapped buffers to make this step able to be done concurrently
    pParam->pRenderer->updateInstanceBuffers();

    pParam->pRenderer->render();

    if (pParam->pAfterSceneRenderCallback) {
        pParam->pAfterSceneRenderCallback();
    }

    //pParam->pWindow->swapBuffers();

    pParam->pWindow->releaseContext();
}


void Renderer::computeMatrices(const Camera* pCamera) {
    m_tanCameraFovY = glm::tan(pCamera->getFOV()/2.0);
    m_tanCameraFovX = m_tanCameraFovY * pCamera->getAspectRatio();
    m_viewJitter = m_parameters.enableTAA && jitterOn? m_jitterSamples[m_frameCount%m_nJitterSamples] / glm::vec2(m_viewportWidth, m_viewportHeight) : glm::vec2(0.0);
    //m_viewJitter = glm::vec2(0, 5) / glm::vec2(m_viewportWidth, m_viewportHeight);
    m_cameraPosition = pCamera->getPosition();
    m_lastView = m_cameraViewMatrix;
    m_lastViewProj = m_viewProj;
    m_cameraViewMatrix = pCamera->calculateViewMatrix();
    m_projectionNoJitter = pCamera->calculateProjectionMatrix();
    m_cameraProjectionMatrix = glm::translate(glm::mat4(1.0), glm::vec3(m_viewJitter, 0.0)) * m_projectionNoJitter;
    m_lightDirectionViewSpace = glm::normalize(glm::vec3(m_cameraViewMatrix * glm::vec4(m_lightDirection, 0.0)));
    m_viewInverse = glm::inverse(m_cameraViewMatrix);
    m_viewNormals = glm::transpose(m_viewInverse);
    m_projectionInverse = glm::inverse(m_cameraProjectionMatrix);
    m_projectionNoJitterInverse = glm::inverse(m_projectionNoJitter);
    // Calculate light view matrices for shadow mapping passes
    m_lightViewMatrix = glm::lookAt(glm::vec3(0), glm::vec3(0)+m_lightDirection, glm::abs(m_lightDirection.y) < 1.0? glm::vec3(0, 1, 0) : glm::vec3(0, 0, 1));
    m_viewToLightMatrix = m_lightViewMatrix * m_viewInverse;
    //m_reprojectionMatrix = m_lastViewProj * m_viewInverse;
    m_viewProj = m_cameraProjectionMatrix * m_cameraViewMatrix;
    m_inverseViewProj = glm::inverse(m_viewProj);
}

void Renderer::computeShadowMapMatrices(const glm::vec3& sceneAABBMin, const glm::vec3& sceneAABBMax, const Camera* pCamera) {

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
    for(int i = 0; i < 8; ++i) {
        lsSceneAABBPositions[i] = glm::vec3(m_lightViewMatrix * glm::vec4(wsSceneAABBPositions[i], 1.0));
    }

    float cameraZRange = m_parameters.shadowMapMaxDistance - pCamera->getNearPlane(); //pCamera->getFarPlane() - pCamera->getNearPlane();
    float fovy = pCamera->getFOV();
    float aspect = pCamera->getAspectRatio();

    glm::mat4 cameraViewInverse = glm::inverse(m_cameraViewMatrix);

    for(uint32_t i = 0; i < m_parameters.shadowMapNumCascades; i++) {

        float intervalEnd  = pCamera->getNearPlane() + glm::pow(m_parameters.shadowMapCascadeScale, m_parameters.shadowMapNumCascades-(i+1)) * cameraZRange ;
        float intervalStart = (i == 0) ? pCamera->getNearPlane() : m_shadowMapCascadeSplitDepths[i-1];

        m_shadowMapCascadeBlurRanges[i] = m_parameters.shadowMapCascadeBlurSize * (intervalEnd-intervalStart);
        m_shadowMapCascadeSplitDepths[i] = intervalEnd;

        if(i > 0) intervalStart -= m_shadowMapCascadeBlurRanges[i-1];  // make sure entire blur range of previous cascade is covered by this map

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
        sPos = glm::vec3(m_lightViewMatrix * cameraViewInverse * glm::vec4(sPos, 1.0f));

        // set orthographic bounds to texel-size increments
        // prevents flickering shadow edges, from MSDN 'Common Techniques to Improve Shadow Depth Maps'
        // based on implementation by Bryan Law-Smith
        // http://longforgottenblog.blogspot.com/2014/12/rendering-post-stable-cascaded-shadow.html
        float worldUnitsPerTexel = sRad * 2.0f / (float) m_parameters.shadowMapFilterResolution;

        lightBoxExtentsMin = worldUnitsPerTexel * glm::floor((sPos - glm::vec3(sRad)) / worldUnitsPerTexel);
        lightBoxExtentsMax = lightBoxExtentsMin + glm::floor(2.0f * sRad / worldUnitsPerTexel) * worldUnitsPerTexel;

        // the near plane is fixed to the scene AABB so that potential occluders outside the cascade don't get clipped when rendering
        float nearPlane = -lightBoxExtentsMax.z;
        math_util::getClippedNearPlane(lightBoxExtentsMin.x, lightBoxExtentsMax.x, lightBoxExtentsMin.y, lightBoxExtentsMax.y, lsSceneAABBPositions, &nearPlane);

        m_shadowMapCascadeMatrices[i] = glm::ortho(lightBoxExtentsMin.x, lightBoxExtentsMax.x, lightBoxExtentsMin.y, lightBoxExtentsMax.y, nearPlane, -lightBoxExtentsMin.z);
        m_viewShadowMapCascadeMatrices[i] = m_shadowMapCascadeMatrices[i] * m_viewToLightMatrix;
        m_shadowMapCascadeMatrices[i] *= m_cameraViewMatrix;
    }
}

void Renderer::computePointLightShadowMatrices() {

    glm::vec3 directions[] = {
        {1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}
    };
    glm::vec3 upDirs[] = {
        {0, -1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}, {0, -1, 0}, {0, -1, 0}
    };

    for(uint32_t i = 0; i < m_inUsePointLightShadowMaps; ++i) {
        uint32_t lightIndex = m_pointLightShadowMapLightIndices[i];

        glm::mat4 proj = glm::perspective((float) M_PI/2.0f, 1.0f, m_parameters.pointLightShadowMapNear, m_pointLightBoundingSphereRadii[lightIndex]);
        glm::vec3 lightPos = m_pointLightPositions[lightIndex];
        for(uint32_t j = 0; j < 6; ++j) {
            m_pointLightShadowMapMatrices[i*6+j] = proj * glm::lookAt(lightPos, lightPos+directions[j], upDirs[j]);
        }
    }
}

void Renderer::renderShadowMap() {
    // Render depth for each cascade into depth texture array

    m_shadowMapRenderLayer.setEnabledDrawTargets({});
    for(uint32_t i = 0; i < m_parameters.shadowMapNumCascades; i++) {
        m_shadowMapRenderLayer.setDepthTexture(&m_lightDepthTexture, i);

        m_shadowMapRenderLayer.bind();
        glViewport(0, 0, m_parameters.shadowMapResolution, m_parameters.shadowMapResolution);
        glClear(GL_DEPTH_BUFFER_BIT);

        for(int j = 0; j < 2; ++j) {
            const Shader& activeShader = (j == 0) ? m_depthOnlyShaderSkinned : m_depthOnlyShader;
            activeShader.bind();

            const CallBucket& activeCallBucket = (j == 0) ? m_skinnedShadowMapCallBuckets[i] : m_shadowMapCallBuckets[i];
            if (activeCallBucket.numInstances == 0) continue;

            glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, activeCallBucket.instanceTransformBuffer);

            m_pBoundMesh = nullptr;
            for(uint32_t k = 0; k < activeCallBucket.callInfos.size(); ++k) {
                if(!activeCallBucket.usageFlags[k]) break;
                const CallInfo& callInfo = activeCallBucket.callInfos[k];
                const CallHeader& header = callInfo.header;

                if (j==0) {
                     activeShader.setUniform("numJoints", (uint32_t) header.pSkeletonDesc->getNumJoints());
                }

                activeShader.setUniform("transformBufferOffset", callInfo.transformBufferOffset);

                if(header.pMesh != m_pBoundMesh) {
                    m_pBoundMesh = header.pMesh;
                    m_pBoundMesh->bind();
                }

                glDrawElementsInstanced(m_pBoundMesh->getDrawType(), m_pBoundMesh->getIndexCount(), GL_UNSIGNED_INT, nullptr, callInfo.numInstances);
            }
        }
    }


    // Convert depth texture to variance shadow map

    // The depth texture is still bound to the depth channel, so make sure not
    // to override it. We don't need depth for a fullscreen quad anyway

    glDepthMask(GL_FALSE);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);

    m_depthToVarianceShader.bind();
    m_depthToVarianceShader.setUniform("enableEVSM", m_parameters.enableEVSM ? 1 : 0);
    m_depthToVarianceShader.setUniform("depthTextureArray", 0);

    m_lightDepthTexture.bind(0);
    m_fullScreenQuad.bind();
    for(uint32_t i = 0; i < m_parameters.shadowMapNumCascades; i++) {
        m_shadowMapRenderLayer.setEnabledDrawTargets({i});
        m_shadowMapRenderLayer.bind();
        glViewport(0, 0, m_parameters.shadowMapResolution, m_parameters.shadowMapResolution);
        glClear(GL_COLOR_BUFFER_BIT);

        m_depthToVarianceShader.setUniform("arrayLayer", i);

        m_fullScreenQuad.draw();
    }

    m_shadowMapRenderLayer.unbind();
}

void Renderer::filterShadowMap() {
    glDisable(GL_BLEND);

    //static const float gaussianKernel5[] = {0.06136, 0.24477, 0.38774, 0.24477, 0.06136};
    //static const float filterKernel3[] = {0.15, 0.7, 0.15};

    // 9-tap gaussian kernel weights (sigma = 2.0): [0.028532, 0.067234, 0.124009, 0.179044, 0.20236, 0.179044, 0.124009, 0.067234, 0.028532]
    // half-kernel: [0.20236, 0.179044, 0.124009, 0.067234, 0.028532]
    // combine 2nd and 3rd, 4th and 5th to exploit linear sampling: [0.20236, 0.303053, 0.095766]
    // texcoord offsets for linear sampled kernel (lerp between whole numbered offsets for the discrete sampled kernel using the kernel weights): [0.0, 1.409199051, 3.297934549]
    //static const float kernelWeights[] = {0.20236, 0.303053, 0.095766};
    //static const float kernelOffsets[] = {0.0, 1.409199051, 3.297934549};

    // 5-tap gaussian kernel weights : {0.06136, 0.24477, 0.38774, 0.24477, 0.06136}
    // half-kernel: {0.38774, 0.24477, 0.06136}
    // combine 2nd and 3rd to exploit linear sampling: {0.38774, 0.06136 + 0.24477 = 0.30613}
    // texcoord offsets for linear sampled kernel (lerp between whole numbered offsets for the discrete sampled kernel using the kernel weights): {0.0, (1.0 * 0.24477 + 2.0 * 0.06136) / 0.30613 = 1.200437723}
    static const float kernelWeights5t[] = {0.38774, 0.30613};
    static const float kernelOffsets5t[] = {0.0, 1.200437723};

    //m_fullScreenFilterShader.setUniformArray("kernel", 3, filterKernel3);

    m_horizontalFilterShader.bind();
    m_horizontalFilterShader.setUniform("tex_sampler", 0);
    m_horizontalFilterShader.setUniform("kernelWidth", 2);
    m_horizontalFilterShader.setUniformArray("kernelWeights", 2, kernelWeights5t);
    m_horizontalFilterShader.setUniformArray("kernelOffsets", 2, kernelOffsets5t);

    m_fullScreenQuad.bind();

    for(uint32_t i = 0; i < m_parameters.shadowMapNumCascades; i++) {
        // Downsample the cascade array layer into the filter texture object
        m_shadowMapRenderLayer.setEnabledReadTarget(i);
        m_shadowMapFilterRenderLayer.setEnabledDrawTargets({1});
        m_shadowMapRenderLayer.bind(GL_READ_FRAMEBUFFER);
        m_shadowMapFilterRenderLayer.bind(GL_DRAW_FRAMEBUFFER);
        glBlitFramebuffer(0, 0, m_parameters.shadowMapResolution, m_parameters.shadowMapResolution,
                          0, 0, m_parameters.shadowMapFilterResolution, m_parameters.shadowMapFilterResolution,
                          GL_COLOR_BUFFER_BIT, GL_LINEAR);

        // First pass filters horizontally
        m_shadowMapFilterRenderLayer.setEnabledDrawTargets({0});
        m_shadowMapFilterRenderLayer.bind();
        glViewport(0, 0, m_parameters.shadowMapFilterResolution, m_parameters.shadowMapFilterResolution);
        glClear(GL_COLOR_BUFFER_BIT);

        m_shadowMapFilterTextures[1].bind(0);

        float offsetScale = glm::pow(1.0f - m_parameters.shadowMapCascadeScale, i);  // we should shrink the blur radius for further away maps to keep visual blurring relatively constant
        m_horizontalFilterShader.setUniform("coordOffset", glm::vec2(offsetScale/m_parameters.shadowMapFilterResolution, 0.0));

        m_fullScreenQuad.draw();

        // Second pass filters vertically
        m_shadowMapFilterRenderLayer.setEnabledDrawTargets({1});
        m_shadowMapFilterRenderLayer.bind();
        glViewport(0, 0, m_parameters.shadowMapFilterResolution, m_parameters.shadowMapFilterResolution);
        glClear(GL_COLOR_BUFFER_BIT);

        m_shadowMapFilterTextures[0].bind(0);

        m_horizontalFilterShader.setUniform("coordOffset", glm::vec2(0.0, offsetScale/m_parameters.shadowMapFilterResolution));

        m_fullScreenQuad.draw();

        // upsample filtered texture to shadow render layer
        m_shadowMapFilterRenderLayer.setEnabledReadTarget(1);
        m_shadowMapRenderLayer.setEnabledDrawTargets({i});
        m_shadowMapFilterRenderLayer.bind(GL_READ_FRAMEBUFFER);
        m_shadowMapRenderLayer.bind(GL_DRAW_FRAMEBUFFER);
        glBlitFramebuffer(0, 0, m_parameters.shadowMapFilterResolution, m_parameters.shadowMapFilterResolution,
                          0, 0, m_parameters.shadowMapResolution, m_parameters.shadowMapResolution,
                          GL_COLOR_BUFFER_BIT, GL_LINEAR);
    }

    // Generate shadow map mipmaps
    // Not working for shadow maps with the deferred renderer currently
    if(m_shadowMapArrayTexture.getParameters().useMipmapFiltering) {
        m_shadowMapArrayTexture.generateMipmaps();
    }
}

void Renderer::renderPointLightShadows() {
    static const glm::vec3 dir[] = {
        {1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}
    };
    static const glm::vec3 up[] = {
        {0, -1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}, {0, -1, 0}, {0, -1, 0}
    };
    static const glm::vec3 right[] = {
        {0, 0, -1}, {0, 0, 1}, {1, 0, 0}, {1, 0, 0}, {1, 0, 0}, {-1, 0, 0}
    };

    // Render depth cubemap
    // depth clamping used to not clip occluders extremely close to the light
    // haven't noticed any artifacts due to this, though it will slightly affect variance
    glEnable(GL_DEPTH_CLAMP);
    glDisable(GL_BLEND);
    glDepthMask(GL_TRUE);
    glEnable(GL_DEPTH_TEST);
    m_pointLightShadowRenderLayer.setEnabledDrawTargets({});

    /*if(m_parameters.useCubemapGeometryShader) {
        // Render objects once per cube using a geometry shader and gl_Layer to split cube faces
        // Potentially lower draw call overhead, may help with lots of lights
        for(int i = 0; i < m_inUsePointLightShadowMaps; ++i) {
            m_pointLightShadowRenderLayer.setDepthTexture(m_pointLightShadowDepthMaps[i], -1);
            m_pointLightShadowRenderLayer.bind();
            glViewport(0, 0, m_parameters.pointLightShadowMapResolution, m_parameters.pointLightShadowMapResolution);
            glClear(GL_DEPTH_BUFFER_BIT);

            for(int j = 0; j < 2; ++j) {
                const Shader& activeShader = (j == 0) ? m_pointLightShadowMapShaderSkinned : m_pointLightShadowMapShader;
                activeShader.bind();

                const CallBucket& activeCallBucket = (j == 0) ? m_pointLightShadowCallBucketsSkinned[i] : m_pointLightShadowCallBuckets[i];
                if(activeCallBucket.numInstances == 0) continue;

                activeShader.setUniformArray("cubeFaceMatrices", 6, &m_pointLightShadowMapMatrices[6*i]);

                //glBindVertexArray(activeCallBucket.multiDrawVAO);
                glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, activeCallBucket.instanceTransformBuffer);
                //glBindBuffer(GL_DRAW_INDIRECT_BUFFER, activeCallBucket.multiDrawInfoBuffer);
                //glMultiDrawElementsIndirect(GL_TRIANGLES, GL_UNSIGNED_INT, 0, activeCallBucket.numMeshes, 0);


                m_pBoundMesh = nullptr;
                for(uint32_t k = 0; k < activeCallBucket.callInfos.size(); ++k) {
                    if(!activeCallBucket.usageFlags[k]) continue;
                    //const CallInfo* callInfo = activeCallBucket.callInfos[k];
                    const CallInfo& callInfo = activeCallBucket.callInfos[k];
                    const CallHeader& header = callInfo.header;
*/
                    /*if(j == 0) {
                        for(size_t jointID = 0; jointID < callInfo->header.pSkeleton->getJointCount(); ++jointID) {
                            const Joint* joint = callInfo->header.pSkeleton->getJoint(jointID);
                            m_skinningMatrices[jointID] = joint->getWorldMatrix() * joint->getInverseBindMatrix();
                        }
                        activeShader.setUniformArray("skinningMatrices", (int) callInfo->header.pSkeleton->getJointCount(), m_skinningMatrices.data());
                    }*/

/*                    //glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, callInfo->instanceTransformBuffer);
                    activeShader.setUniform("baseInstance", callInfo.baseInstance);

                    if(header.pMesh != m_pBoundMesh) {
                        m_pBoundMesh = header.pMesh;
                        m_pBoundMesh->bind();
                    }

                    glDrawElementsInstanced(m_pBoundMesh->getDrawType(), m_pBoundMesh->getIndexCount(), GL_UNSIGNED_INT, nullptr, callInfo.numInstances);
                }
            }
        }
    } else {*/
        // Render each face separately as a 2D target
        // Potentially 6x more draw calls, but may be better due to per-face culling
        for(uint32_t i = 0; i < m_inUsePointLightShadowMaps; ++i) {
            for(uint32_t ci = 0; ci < 6; ++ci) {
                m_pointLightShadowRenderLayer.setDepthTexture(m_pointLightShadowDepthMaps[i], ci);
                m_pointLightShadowRenderLayer.bind();
                m_pointLightShadowRenderLayer.validate();
                glViewport(0, 0, m_parameters.pointLightShadowMapResolution, m_parameters.pointLightShadowMapResolution);
                glClear(GL_DEPTH_BUFFER_BIT);

                for(int j = 0; j < 2; ++j) {
                    const Shader& activeShader = (j == 0) ? m_depthOnlyShaderSkinned : m_depthOnlyShader;
                    activeShader.bind();

                    const CallBucket& activeCallBucket = (j == 0) ? m_pointLightShadowCallBucketsSkinned[6*i+ci] : m_pointLightShadowCallBuckets[6*i+ci];
                    if (activeCallBucket.numInstances == 0) continue;

                    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, activeCallBucket.instanceTransformBuffer);

                    m_pBoundMesh = nullptr;
                    for(uint32_t k = 0; k < activeCallBucket.callInfos.size(); ++k) {
                        if(!activeCallBucket.usageFlags[k]) break;

                        const CallInfo& callInfo = activeCallBucket.callInfos[k];
                        const CallHeader& header = callInfo.header;

                        if (j==0) {
                             activeShader.setUniform("numJoints", (uint32_t) header.pSkeletonDesc->getNumJoints());
                        }

                        activeShader.setUniform("transformBufferOffset", callInfo.transformBufferOffset);

                        if(header.pMesh != m_pBoundMesh) {
                            m_pBoundMesh = header.pMesh;
                            m_pBoundMesh->bind();
                        }

                        glDrawElementsInstanced(m_pBoundMesh->getDrawType(), m_pBoundMesh->getIndexCount(), GL_UNSIGNED_INT, nullptr, callInfo.numInstances);

                    }
                }
            }
        }
    //}
    glDisable(GL_DEPTH_CLAMP);

    // Convert depth to variance
    glDepthMask(GL_FALSE);
    glDisable(GL_DEPTH_TEST);
    m_pointLightVarianceShader.bind();
    m_pointLightVarianceShader.setUniform("enableEVSM", m_parameters.enableEVSM ? 1 : 0);
    m_pointLightVarianceShader.setUniform("depthCubemap", 0);
    // remove depth texture for framebuffer completeness
    m_pointLightShadowRenderLayer.setDepthTexture(nullptr);
    m_fullScreenQuad.bind();
    for(uint32_t i = 0; i < m_inUsePointLightShadowMaps; ++i) {
        m_pointLightShadowDepthMaps[i]->bind(0);

        for(uint32_t j = 0; j < 6; ++j) {
            m_pointLightShadowRenderLayer.setTextureAttachment(0, m_pointLightShadowMaps[i], j);
            m_pointLightShadowRenderLayer.setEnabledDrawTargets({0});
            m_pointLightShadowRenderLayer.bind();
            glClear(GL_COLOR_BUFFER_BIT);

            m_pointLightVarianceShader.setUniform("faceDirection", dir[j]);
            m_pointLightVarianceShader.setUniform("faceUp", up[j]);
            m_pointLightVarianceShader.setUniform("faceRight", right[j]);

            m_fullScreenQuad.draw();
        }

    }
    m_pointLightShadowRenderLayer.setTextureAttachment(0, nullptr);

}

void Renderer::renderGBuffer() {
    glDisable(GL_BLEND);

    glDepthMask(GL_TRUE);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_STENCIL_TEST);
    glStencilFunc(GL_ALWAYS, 1, 0x01);
    glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
    glStencilMask(0xFFFFFFFF);

    m_gBufferRenderLayer.setEnabledDrawTargets({0, 1, 2});

    m_gBufferRenderLayer.bind();

    glViewport(0, 0, m_viewportWidth, m_viewportHeight);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);


    for(int i = 0; i < 2; i++) {
        const Shader& activeShader = (i == 0) ? m_gBufferShaderSkinned : m_gBufferShader;
        const CallBucket& activeCallBucket = (i == 0) ? m_skinnedCallBucket : m_defaultCallBucket;
        if (activeCallBucket.numInstances == 0) continue;

        activeShader.bind();
        activeShader.setUniform("diffuseTexture", 0);
        activeShader.setUniform("normalsTexture", 1);
        activeShader.setUniform("emissionTexture", 2);
        activeShader.setUniform("metallicRoughnessTexture", 3);
        activeShader.setUniform("useDiffuseTexture", 0);
        activeShader.setUniform("useNormalsTexture", 0);
        activeShader.setUniform("useEmissionTexture", 0);
        activeShader.setUniform("useMetallicRoughnessTexture", 0);

        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, activeCallBucket.instanceTransformBuffer);

        m_pBoundMesh = nullptr;
        m_pBoundDiffuseTexture = nullptr;
        m_pBoundNormalsTexture = nullptr;
        m_pBoundEmissionTexture = nullptr;
        m_pBoundMetallicRoughnessTexture = nullptr;
        bool uUseDiffuseTexture = false;
        bool uUseNormalsTexture = false;
        bool uUseEmissionTexture = false;
        bool uUseMetallicRoughnessTexture = false;
        for(uint32_t j = 0; j < activeCallBucket.callInfos.size(); ++j) {
            if(!activeCallBucket.usageFlags[j]) break;

            const CallInfo& callInfo = activeCallBucket.callInfos[j];
            const CallHeader& header = callInfo.header;

            if (i==0) {
                 activeShader.setUniform("numJoints", (uint32_t) header.pSkeletonDesc->getNumJoints());
            }

            activeShader.setUniform("transformBufferOffset", callInfo.transformBufferOffset);

            if(const Material* pMaterial = header.pMaterial) {
                activeShader.setUniform("color", pMaterial->getTintColor());
                activeShader.setUniform("roughness", pMaterial->getRoughness());
                activeShader.setUniform("metallic", pMaterial->getMetallic());
                activeShader.setUniform("emission", pMaterial->getEmissionIntensity());
                activeShader.setUniform("alpha", pMaterial->getAlpha());
                activeShader.setUniform("alphaMaskThreshold", pMaterial->getAlphaMaskThreshold());

                if(pMaterial->getDiffuseTexture() != nullptr) {
                    if(!uUseDiffuseTexture) {
                        activeShader.setUniform("useDiffuseTexture", 1);
                        uUseDiffuseTexture = true;
                    }
                    if(pMaterial->getDiffuseTexture() != m_pBoundDiffuseTexture) {
                        pMaterial->getDiffuseTexture()->bind(0);
                        m_pBoundDiffuseTexture = pMaterial->getDiffuseTexture();
                    }
                } else if(uUseDiffuseTexture){
                    activeShader.setUniform("useDiffuseTexture", 0);
                    uUseDiffuseTexture = false;
                }

                if(pMaterial->getNormalsTexture() != nullptr) {
                    if(!uUseNormalsTexture) {
                        activeShader.setUniform("useNormalsTexture", 1);
                        uUseNormalsTexture = true;
                    }
                    if(pMaterial->getNormalsTexture() != m_pBoundNormalsTexture) {
                        pMaterial->getNormalsTexture()->bind(1);
                        m_pBoundNormalsTexture = pMaterial->getNormalsTexture();
                    }
                } else if(uUseNormalsTexture) {
                    activeShader.setUniform("useNormalsTexture", 0);
                    uUseNormalsTexture = false;
                }

                if(pMaterial->getEmissionTexture() != nullptr) {
                    if(!uUseEmissionTexture) {
                        activeShader.setUniform("useEmissionTexture", 1);
                        uUseEmissionTexture = true;
                    }
                    if(pMaterial->getEmissionTexture() != m_pBoundEmissionTexture) {
                        pMaterial->getEmissionTexture()->bind(2);
                        m_pBoundEmissionTexture = pMaterial->getEmissionTexture();
                    }
                } else if(uUseEmissionTexture) {
                    activeShader.setUniform("useEmissionTexture", 0);
                    uUseEmissionTexture = false;
                }

                if(pMaterial->getMetallicRoughnessTexture() != nullptr) {
                    if(!uUseMetallicRoughnessTexture) {
                        activeShader.setUniform("useMetallicRoughnessTexture", 1);
                        uUseMetallicRoughnessTexture = true;
                    }
                    if(pMaterial->getMetallicRoughnessTexture() != m_pBoundMetallicRoughnessTexture) {
                        pMaterial->getMetallicRoughnessTexture()->bind(3);
                        m_pBoundMetallicRoughnessTexture = pMaterial->getMetallicRoughnessTexture();
                    }
                } else if(uUseMetallicRoughnessTexture) {
                    activeShader.setUniform("useMetallicRoughnessTexture", 0);
                    uUseMetallicRoughnessTexture = false;
                }
            } else {
                if(uUseDiffuseTexture){
                    activeShader.setUniform("useDiffuseTexture", 0);
                    uUseDiffuseTexture = false;
                }
                if(uUseNormalsTexture) {
                    activeShader.setUniform("useNormalsTexture", 0);
                    uUseNormalsTexture = false;
                }
                if(uUseEmissionTexture) {
                    activeShader.setUniform("useEmissionTexture", 0);
                    uUseEmissionTexture = false;
                }
                if(uUseMetallicRoughnessTexture) {
                    activeShader.setUniform("useMetallicRoughnessTexture", 0);
                    uUseMetallicRoughnessTexture = false;
                }
                activeShader.setUniform("color", glm::vec3(1.0, 0.0, 1.0));
                activeShader.setUniform("metallic", 0.0f);
                activeShader.setUniform("roughness", 1.0f);
                activeShader.setUniform("emission", glm::vec3(0.0));
            }

            if(header.pMesh != m_pBoundMesh) {
                m_pBoundMesh = header.pMesh;
                m_pBoundMesh->bind();
            }

            VKR_DEBUG_CALL(glDrawElementsInstanced(m_pBoundMesh->getDrawType(), m_pBoundMesh->getIndexCount(), GL_UNSIGNED_INT, nullptr, callInfo.numInstances));
        }
    }
    //glStencilMask(0xFF);

    glDisable(GL_STENCIL_TEST);
    m_gBufferRenderLayer.unbind();
}

void Renderer::renderSSAOMap() {
    glClearColor(1.0, 0.0, 0.0, 0.0);
    glDisable(GL_BLEND);

    m_ssaoRenderLayer.setEnabledDrawTargets({0});
    m_ssaoRenderLayer.bind();
    glViewport(0, 0, m_ssaoTargetTexture.getParameters().width, m_ssaoTargetTexture.getParameters().height);
    glClear(GL_COLOR_BUFFER_BIT);

    m_ssaoShader.bind();
    m_ssaoShader.setUniformArray("ssaoKernelSamples", (int) m_ssaoKernel.size(), m_ssaoKernel.data());
    m_ssaoShader.setUniform("projection", m_cameraProjectionMatrix);
    m_ssaoShader.setUniform("inverseProjection", m_projectionInverse);
    m_ssaoShader.setUniform("gBufferDepth", 0);
    m_ssaoShader.setUniform("gBufferNormalViewSpace", 1);
    m_ssaoShader.setUniform("randomRotationTexture", 2);

    m_gBufferDepthTexture.bind(0);
    m_gBufferNormalViewSpaceTexture.bind(1);
    m_ssaoNoiseTexture.bind(2);

    m_fullScreenQuad.bind();
    m_fullScreenQuad.draw();

    m_ssaoRenderLayer.unbind();

    glClearColor(0.0, 0.0, 0.0, 0.0);

    // filter ssao texture

    // 9-tap gaussian kernel weights (sigma = 2.0): [0.028532, 0.067234, 0.124009, 0.179044, 0.20236, 0.179044, 0.124009, 0.067234, 0.028532]
    // half-kernel: [0.20236, 0.179044, 0.124009, 0.067234, 0.028532]
    // combine 2nd and 3rd, 4th and 5th to exploit linear sampling: [0.20236, 0.303053, 0.095766]
    // texcoord offsets for linear sampled kernel (lerp between whole numbered offsets for the discrete sampled kernel using the kernel weights): [0.0, 1.409199051, 3.297934549]

    // 5-tap gaussian: [0.06136, 0.24477, 0.38774, 0.24477, 0.06136]
    // half: [0.38774, 0.24477, 0.06136]
    // combine weights: [0.38774, 0.24477 + 0.06136] = [0.38774, 0.30613]
    // offsets: [0.0, lerp(1.0, 2.0, 0.06136 / 0.30613)] = [0.0, 1.20043772253]

    //static const float kernelWeights[] = {0.38774, 0.30613};
    //static const float kernelOffsets[] = {0.0, 1.20043772253};

    static const float kernelWeights[] = {0.2, 0.4};
    static const float kernelOffsets[] = {0.0, 1.5};

    //m_fullScreenFilterShader.setUniformArray("kernel", 3, filterKernel3);

    m_horizontalFilterShader.bind();
    m_horizontalFilterShader.setUniform("tex_sampler", 0);
    m_horizontalFilterShader.setUniform("kernelWidth", 2);
    m_horizontalFilterShader.setUniformArray("kernelWeights", 2, kernelWeights);
    m_horizontalFilterShader.setUniformArray("kernelOffsets", 2, kernelOffsets);


    m_ssaoFilterRenderLayer.setEnabledDrawTargets({0});
    m_ssaoFilterRenderLayer.bind();
    glViewport(0, 0, m_ssaoTargetTexture.getParameters().width, m_ssaoTargetTexture.getParameters().height);
    glClear(GL_COLOR_BUFFER_BIT);

    m_horizontalFilterShader.setUniform("coordOffset", glm::vec2(1.0/m_ssaoTargetTexture.getParameters().width, 0.0));

    m_ssaoTargetTexture.bind(0);
    m_fullScreenQuad.draw();

    m_ssaoFilterRenderLayer.setEnabledDrawTargets({1});
    m_ssaoFilterRenderLayer.bind();
    glViewport(0, 0, m_ssaoTargetTexture.getParameters().width, m_ssaoTargetTexture.getParameters().height);
    glClear(GL_COLOR_BUFFER_BIT);

    m_horizontalFilterShader.setUniform("coordOffset", glm::vec2(0.0, 1.0/m_ssaoTargetTexture.getParameters().height));

    m_ssaoFilterTexture.bind(0);
    m_fullScreenQuad.draw();

    m_ssaoFilterRenderLayer.unbind();
}


//#include <glm/gtx/string_cast.hpp>
void Renderer::renderDeferredPass() {
    // Blit depth info from GBuffer to scene RB (downsample)
    /*m_gBufferRenderLayer.bind(GL_READ_FRAMEBUFFER);
    m_sceneRenderLayer.bind(GL_DRAW_FRAMEBUFFER);
    VKR_DEBUG_CALL(glBlitFramebuffer(
        0, 0, m_viewportWidth, m_viewportHeight,
        0, 0, m_viewportWidth, m_viewportHeight,
        GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT,
        GL_NEAREST));*/
    glCopyImageSubData(m_gBufferDepthTexture.getHandle(), GL_TEXTURE_2D, 0, 0, 0, 0,
                       m_sceneDepthStencilRenderBuffer.getHandle(), GL_RENDERBUFFER, 0, 0, 0, 0,
                       m_viewportWidth, m_viewportHeight, 1);

    // Render deferred pass

    // Depth writes disabled for entire deferred pass
    // Don't want light bounding volumes to interfere with scene depth info

    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);

    glEnable(GL_STENCIL_TEST);
    glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);

    // enable additive blending
    glEnable(GL_BLEND);
    glBlendFuncSeparate(GL_ONE, GL_ONE, GL_ONE, GL_ZERO);


    m_sceneRenderLayer.setEnabledDrawTargets({0});
    m_sceneRenderLayer.bind();
    glViewport(0, 0, m_viewportWidth, m_viewportHeight);
    glClearColor(0, 0, 0, 0);
    glClear(GL_COLOR_BUFFER_BIT);


    // Lighting passes

    m_gBufferDepthTexture.bind(0);
    m_gBufferNormalViewSpaceTexture.bind(1);
    m_gBufferAlbedoMetallicTexture.bind(2);
    m_gBufferEmissionRoughnessTexture.bind(3);
    m_gBufferDepthTexture.bind(5);

    // Directional lighting pass
    if (m_directionalLightEnabled) {
        m_fullScreenQuad.bind();

        glStencilFunc(GL_NOTEQUAL, 0, 0x01);

        m_deferredDirectionalLightShader.bind();
        m_deferredDirectionalLightShader.setUniform("lightDirectionViewSpace", m_lightDirectionViewSpace);
        m_deferredDirectionalLightShader.setUniform("lightIntensity", m_lightIntensity);

        // TODO: use a UBO for shadow parameters (soft todo lol)
        if (m_directionalShadowsEnabled) {
            m_deferredDirectionalLightShader.setUniform("enableShadows", 1);
            m_deferredDirectionalLightShader.setUniformArray("shadowCascadeMatrices", m_parameters.shadowMapNumCascades, m_viewShadowMapCascadeMatrices.data());
            m_deferredDirectionalLightShader.setUniformArray("cascadeSplitDepths", m_parameters.shadowMapNumCascades, m_shadowMapCascadeSplitDepths.data());
            m_deferredDirectionalLightShader.setUniformArray("cascadeBlurRanges", m_parameters.shadowMapNumCascades, m_shadowMapCascadeBlurRanges.data());
            m_deferredDirectionalLightShader.setUniform("numShadowCascades", m_parameters.shadowMapNumCascades);
            m_deferredDirectionalLightShader.setUniform("lightBleedCorrectionBias", m_parameters.shadowMapLightBleedCorrectionBias);
            m_deferredDirectionalLightShader.setUniform("lightBleedCorrectionPower", m_parameters.shadowMapLightBleedCorrectionPower);
        } else {
            m_deferredDirectionalLightShader.setUniform("enableShadows", 0);
        }

        m_deferredDirectionalLightShader.setUniform("inverseProjection", m_projectionInverse);

        //m_deferredDirectionalLightShader.setUniform("gBufferPositionViewSpace", 0);
        m_deferredDirectionalLightShader.setUniform("gBufferNormalViewSpace", 1);
        m_deferredDirectionalLightShader.setUniform("gBufferAlbedoMetallic", 2);
        m_deferredDirectionalLightShader.setUniform("gBufferEmissionRoughness", 3);
        m_deferredDirectionalLightShader.setUniform("gBufferDepth", 0);

        m_deferredDirectionalLightShader.setUniform("shadowMap", 4);
        m_shadowMapArrayTexture.bind(4);

        m_deferredDirectionalLightShader.setUniform("enableEVSM", m_parameters.enableEVSM ? 1 : 0);

        m_fullScreenQuad.draw();
    }

    // stencil point light passes
    if (m_parameters.enablePointLights && !m_pointLightPositions.empty()) {
        glCullFace(GL_FRONT);

        m_pointLightSphere.bind();

        std::vector<glm::mat4> pointLightMVPs(m_pointLightPositions.size());
        glStencilMask(0xFE);
        for(size_t i = 0; i < m_pointLightPositions.size(); ++i) {
            glm::mat4 pointLightModelMatrix =
                glm::translate(glm::mat4(1.0f), m_pointLightPositions[i]) *
                glm::mat4(glm::mat3(m_pointLightBoundingSphereRadii[i]));

            pointLightMVPs[i] = m_viewProj * pointLightModelMatrix;

            // Set state for stencil render
            glDisable(GL_CULL_FACE);
            glEnable(GL_DEPTH_TEST);
            glStencilFunc(GL_ALWAYS, 0, 0xFE);
            glStencilOpSeparate(GL_BACK, GL_KEEP, GL_INCR_WRAP, GL_KEEP);
            glStencilOpSeparate(GL_FRONT, GL_KEEP, GL_DECR_WRAP, GL_KEEP);

            // Configure render layer for writing to depth-stencil attachment only
            m_sceneRenderLayer.setEnabledDrawTargets({});
            m_sceneRenderLayer.bind();
            //glClear(GL_STENCIL_BUFFER_BIT);

            // Render light bounds to stencil buffer
            m_passthroughShader.bind();
            m_passthroughShader.setUniform("modelViewProj", pointLightMVPs[i]);

            m_pointLightSphere.draw();

            // Set state for lighting pass

            glDisable(GL_DEPTH_TEST);
            glEnable(GL_CULL_FACE);
            glStencilFunc(GL_NOTEQUAL, 0, 0xFE);

            // Configure render layer for color writes
            m_sceneRenderLayer.setEnabledDrawTargets({0});
            m_sceneRenderLayer.bind();

            // Choose shader (unshadowed vs shadowed)
            int mapIndex = m_pointLightShadowMapIndices[i];
            const Shader& plShader = (mapIndex < 0) ? m_deferredPointLightShader : m_deferredPointLightShaderShadow;
            plShader.bind();

            // Render lighting pass
            if(mapIndex >= 0) {
                plShader.setUniform("shadowMap", 4);
                m_pointLightShadowMaps[mapIndex]->bind(4);
                plShader.setUniform("inverseView", m_viewInverse);
                plShader.setUniformArray("cubeFaceMatrices", 6, &m_pointLightShadowMapMatrices[mapIndex*6]);
                plShader.setUniform("lightBleedCorrectionBias", m_parameters.shadowMapLightBleedCorrectionBias);
                plShader.setUniform("lightBleedCorrectionPower", m_parameters.shadowMapLightBleedCorrectionPower);
                plShader.setUniform("enableEVSM", m_parameters.enableEVSM ? 1 : 0);
            }

            //plShader.setUniform("gBufferPositionViewSpace", 0);
            plShader.setUniform("gBufferNormalViewSpace", 1);
            plShader.setUniform("gBufferAlbedoMetallic", 2);
            plShader.setUniform("gBufferEmissionRoughness", 3);
            plShader.setUniform("gBufferDepth", 0);

            plShader.setUniform("inverseProjection", m_projectionInverse);
            plShader.setUniform("pixelSize", glm::vec2(1.0/m_viewportWidth, 1.0/m_viewportHeight));

            plShader.setUniform("minIntensity", m_parameters.pointLightMinIntensity);
            plShader.setUniform("lightPositionViewSpace", glm::vec3(m_cameraViewMatrix * glm::vec4(m_pointLightPositions[i], 1.0)));
            plShader.setUniform("lightIntensity", m_pointLightIntensities[i]);
            plShader.setUniform("modelViewProj", pointLightMVPs[i]);

            m_pointLightSphere.draw();
        }
        //glStencilMask(0xFF);
        glCullFace(GL_BACK);
    }

    // Ambient + Emission pass (done last to maximize pipeline time for SSAO pass)

    {
        glStencilFunc(GL_NOTEQUAL, 0, 0x01);

        m_deferredAmbientEmissionShader.bind();
        //m_deferredAmbientEmissionShader.setUniform("gBufferPositionViewSpace", 0);
        //m_deferredAmbientEmissionShader.setUniform("gBufferNormalViewSpace", 1);
        m_deferredAmbientEmissionShader.setUniform("gBufferAlbedoMetallic", 2);
        m_deferredAmbientEmissionShader.setUniform("gBufferEmissionRoughness", 3);

        m_deferredAmbientEmissionShader.setUniform("enableSSAO", m_parameters.enableSSAO ? 1 : 0);
        m_deferredAmbientEmissionShader.setUniform("ssaoMap", 4);
        m_ssaoTargetTexture.bind(4);

        m_deferredAmbientEmissionShader.setUniform("ambientPower", m_parameters.ambientPower);
        m_deferredAmbientEmissionShader.setUniform("ambientIntensity", m_ambientLightIntensity);

        m_fullScreenQuad.bind();
        m_fullScreenQuad.draw();
    }

    // Put state back to normal for composite pass
    glDisable(GL_STENCIL_TEST);

    m_sceneRenderLayer.unbind();
}

void Renderer::renderBackground() {
    // Background pass

    // bayerCoords[i] = {x, y} where bayer4x4[x][y] = i
    static const glm::vec2 bayerCoords[16] = {
        {0, 0}, {2, 2}, {2, 0}, {0, 2},
        {1, 1}, {3, 3}, {3, 1}, {1, 3},
        {1, 0}, {3, 2}, {3, 0}, {1, 2},
        {0, 1}, {2, 3}, {2, 1}, {0, 3}
    };

    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);

    m_cloudsRenderLayer.setEnabledDrawTargets({0});
    m_cloudsRenderLayer.bind();
    glViewport(0, 0, m_cloudsTextureWidth, m_cloudsTextureHeight);
    glClear(GL_COLOR_BUFFER_BIT);

    m_fullScreenQuad.bind();

    if (rebuildCloudTextures) {
        createCloudTexture();
        rebuildCloudTextures = false;
    }
    if (reloadCloudShader) {
        m_backgroundShader.linkShaderFiles("shaders/vertex_fs.glsl", "shaders/fragment_background.glsl");
        reloadCloudShader = false;
    }

    m_backgroundShader.bind();
    m_backgroundShader.setUniform("tanFovX", m_tanCameraFovX);
    m_backgroundShader.setUniform("tanFovY", m_tanCameraFovY);
    m_backgroundShader.setUniform("inverseView", glm::inverse(m_cameraViewMatrix));
    m_backgroundShader.setUniform("sunDirection", m_lightDirection);
    m_backgroundShader.setUniform("sunIntensity", m_lightIntensity);
    m_backgroundShader.setUniform("worldPos", m_cameraPosition);
    m_backgroundShader.setUniform("pixelOffset", bayerCoords[m_frameCount%16u] / glm::vec2(m_viewportWidth, m_viewportHeight));
    m_backgroundShader.setUniform("time", static_cast<float>(glfwGetTime()));
    m_backgroundShader.setUniform("frame", (unsigned int) m_frameCount);
    m_backgroundShader.setUniform("cloudiness", cloudiness);
    m_backgroundShader.setUniform("cloudDensity", cloudDensity);
    m_backgroundShader.setUniform("fExtinction", cloudExtinction);
    m_backgroundShader.setUniform("fScattering", cloudScattering);
    m_backgroundShader.setUniform("cloudNoiseBaseScale", cloudNoiseBaseScale);
    m_backgroundShader.setUniform("cloudNoiseDetailScale", cloudNoiseDetailScale);
    m_backgroundShader.setUniform("detailNoiseInfluence", cloudDetailNoiseInfluence);
    m_backgroundShader.setUniform("rPlanet", planetRadius);
    m_backgroundShader.setUniform("cloudBottom", cloudBottomHeight);
    m_backgroundShader.setUniform("cloudTop", cloudTopHeight);
    m_backgroundShader.setUniform("cloudSteps", cloudSteps);
    m_backgroundShader.setUniform("shadowSteps", cloudShadowSteps);
    m_backgroundShader.setUniform("shadowDist", cloudShadowStepSize);
    m_backgroundShader.setUniform("baseNoiseContribution", cloudBaseNoiseContribution);
    m_backgroundShader.setUniform("baseNoiseThreshold", cloudBaseNoiseThreshold);

    VKR_DEBUG_CALL(m_cloudNoiseBaseTexture.bind(0));
    m_backgroundShader.setUniform("cloudNoiseBase", 0);

    VKR_DEBUG_CALL(m_cloudNoiseDetailTexture.bind(1));
    m_backgroundShader.setUniform("cloudNoiseDetail", 1);

    m_fullScreenQuad.draw();

    if (m_cloudHistoryValid) {
        m_cloudsAccumRenderLayer.setEnabledDrawTargets({(unsigned int) ((m_frameCount+1u)%2u)});
        m_cloudsAccumRenderLayer.bind();
        glViewport(0, 0, m_viewportWidth, m_viewportHeight);
        glClear(GL_COLOR_BUFFER_BIT);
        m_cloudsAccumShader.bind();
        m_cloudsAccumShader.setUniform("frame", (unsigned int) m_frameCount);
        m_cloudsAccumShader.setUniform("currentFrame", 0);
        m_cloudsAccumShader.setUniform("history", 1);
        m_cloudsAccumShader.setUniform("motionBuffer", 2);
        m_cloudsRenderTexture.bind(0);
        m_cloudsHistoryTexture[m_frameCount%2u].bind(1);
        m_motionBuffer.bind(2);
        m_fullScreenQuad.draw();
    } else {
        m_cloudsAccumRenderLayer.setEnabledDrawTargets({(unsigned int) ((m_frameCount+1u)%2u)});
        m_cloudsRenderLayer.setEnabledReadTarget(0);
        m_cloudsAccumRenderLayer.bind(GL_DRAW_FRAMEBUFFER);
        m_cloudsRenderLayer.bind(GL_READ_FRAMEBUFFER);
        glBlitFramebuffer(0, 0, m_viewportWidth/4, m_viewportHeight/4, 0, 0, m_viewportWidth, m_viewportHeight, GL_COLOR_BUFFER_BIT, GL_LINEAR);
        m_cloudHistoryValid = true;
    }


    // draw the clouds onto the scene texture, but do it "underneath"
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE_MINUS_DST_ALPHA, GL_DST_ALPHA);

    m_sceneRenderLayer.setEnabledDrawTargets({0});
    m_sceneRenderLayer.bind();
    glViewport(0, 0, m_viewportWidth, m_viewportHeight);

    m_fullScreenShader.bind();
    m_fullScreenShader.setUniform("texSampler", 0);
    m_fullScreenShader.setUniform("enableGammaCorrection", 0);
    m_fullScreenShader.setUniform("enableToneMapping", 0);
    m_cloudsHistoryTexture[(m_frameCount+1u)%2u].bind(0);

    m_fullScreenQuad.draw();
}

void Renderer::renderBloom() {
    glDepthMask(GL_FALSE);
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    m_fullScreenQuad.bind();

    m_bloomRenderLayer[0].setTextureAttachment(0, m_bloomTextures[0]);
    m_bloomRenderLayer[0].setEnabledDrawTargets({0});
    m_bloomRenderLayer[0].bind();
    glViewport(0, 0, m_viewportWidth, m_viewportHeight);
    //glClear(GL_COLOR_BUFFER_BIT);
    glClear(GL_COLOR_BUFFER_BIT);

    m_hdrExtractShader.bind();
    m_hdrExtractShader.setUniform("texSampler", 0);
    m_sceneTexture.bind(0);

    m_fullScreenQuad.draw();

    // do blurring for bloom effect
    static const float kernelWeights[] = {0.20236, 0.303053, 0.095766};
    static const float kernelOffsets[] = {0.0, 1.409199051, 3.297934549};

    //m_fullScreenFilterShader.setUniformArray("kernel", 3, filterKernel3);

    m_horizontalFilterShader.bind();
    m_horizontalFilterShader.setUniform("tex_sampler", 0);
    m_horizontalFilterShader.setUniform("kernelWidth", 3);
    m_horizontalFilterShader.setUniformArray("kernelWeights", 3, kernelWeights);
    m_horizontalFilterShader.setUniformArray("kernelOffsets", 3, kernelOffsets);

    //int nBloomIterations = 1;
    for(uint32_t i = 0; i < m_parameters.numBloomLevels; ++i) {
        TextureParameters cLevelParameters = m_bloomTextures[2*i]->getParameters();

        m_bloomRenderLayer[i%2].setTextureAttachment(0, m_bloomTextures[2*i]);
        m_bloomRenderLayer[i%2].setTextureAttachment(1, m_bloomTextures[2*i+1]);

        if(i > 0) {
            TextureParameters pLevelParameters = m_bloomTextures[2*i-1]->getParameters();
            m_bloomRenderLayer[(i-1)%2].setEnabledReadTarget(0);
            m_bloomRenderLayer[i%2].setEnabledDrawTargets({0});
            m_bloomRenderLayer[(i-1)%2].bind(GL_READ_FRAMEBUFFER);
            m_bloomRenderLayer[i%2].bind(GL_DRAW_FRAMEBUFFER);
            glViewport(0, 0, cLevelParameters.width, cLevelParameters.height);
            glClear(GL_COLOR_BUFFER_BIT);
            glBlitFramebuffer(
                0, 0, pLevelParameters.width, pLevelParameters.height,
                0, 0, cLevelParameters.width, cLevelParameters.height,
                GL_COLOR_BUFFER_BIT, GL_LINEAR);
            m_bloomRenderLayer[(i-1)%2].setTextureAttachment(0, nullptr);
            m_bloomRenderLayer[(i-1)%2].setTextureAttachment(1, nullptr);
        }

        // First pass filters horizontally
        m_bloomRenderLayer[i%2].setEnabledDrawTargets({1});
        m_bloomRenderLayer[i%2].validate();
        m_bloomRenderLayer[i%2].bind();
        glViewport(0, 0, cLevelParameters.width, cLevelParameters.height);
        glClear(GL_COLOR_BUFFER_BIT);

        m_bloomTextures[2*i]->bind(0);

        m_horizontalFilterShader.setUniform("coordOffset", glm::vec2(1.0f/cLevelParameters.width, 0.0));

        m_fullScreenQuad.draw();

        // Second pass filters vertically
        m_bloomRenderLayer[i%2].setEnabledDrawTargets({0});
        m_bloomRenderLayer[i%2].validate();
        m_bloomRenderLayer[i%2].bind();
        glViewport(0, 0, cLevelParameters.width, cLevelParameters.height);
        glClear(GL_COLOR_BUFFER_BIT);

        m_bloomTextures[2*i+1]->bind(0);

        m_horizontalFilterShader.setUniform("coordOffset", glm::vec2(0.0, 1.0f/cLevelParameters.height));

        m_fullScreenQuad.draw();
    }
    m_bloomRenderLayer[0].setTextureAttachment(1, nullptr);

    // Upsample and accumulate results into level-0 texture with additive blending
    glBlendFunc(GL_ONE, GL_ONE);
    //glBlendFuncSeparate(GL_ONE, GL_ONE, GL_ONE, GL_ZERO);
    m_fullScreenShader.bind();
    m_fullScreenShader.setUniform("tex_sampler", 0);
    m_fullScreenShader.setUniform("enableGammaCorrection", 0);
    m_fullScreenShader.setUniform("enableToneMapping", 0);
    for (int i = m_parameters.numBloomLevels-1; i > 0; --i) {
        m_bloomRenderLayer[0].setTextureAttachment(0, m_bloomTextures[2*(i-1)]);
        m_bloomRenderLayer[0].setEnabledDrawTargets({0});
        m_bloomRenderLayer[0].bind();

        glViewport(0, 0, m_bloomTextures[2*(i-1)]->getParameters().width, m_bloomTextures[2*(i-1)]->getParameters().height);

        m_bloomTextures[2*i]->bind(0);

        m_fullScreenQuad.draw();
    }

    m_sceneRenderLayer.bind();
    glViewport(0, 0, m_viewportWidth, m_viewportHeight);

    m_bloomTextures[0]->bind(0);
    m_fullScreenQuad.draw();

    m_sceneRenderLayer.unbind();
}

void Renderer::renderCompositePass() {
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    m_sceneRenderLayer.unbind();  // ensure we are drawing to the default framebuffer
    glViewport(0, 0, m_viewportWidth, m_viewportHeight);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    m_fullScreenShader.bind();
    m_fullScreenQuad.bind();

    m_fullScreenShader.setUniform("texSampler", 0);
    m_fullScreenShader.setUniform("enableGammaCorrection", 1);
    m_fullScreenShader.setUniform("enableToneMapping", 1);

    m_fullScreenShader.setUniform("exposure", m_parameters.exposure);
    m_fullScreenShader.setUniform("gamma", m_parameters.gamma);

    //m_cloudsHistoryTexture[(m_frameCount+1u)%2u].bind(0);
    //m_fullScreenQuad.draw();

    m_sceneTexture.bind(0);
    m_fullScreenQuad.draw();
    // This can be used for debugging to draw a different texture in the
    // lower right corner of the window

   //glViewport(2*m_viewportWidth/3, 0, m_viewportWidth/3, m_viewportHeight/3);

    /*m_gBufferNormalViewSpaceTexture.bind(0);
    m_fullScreenShader.setUniform("enableGammaCorrection", 0);
    m_fullScreenShader.setUniform("enableToneMapping", 0);
    m_fullScreenQuad.draw();*/

    //m_cloudsHistoryTexture[(m_frameCount+1u)%2u].bind(0);
    //m_motionBuffer.bind(0);
    //m_fullScreenShader.setUniform("enableGammaCorrection", 0);
    //m_fullScreenShader.setUniform("enableToneMapping", 0);
    //m_fullScreenShader.setUniform("scaleAndShift", 1);
    //m_fullScreenQuad.draw();
    //m_fullScreenShader.setUniform("scaleAndShift", 0);

    /*m_pointLightShadowMaps[0]->bind(1);

    m_fullScreenShader.setUniform("cubeSampler", 1);
    m_fullScreenShader.setUniform("cube", 1);
    m_fullScreenShader.setUniform("enableGammaCorrection", 0);
    m_fullScreenShader.setUniform("enableToneMapping", 0);

    m_fullScreenQuad.draw();
    m_fullScreenShader.setUniform("cube", 0);*/


    /*m_fullScreenShader.setUniform("enableGammaCorrection", 0);
    m_fullScreenShader.setUniform("enableToneMapping", 0);
    m_worleyTexture.bind(0);
    glViewport(2*m_viewportWidth/3, 0, 256, 256);
    m_fullScreenShader.setUniform("mask", glm::vec4(1, 0, 0, 0));
    m_fullScreenQuad.draw();
    glViewport(2*m_viewportWidth/3+256, 0, 256, 256);
    m_fullScreenShader.setUniform("mask", glm::vec4(0, 1, 0, 0));
    m_fullScreenQuad.draw();
    glViewport(2*m_viewportWidth/3, 256, 256, 256);
    m_fullScreenShader.setUniform("mask", glm::vec4(0, 0, 1, 0));
    m_fullScreenQuad.draw();
    glViewport(2*m_viewportWidth/3+256, 256, 256, 256);
    m_fullScreenShader.setUniform("mask", glm::vec4(0, 0, 0, 1));
    m_fullScreenQuad.draw();
    m_fullScreenShader.setUniform("mask", glm::vec4(1));*/

}

void Renderer::renderBackgroundMotionVectors() {
    glDisable(GL_BLEND);
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);


    // Camera motion vectors

    m_fullScreenQuad.bind();

    // render motion vectors for the background, computed slightly differently than the depth-buffer camera motion vectors
    // because it's based on ray directions instead of raster positions
    m_motionRenderLayer.setEnabledDrawTargets({0});
    m_motionRenderLayer.validate();
    m_motionRenderLayer.bind();
    glViewport(0, 0, m_viewportWidth, m_viewportHeight);
    glClearColor(0.0, 0.0, 0.0, 1.0);
    glClear(GL_COLOR_BUFFER_BIT);

    m_backgroundMotionShader.bind();
    m_backgroundMotionShader.setUniform("inverseProjection", m_projectionNoJitterInverse);
    m_backgroundMotionShader.setUniform("reprojection", m_projectionNoJitter * m_lastView * m_viewInverse);
    m_fullScreenQuad.draw();
}

void Renderer::renderMotionVectors() {
    glDisable(GL_BLEND);

    m_motionRenderLayer.setEnabledDrawTargets({0});
    m_motionRenderLayer.validate();
    m_motionRenderLayer.bind();
    if (!m_parameters.enableBackgroundLayer) {
        glViewport(0, 0, m_viewportWidth, m_viewportHeight);
        glClearColor(0.0, 0.0, 0.0, 1.0);
        glClear(GL_COLOR_BUFFER_BIT);
    }

    // Camera motion vectors

    m_fullScreenQuad.bind();

    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);

    if (m_parameters.enableBackgroundLayer) {
        glEnable(GL_STENCIL_TEST);
        glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);

        // Use the scene stencil info to avoid overriding motion vectors for the background
        // by only rendering where there is scene geometry
        m_motionRenderLayer.clearAttachment(GL_DEPTH_STENCIL_ATTACHMENT);
        m_motionRenderLayer.setDepthTexture(&m_gBufferDepthTexture);
        m_motionRenderLayer.setEnabledDrawTargets({0});
        m_motionRenderLayer.validate();
        m_motionRenderLayer.bind();
        glViewport(0, 0, m_viewportWidth, m_viewportHeight);

        glStencilFunc(GL_EQUAL, 1, 0x01);  // lowest stencil bit =1 => geometry, ie depth buffer valid
    }

    m_cameraMotionShader.bind();
    m_cameraMotionShader.setUniform("gBufferDepth", 0);
    m_cameraMotionShader.setUniform("lastViewProj", m_lastViewProj);
    m_cameraMotionShader.setUniform("inverseViewProj", m_inverseViewProj);
    m_gBufferDepthTexture.bind(0);
    m_fullScreenQuad.draw();

    if (m_parameters.enableBackgroundLayer) {
        glDisable(GL_STENCIL_TEST);
        m_motionRenderLayer.clearAttachment(GL_DEPTH_STENCIL_ATTACHMENT);
        m_motionRenderLayer.setRenderBufferAttachment(&m_motionVectorsDepthBuffer);
        m_motionRenderLayer.setEnabledDrawTargets({0});
        m_motionRenderLayer.validate();
        m_motionRenderLayer.bind();
        glViewport(0, 0, m_viewportWidth, m_viewportHeight);
    }

    // Object motion vectors

    glDepthMask(GL_TRUE);
    glEnable(GL_DEPTH_TEST);
    glClear(GL_DEPTH_BUFFER_BIT);

    for(int i = 0; i < 2; i++) {
        const Shader& activeShader = (i == 0) ? m_objectMotionShaderSkinned : m_objectMotionShader;
        const CallBucket& activeCallBucket = (i == 0) ? m_motionVectorsCallBucketSkinned : m_motionVectorsCallBucket;
        if (activeCallBucket.numInstances == 0) continue;

        activeShader.bind();

        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, activeCallBucket.instanceTransformBuffer);

        m_pBoundMesh = nullptr;
        for(uint32_t j = 0; j < activeCallBucket.callInfos.size(); ++j) {
            if(!activeCallBucket.usageFlags[j]) break;

            const CallInfo& callInfo = activeCallBucket.callInfos[j];
            const CallHeader& header = callInfo.header;

            if (i==0) {
                 activeShader.setUniform("numJoints", (uint32_t) header.pSkeletonDesc->getNumJoints());
            }

            activeShader.setUniform("transformBufferOffset", callInfo.transformBufferOffset);

            if(header.pMesh != m_pBoundMesh) {
                m_pBoundMesh = header.pMesh;
                m_pBoundMesh->bind();
            }

            VKR_DEBUG_CALL(glDrawElementsInstanced(m_pBoundMesh->getDrawType(), m_pBoundMesh->getIndexCount(), GL_UNSIGNED_INT, nullptr, callInfo.numInstances));
        }
    }

    m_motionRenderLayer.unbind();
}

void Renderer::renderTAA() {
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glDisable(GL_BLEND);

    if (m_historyValid) {
        m_fullScreenQuad.bind();

        //m_historyRenderLayer.setTextureAttachment(2, &m_motionBuffer);
        VKR_DEBUG_CALL(m_historyRenderLayer.setEnabledDrawTargets({(unsigned int) ((m_frameCount+1)%2)});
        m_historyRenderLayer.bind();
        glViewport(0, 0, m_viewportWidth, m_viewportHeight);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        m_taaShader.bind();
        m_taaShader.setUniform("currentBuffer", 0);
        m_taaShader.setUniform("historyBuffer", 1);
        m_taaShader.setUniform("motionBuffer", 2);
        //m_taaShader.setUniform("gBufferDepth", 3);
        //m_taaShader.setUniform("lastViewProj", m_lastViewProj);
        //m_taaShader.setUniform("inverseViewProj", m_inverseViewProj);
        //m_gBufferDepthTexture.bind(3);
        m_taaShader.setUniform("jitter", m_viewJitter);
        m_sceneTexture.bind(0);
        m_historyBuffer[m_frameCount%2].bind(1);
        m_motionBuffer.bind(2);
        m_fullScreenQuad.draw();)

        /*if (sharpenTAA) {
            m_sceneRenderLayer.setEnabledDrawTargets({0});
            m_sceneRenderLayer.bind();
            glViewport(0, 0, m_viewportWidth, m_viewportHeight);
            glClear(GL_COLOR_BUFFER_BIT);
            m_sharpenShader.bind();
            m_sharpenShader.setUniform("texSampler", 0);
            m_sharpenShader.setUniform("amount", sharpenAmount);
            m_historyBuffer[(m_frameCount+1)%2].bind(0);
            m_fullScreenQuad.draw();
        } else */
        if (m_parameters.enableMotionBlur) {
            m_sceneRenderLayer.setEnabledDrawTargets({0});
            m_sceneRenderLayer.bind();
            glViewport(0, 0, m_viewportWidth, m_viewportHeight);
            glClear(GL_COLOR_BUFFER_BIT);
            VKR_DEBUG_CALL(m_taaMotionBlurCompositeShader.bind();)
            VKR_DEBUG_CALL(m_taaMotionBlurCompositeShader.setUniform("motionBlurTexture", 0);)
            VKR_DEBUG_CALL(m_taaMotionBlurCompositeShader.setUniform("taaTexture", 1);)
            VKR_DEBUG_CALL(m_taaMotionBlurCompositeShader.setUniform("motionBuffer", 2);)
            VKR_DEBUG_CALL(m_taaMotionBlurCompositeShader.setUniform("jitter", m_viewJitter);)
            m_motionBlurTexture.bind(0);
            m_historyBuffer[(m_frameCount+1)%2].bind(1);
            m_motionBuffer.bind(2);
            VKR_DEBUG_CALL(m_fullScreenQuad.draw();)
        } else {
            m_historyRenderLayer.setEnabledReadTarget((unsigned int) ((m_frameCount+1)%2));
            m_sceneRenderLayer.setEnabledDrawTargets({0});
            m_historyRenderLayer.bind(GL_READ_FRAMEBUFFER);
            m_sceneRenderLayer.bind(GL_DRAW_FRAMEBUFFER);
            glBlitFramebuffer(0, 0, m_viewportWidth, m_viewportHeight, 0, 0, m_viewportWidth, m_viewportHeight, GL_COLOR_BUFFER_BIT, GL_NEAREST);
        }
    } else {
        m_historyRenderLayer.setEnabledDrawTargets({(unsigned int) ((m_frameCount+1)%2)});
        m_sceneRenderLayer.setEnabledReadTarget(0);
        m_historyRenderLayer.bind(GL_DRAW_FRAMEBUFFER);
        m_sceneRenderLayer.bind(GL_READ_FRAMEBUFFER);
        glBlitFramebuffer(0, 0, m_viewportWidth, m_viewportHeight, 0, 0, m_viewportWidth, m_viewportHeight, GL_COLOR_BUFFER_BIT, GL_NEAREST);
        m_historyValid = true;
    }

    m_sceneRenderLayer.unbind();
}

void Renderer::renderMotionBlur() {
    glDisable(GL_BLEND);
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);

    m_motionBlurRenderLayer.setEnabledDrawTargets({0});
    m_motionBlurRenderLayer.bind();
    m_motionBlurRenderLayer.validate();
    glViewport(0, 0, m_viewportWidth, m_viewportHeight);
    glClear(GL_COLOR_BUFFER_BIT);

    m_motionBlurShader.bind();
    m_motionBlurShader.setUniform("sceneColor", 0);
    m_motionBlurShader.setUniform("motionBuffer", 1);
    m_motionBlurShader.setUniform("depthBuffer", 2);

    m_sceneTexture.bind(0);
    m_motionBuffer.bind(1);
    m_gBufferDepthTexture.bind(2);

    m_fullScreenQuad.bind();
    VKR_DEBUG_CALL(m_fullScreenQuad.draw();)

    if (!m_parameters.enableTAA) {
        m_motionBlurRenderLayer.setEnabledReadTarget(0);
        m_sceneRenderLayer.setEnabledDrawTargets({0});
        m_motionBlurRenderLayer.bind(GL_READ_FRAMEBUFFER);
        m_sceneRenderLayer.bind(GL_DRAW_FRAMEBUFFER);
        glBlitFramebuffer(0, 0, m_viewportWidth, m_viewportHeight, 0, 0, m_viewportWidth, m_viewportHeight, GL_COLOR_BUFFER_BIT, GL_NEAREST);
    }
}

void Renderer::renderTransparency() {
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glEnable(GL_BLEND);

    // I don't know how to avoid clearing both separately...
    m_transparencyRenderLayer.setEnabledDrawTargets({0});
    m_transparencyRenderLayer.bind();
    glViewport(0, 0, m_viewportWidth, m_viewportHeight);
    glClearColor(0, 0, 0, 0);
    glClear(GL_COLOR_BUFFER_BIT);
    m_transparencyRenderLayer.setEnabledDrawTargets({1});
    m_transparencyRenderLayer.bind();
    glViewport(0, 0, m_viewportWidth, m_viewportHeight);
    glClearColor(1, 0, 0, 0);
    glClear(GL_COLOR_BUFFER_BIT);

    m_transparencyRenderLayer.setEnabledDrawTargets({0, 1});
    m_transparencyRenderLayer.bind();

    glBlendFunci(0, GL_ONE, GL_ONE);
    glBlendFunci(1, GL_ZERO, GL_ONE_MINUS_SRC_COLOR);

    for(int i = 0; i < 2; i++) {
        const Shader& activeShader = (i == 0) ? m_transparencyShaderSkinned : m_transparencyShader;
        const CallBucket& activeCallBucket = (i == 0) ? m_transparencyCallBucketSkinned : m_transparencyCallBucket;
        if (activeCallBucket.numInstances == 0) continue;

        activeShader.bind();
        activeShader.setUniform("diffuseTexture", 0);
        activeShader.setUniform("normalsTexture", 1);
        activeShader.setUniform("emissionTexture", 2);
        activeShader.setUniform("metallicRoughnessTexture", 3);
        activeShader.setUniform("useDiffuseTexture", 0);
        activeShader.setUniform("useNormalsTexture", 0);
        activeShader.setUniform("useEmissionTexture", 0);
        activeShader.setUniform("useMetallicRoughnessTexture", 0);

        activeShader.setUniform("inverseProjection", m_projectionInverse);
        activeShader.setUniform("lightDirection", m_lightDirectionViewSpace);
        activeShader.setUniform("lightIntensity", m_lightIntensity);

        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, activeCallBucket.instanceTransformBuffer);

        m_pBoundMesh = nullptr;
        m_pBoundDiffuseTexture = nullptr;
        m_pBoundNormalsTexture = nullptr;
        m_pBoundEmissionTexture = nullptr;
        m_pBoundMetallicRoughnessTexture = nullptr;
        bool uUseDiffuseTexture = false;
        bool uUseNormalsTexture = false;
        bool uUseEmissionTexture = false;
        bool uUseMetallicRoughnessTexture = false;
        for(uint32_t j = 0; j < activeCallBucket.callInfos.size(); ++j) {
            if(!activeCallBucket.usageFlags[j]) break;

            const CallInfo& callInfo = activeCallBucket.callInfos[j];
            const CallHeader& header = callInfo.header;

            if (i==0) {
                 activeShader.setUniform("numJoints", (uint32_t) header.pSkeletonDesc->getNumJoints());
            }

            activeShader.setUniform("transformBufferOffset", callInfo.transformBufferOffset);

            if(const Material* pMaterial = header.pMaterial) {
                activeShader.setUniform("color", pMaterial->getTintColor());
                activeShader.setUniform("roughness", pMaterial->getRoughness());
                activeShader.setUniform("metallic", pMaterial->getMetallic());
                activeShader.setUniform("emission", pMaterial->getEmissionIntensity());
                activeShader.setUniform("alpha", pMaterial->getAlpha());

                if(pMaterial->getDiffuseTexture() != nullptr) {
                    if(!uUseDiffuseTexture) {
                        activeShader.setUniform("useDiffuseTexture", 1);
                        uUseDiffuseTexture = true;
                    }
                    if(pMaterial->getDiffuseTexture() != m_pBoundDiffuseTexture) {
                        pMaterial->getDiffuseTexture()->bind(0);
                        m_pBoundDiffuseTexture = pMaterial->getDiffuseTexture();
                    }
                } else if(uUseDiffuseTexture){
                    activeShader.setUniform("useDiffuseTexture", 0);
                    uUseDiffuseTexture = false;
                }

                if(pMaterial->getNormalsTexture() != nullptr) {
                    if(!uUseNormalsTexture) {
                        activeShader.setUniform("useNormalsTexture", 1);
                        uUseNormalsTexture = true;
                    }
                    if(pMaterial->getNormalsTexture() != m_pBoundNormalsTexture) {
                        pMaterial->getNormalsTexture()->bind(1);
                        m_pBoundNormalsTexture = pMaterial->getNormalsTexture();
                    }
                } else if(uUseNormalsTexture) {
                    activeShader.setUniform("useNormalsTexture", 0);
                    uUseNormalsTexture = false;
                }

                if(pMaterial->getEmissionTexture() != nullptr) {
                    if(!uUseEmissionTexture) {
                        activeShader.setUniform("useEmissionTexture", 1);
                        uUseEmissionTexture = true;
                    }
                    if(pMaterial->getEmissionTexture() != m_pBoundEmissionTexture) {
                        pMaterial->getEmissionTexture()->bind(2);
                        m_pBoundEmissionTexture = pMaterial->getEmissionTexture();
                    }
                } else if(uUseEmissionTexture) {
                    activeShader.setUniform("useEmissionTexture", 0);
                    uUseEmissionTexture = false;
                }

                if(pMaterial->getMetallicRoughnessTexture() != nullptr) {
                    if(!uUseMetallicRoughnessTexture) {
                        activeShader.setUniform("useMetallicRoughnessTexture", 1);
                        uUseMetallicRoughnessTexture = true;
                    }
                    if(pMaterial->getMetallicRoughnessTexture() != m_pBoundMetallicRoughnessTexture) {
                        pMaterial->getMetallicRoughnessTexture()->bind(3);
                        m_pBoundMetallicRoughnessTexture = pMaterial->getMetallicRoughnessTexture();
                    }
                } else if(uUseMetallicRoughnessTexture) {
                    activeShader.setUniform("useMetallicRoughnessTexture", 0);
                    uUseMetallicRoughnessTexture = false;
                }
            } else {
                if(uUseDiffuseTexture){
                    activeShader.setUniform("useDiffuseTexture", 0);
                    uUseDiffuseTexture = false;
                }
                if(uUseNormalsTexture) {
                    activeShader.setUniform("useNormalsTexture", 0);
                    uUseNormalsTexture = false;
                }
                if(uUseEmissionTexture) {
                    activeShader.setUniform("useEmissionTexture", 0);
                    uUseEmissionTexture = false;
                }
                if(uUseMetallicRoughnessTexture) {
                    activeShader.setUniform("useMetallicRoughnessTexture", 0);
                    uUseMetallicRoughnessTexture = false;
                }
                activeShader.setUniform("color", glm::vec3(1.0, 0.0, 1.0));
                activeShader.setUniform("metallic", 0.0f);
                activeShader.setUniform("roughness", 1.0f);
                activeShader.setUniform("emission", glm::vec3(0.0));
            }

            if(header.pMesh != m_pBoundMesh) {
                m_pBoundMesh = header.pMesh;
                m_pBoundMesh->bind();
            }

            VKR_DEBUG_CALL(glDrawElementsInstanced(m_pBoundMesh->getDrawType(), m_pBoundMesh->getIndexCount(), GL_UNSIGNED_INT, nullptr, callInfo.numInstances));
        }
    }

    m_sceneRenderLayer.setEnabledDrawTargets({0});
    m_sceneRenderLayer.bind();

    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    m_transparencyCompositeShader.bind();
    m_transparencyCompositeShader.setUniform("accumTexture", 0);
    m_transparencyCompositeShader.setUniform("revealageTexture",1);
    m_transparencyAccumTexture.bind(0);
    m_transparencyRevealageTexture.bind(1);

    m_fullScreenQuad.bind();
    m_fullScreenQuad.draw();
}

// Init blocks for various modules
// Split up into logical blocks more or less corresponding to
// how I might divide this work into separate classes
// e.g. DeferredRenderer, ShadowMapRenderer, SSAORenderer or something
// I don't like how monolithic this Renderer class is, but it is effective

void Renderer::initDeferredPass() {
    if(m_parameters.enableMSAA) {
        m_deferredAmbientEmissionShader.linkShaderFiles("shaders/vertex_fs.glsl", "shaders/fragment_deferred_ae.glsl", "", "#version 400\n#define ENABLE_MSAA\n");
        m_deferredDirectionalLightShader.linkShaderFiles("shaders/vertex_fs.glsl", "shaders/fragment_deferred.glsl", "", "#version 400\n#define ENABLE_MSAA\n");
        m_deferredPointLightShader.linkShaderFiles("shaders/vertex_pt.glsl", "shaders/fragment_deferred_pl.glsl", "", "#version 400\n#define ENABLE_MSAA\n");
        m_deferredPointLightShaderShadow.linkShaderFiles("shaders/vertex_pt.glsl", "shaders/fragment_deferred_pl.glsl", "", "#version 400\n#define ENABLE_MSAA\n#define ENABLE_SHADOW\n");
    } else {
        m_deferredAmbientEmissionShader.linkShaderFiles("shaders/vertex_fs.glsl", "shaders/fragment_deferred_ae.glsl", "", "#version 400\n");
        m_deferredDirectionalLightShader.linkShaderFiles("shaders/vertex_fs.glsl", "shaders/fragment_deferred.glsl", "", "#version 400\n");
        m_deferredPointLightShader.linkShaderFiles("shaders/vertex_pt.glsl", "shaders/fragment_deferred_pl.glsl", "", "#version 400\n");
        m_deferredPointLightShaderShadow.linkShaderFiles("shaders/vertex_pt.glsl", "shaders/fragment_deferred_pl.glsl", "", "#version 400\n#define ENABLE_SHADOW\n");
    }


    TextureParameters sceneTextureParameters = {};
    sceneTextureParameters.arrayLayers = 1;
    sceneTextureParameters.numComponents = 4;
    sceneTextureParameters.useFloatComponents = true;  // needed for HDR rendering
    sceneTextureParameters.useLinearFiltering = true;
    sceneTextureParameters.useEdgeClamping = true;
    m_sceneTexture.setParameters(sceneTextureParameters);
    m_sceneRenderLayer.setTextureAttachment(0, &m_sceneTexture);

    RenderBufferParameters sceneRBParameters = {};
    sceneRBParameters.hasStencilComponent = true;
    sceneRBParameters.useFloat32 = true;
    sceneRBParameters.samples = 1;
    m_sceneDepthStencilRenderBuffer.setParameters(sceneRBParameters);


}

void Renderer::initBackground() {
    m_backgroundShader.linkShaderFiles("shaders/vertex_fs.glsl", "shaders/fragment_background.glsl");
    m_cloudsAccumShader.linkShaderFiles("shaders/vertex_fs.glsl", "shaders/fragment_clouds_accum.glsl");

    TextureParameters cloudsTextureParameters = {};
    cloudsTextureParameters.numComponents = 4;
    cloudsTextureParameters.useFloatComponents = true;
    cloudsTextureParameters.useLinearFiltering = true;
    cloudsTextureParameters.useEdgeClamping = true;
    m_cloudsRenderTexture.setParameters(cloudsTextureParameters);
    cloudsTextureParameters.useLinearFiltering = true;
    m_cloudsHistoryTexture[0].setParameters(cloudsTextureParameters);
    m_cloudsHistoryTexture[1].setParameters(cloudsTextureParameters);

    createCloudTexture();
}

void Renderer::initBloom() {
    m_hdrExtractShader.linkShaderFiles("shaders/vertex_fs.glsl", "shaders/fragment_hdr_extract.glsl");

    TextureParameters bloomTextureParameters = {};
    bloomTextureParameters.arrayLayers = 1;
    bloomTextureParameters.numComponents = 4;
    bloomTextureParameters.useFloatComponents = true;
    bloomTextureParameters.useLinearFiltering = true;
    bloomTextureParameters.useEdgeClamping = true;

    if(m_bloomTextures.size() < 2*m_parameters.numBloomLevels)
        m_bloomTextures.resize(2*m_parameters.numBloomLevels, nullptr);

    if(m_isViewportInitialized) {
        bloomTextureParameters.width = m_viewportWidth;
        bloomTextureParameters.height = m_viewportHeight;
    }

    for(uint32_t i = 0; i < 2*m_parameters.numBloomLevels; ++i) {
        if(m_bloomTextures[i] == nullptr) m_bloomTextures[i] = new Texture();
        m_bloomTextures[i]->setParameters(bloomTextureParameters);
        if(m_isViewportInitialized) {
            m_bloomTextures[i]->allocateData(nullptr);
            if(i%2 == 1) {
                if(bloomTextureParameters.width > 1) bloomTextureParameters.width /= 2;
                if(bloomTextureParameters.height > 1) bloomTextureParameters.height /= 2;
            }
        }
    }
}

void Renderer::initMotionVectors() {
    m_cameraMotionShader.linkShaderFiles("shaders/vertex_fs.glsl", "shaders/fragment_motion_c.glsl");
    m_backgroundMotionShader.linkShaderFiles("shaders/vertex_motion_fs.glsl", "shaders/fragment_motion_fs.glsl");
    m_objectMotionShader.linkShaderFiles("shaders/vertex_motion_o.glsl", "shaders/fragment_motion_o.glsl");
    m_objectMotionShaderSkinned.linkShaderFiles("shaders/vertex_motion_o_skin.glsl", "shaders/fragment_motion_o.glsl");

    TextureParameters motionTexParams = {};
    motionTexParams.numComponents = 2;
    motionTexParams.useEdgeClamping = true;
    motionTexParams.useFloatComponents = true;
    motionTexParams.useLinearFiltering = true;
    motionTexParams.bitsPerComponent = 16;

    RenderBufferParameters depthBufParam = {};

    if (m_isViewportInitialized) {
        motionTexParams.width = m_viewportWidth;
        motionTexParams.height = m_viewportHeight;
        depthBufParam.width = m_viewportWidth;
        depthBufParam.height = m_viewportHeight;
    }

    m_motionBuffer.setParameters(motionTexParams);
    m_motionVectorsDepthBuffer.setParameters(depthBufParam);

    if (m_isViewportInitialized) {
        m_motionBuffer.allocateData(nullptr);
        m_motionVectorsDepthBuffer.allocateData();
    }

    m_motionRenderLayer.setTextureAttachment(0, &m_motionBuffer);
    m_motionRenderLayer.setRenderBufferAttachment(&m_motionVectorsDepthBuffer);
}

// https://en.wikipedia.org/wiki/Halton_sequence
void halton(int base, int n, float* out) {
    int num = 0, den = 1;
    for (int i = 0; i < n; ++i) {
        int x = den - num;
        if (x == 1) {
            num = 1;
            den *= base;
        } else {
            int y = den / base;
            while (x <= y) y /= base;
            num = (base + 1) * y - x;
        }
        out[i] = (float) num / (float) den;
    }
}

void Renderer::initTAA() {
    m_taaShader.linkShaderFiles("shaders/vertex_fs.glsl", "shaders/fragment_taa.glsl");
    m_sharpenShader.linkShaderFiles("shaders/vertex_fs.glsl", "shaders/fragment_sharpen.glsl");
    m_taaMotionBlurCompositeShader.linkShaderFiles("shaders/vertex_fs.glsl", "shaders/fragment_composite_motion_taa.glsl");

    TextureParameters historyTexParams = {};
    historyTexParams.numComponents = 4;
    historyTexParams.useEdgeClamping = true;
    historyTexParams.useFloatComponents = true;
    historyTexParams.useLinearFiltering = true;


    if (m_isViewportInitialized) {
        historyTexParams.width = m_viewportWidth;
        historyTexParams.height = m_viewportHeight;
    }

    m_historyBuffer[0].setParameters(historyTexParams);
    m_historyBuffer[1].setParameters(historyTexParams);

    if (m_isViewportInitialized) {
        m_historyBuffer[0].allocateData(nullptr);
        m_historyBuffer[1].allocateData(nullptr);
    }

    m_historyRenderLayer.setTextureAttachment(0, &m_historyBuffer[0]);
    m_historyRenderLayer.setTextureAttachment(1, &m_historyBuffer[1]);

    /*m_nJitterSamples = 5;
    m_jitterSamples = {{0.0, 0.25}, {0.25, 0.0}, {0.0, 0.0}, {0.0, -0.25}, {-0.25, 0.0}};*/
    m_nJitterSamples = 7;

    float samplesX[7], samplesY[7];
    halton(2, 7, samplesX);
    halton(3, 7, samplesY);

    m_jitterSamples.resize(7);
    for (int i = 0; i < 7; ++i) m_jitterSamples[i] = glm::vec2(samplesX[i] - 0.5, samplesY[i] - 0.5);
}

void Renderer::initMotionBlur() {
    m_motionBlurShader.linkShaderFiles("shaders/vertex_fs.glsl", "shaders/fragment_motion_blur.glsl");

    TextureParameters param = {};
    param.numComponents = 4;
    param.useFloatComponents = true; // HDR
    param.useLinearFiltering = true; // Jitter
    param.useEdgeClamping = true;  // Duh

    if (m_isViewportInitialized) {
        param.width = m_viewportWidth;
        param.height = m_viewportHeight;
    }

    m_motionBlurTexture.setParameters(param);

    if (m_isViewportInitialized) m_motionBlurTexture.allocateData(nullptr);
}

void Renderer::initTransparency() {
    m_transparencyShader.linkShaderFiles("shaders/vertex.glsl", "shaders/fragment_transparency.glsl");
    m_transparencyShaderSkinned.linkShaderFiles("shaders/vertex_skin.glsl", "shaders/fragment_transparency.glsl");
    m_transparencyCompositeShader.linkShaderFiles("shaders/vertex_fs.glsl", "shaders/fragment_composite_transparency.glsl");

    TextureParameters accumTexParam = {};
    accumTexParam.bitsPerComponent = 16;
    accumTexParam.numComponents = 4;
    accumTexParam.useFloatComponents = true;

    TextureParameters revealageTexParam = {};
    revealageTexParam.bitsPerComponent = 8;
    revealageTexParam.numComponents = 1;

    if (m_isViewportInitialized) {
        accumTexParam.width  = revealageTexParam.width  = m_viewportWidth;
        accumTexParam.height = revealageTexParam.height = m_viewportHeight;
    }

    m_transparencyAccumTexture.setParameters(accumTexParam);
    m_transparencyRevealageTexture.setParameters(revealageTexParam);

    if (m_isViewportInitialized) {
        m_transparencyAccumTexture.allocateData(nullptr);
        m_transparencyRevealageTexture.allocateData(nullptr);
    }
}

void Renderer::initFullScreenQuad() {
    float fsqData[] = {-1, -1, 1,  1, -1, 1,  -1, 1, 1,  1, 1, 1,  // vertices
    0, 0, 1, 0, 0, 1, 1, 1};  // texcoords
    m_fullScreenQuad.setVertexCount(4);
    m_fullScreenQuad.setDrawType(GL_TRIANGLE_STRIP);
    m_fullScreenQuad.createVertexAttribute(0, 3);
    m_fullScreenQuad.createVertexAttribute(1, 2);
    m_fullScreenQuad.allocateVertexBuffer(fsqData);
}

void Renderer::initGBuffer() {
    m_gBufferShader.linkShaderFiles("shaders/vertex.glsl", "shaders/fragment_gbuffer.glsl");
    m_gBufferShaderSkinned.linkShaderFiles("shaders/vertex_skin.glsl", "shaders/fragment_gbuffer.glsl");

    /*TextureParameters gBufferPositionTextureParameters = {};
    gBufferPositionTextureParameters.arrayLayers = 1;
    gBufferPositionTextureParameters.numComponents = 4;
    if(m_parameters.enableMSAA) {
        gBufferPositionTextureParameters.samples = 4;
    } else {
        gBufferPositionTextureParameters.samples = 1;
    }
    gBufferPositionTextureParameters.useEdgeClamping = true;
    gBufferPositionTextureParameters.useFloatComponents = true;
    gBufferPositionTextureParameters.bitsPerComponent = 32;

    m_gBufferPositionViewSpaceTexture.setParameters(gBufferPositionTextureParameters);*/

    TextureParameters gBufferNormalTextureParameters = {};
    gBufferNormalTextureParameters.arrayLayers = 1;
    gBufferNormalTextureParameters.numComponents = 3;
    gBufferNormalTextureParameters.bitsPerComponent = 16;
    if(m_parameters.enableMSAA) {
        gBufferNormalTextureParameters.samples = 4;
    } else {
        gBufferNormalTextureParameters.samples = 1;
    }
    gBufferNormalTextureParameters.useEdgeClamping = true;
    gBufferNormalTextureParameters.useFloatComponents = false;
    gBufferNormalTextureParameters.bitsPerComponent = 16;

    m_gBufferNormalViewSpaceTexture.setParameters(gBufferNormalTextureParameters);

    TextureParameters gBufferColorTextureParameters = {};
    gBufferColorTextureParameters.arrayLayers = 1;
    gBufferColorTextureParameters.numComponents = 4;
    if(m_parameters.enableMSAA) {
        gBufferColorTextureParameters.samples = 4;
    } else {
        gBufferColorTextureParameters.samples = 1;
    }
    gBufferColorTextureParameters.useEdgeClamping = true;
    gBufferColorTextureParameters.useFloatComponents = false;

    m_gBufferAlbedoMetallicTexture.setParameters(gBufferColorTextureParameters);

    TextureParameters gBufferEmissionTextureParameters = {};
    gBufferEmissionTextureParameters.arrayLayers = 1;
    gBufferEmissionTextureParameters.numComponents = 4;
    if(m_parameters.enableMSAA) {
        gBufferEmissionTextureParameters.samples = 4;
    } else {
        gBufferEmissionTextureParameters.samples = 1;
    }
    gBufferEmissionTextureParameters.useEdgeClamping = true;
    gBufferEmissionTextureParameters.useFloatComponents = true;

    m_gBufferEmissionRoughnessTexture.setParameters(gBufferEmissionTextureParameters);

    /*RenderBufferParameters gBufferDepthRBParameters = {};
    if(m_parameters.enableMSAA) {
        gBufferDepthRBParameters.samples = 4;
    } else {
        gBufferDepthRBParameters.samples = 1;
    }
    gBufferDepthRBParameters.hasStencilComponent = true;
    m_gBufferDepthRenderBuffer.setParameters(gBufferDepthRBParameters);*/

    TextureParameters gBufferDepthTexParams = {};
    gBufferDepthTexParams.useFloatComponents = true;
    gBufferDepthTexParams.bitsPerComponent = 32;
    gBufferDepthTexParams.useDepthComponent = true;
    gBufferDepthTexParams.useStencilComponent = true;
    gBufferDepthTexParams.useEdgeClamping = true;
    m_gBufferDepthTexture.setParameters(gBufferDepthTexParams);

    /*m_gBufferRenderLayer.setTextureAttachment(0, &m_gBufferPositionViewSpaceTexture);
    m_gBufferRenderLayer.setTextureAttachment(1, &m_gBufferNormalViewSpaceTexture);
    m_gBufferRenderLayer.setTextureAttachment(2, &m_gBufferAlbedoMetallicTexture);
    m_gBufferRenderLayer.setTextureAttachment(3, &m_gBufferEmissionRoughnessTexture);
    //m_gBufferRenderLayer.setTextureAttachment(4, &m_gBufferDepthTexture);*/
}

void Renderer::initShadowMap() {
    m_depthOnlyShader.linkVertexShader("shaders/vertex_depth.glsl");
    m_depthOnlyShaderSkinned.linkVertexShader("shaders/vertex_depth_skin.glsl");
    m_depthToVarianceShader.linkShaderFiles("shaders/vertex_fs.glsl", "shaders/fragment_evsm.glsl");

    m_shadowMapCascadeMatrices.resize(m_parameters.shadowMapNumCascades);
    m_shadowMapCascadeSplitDepths.resize(m_parameters.shadowMapNumCascades);
    m_shadowMapCascadeBlurRanges.resize(m_parameters.shadowMapNumCascades);

    TextureParameters depthTextureParameters = {};
    depthTextureParameters.arrayLayers = m_parameters.shadowMapNumCascades;
    depthTextureParameters.useDepthComponent = true;
    depthTextureParameters.useFloatComponents = false;
    depthTextureParameters.useFloatComponents = true;
    depthTextureParameters.bitsPerComponent = 32;
    depthTextureParameters.useEdgeClamping = true;
    depthTextureParameters.useLinearFiltering = true;
    depthTextureParameters.width = m_parameters.shadowMapResolution;
    depthTextureParameters.height = m_parameters.shadowMapResolution;
    m_lightDepthTexture.setParameters(depthTextureParameters);
    m_lightDepthTexture.allocateData(nullptr);

    TextureParameters shadowMapArrayTextureParameters = {};
    shadowMapArrayTextureParameters.numComponents = 4;
    shadowMapArrayTextureParameters.arrayLayers = m_parameters.shadowMapNumCascades;
    shadowMapArrayTextureParameters.width = m_parameters.shadowMapResolution;
    shadowMapArrayTextureParameters.height = m_parameters.shadowMapResolution;
    shadowMapArrayTextureParameters.useEdgeClamping = true;
    shadowMapArrayTextureParameters.useFloatComponents = true;
    shadowMapArrayTextureParameters.bitsPerComponent = 32;
    shadowMapArrayTextureParameters.useLinearFiltering = true;
    shadowMapArrayTextureParameters.useMipmapFiltering = false;
    shadowMapArrayTextureParameters.useAnisotropicFiltering = false;  // mipmapping and anisotropic I couldn't get to work in the deferred renderer :(
    m_shadowMapArrayTexture.setParameters(shadowMapArrayTextureParameters);
    m_shadowMapArrayTexture.allocateData(nullptr);

    for(uint32_t i = 0; i < m_parameters.shadowMapNumCascades; ++i) {
        m_shadowMapRenderLayer.setTextureAttachment(i, &m_shadowMapArrayTexture, i);
    }

    TextureParameters shadowMapFilterTextureParameters = {};
    shadowMapFilterTextureParameters.numComponents = 4;
    shadowMapFilterTextureParameters.arrayLayers = 1;
    shadowMapFilterTextureParameters.width = m_parameters.shadowMapFilterResolution;
    shadowMapFilterTextureParameters.height = m_parameters.shadowMapFilterResolution;
    shadowMapFilterTextureParameters.useEdgeClamping = true;
    shadowMapFilterTextureParameters.useFloatComponents = true;
    shadowMapFilterTextureParameters.bitsPerComponent = 32;
    shadowMapFilterTextureParameters.useLinearFiltering = true;
    shadowMapFilterTextureParameters.useMipmapFiltering = false;
    m_shadowMapFilterTextures[0].setParameters(shadowMapFilterTextureParameters);
    m_shadowMapFilterTextures[1].setParameters(shadowMapFilterTextureParameters);
    m_shadowMapFilterTextures[0].allocateData(nullptr);
    m_shadowMapFilterTextures[1].allocateData(nullptr);
    m_shadowMapFilterRenderLayer.setTextureAttachment(0, &m_shadowMapFilterTextures[0]);
    m_shadowMapFilterRenderLayer.setTextureAttachment(1, &m_shadowMapFilterTextures[1]);

    // Create call buckets (and per-bucket objects) for each shadow map cascade level
    m_shadowMapCallBuckets.resize(m_parameters.shadowMapNumCascades);
    m_skinnedShadowMapCallBuckets.resize(m_parameters.shadowMapNumCascades);

    m_shadowMapFrustumCullers.resize(m_parameters.shadowMapNumCascades);
    m_shadowMapListBuilders.resize(m_parameters.shadowMapNumCascades);

    m_fillShadowMapCallBucketParams.resize(m_parameters.shadowMapNumCascades);
    m_fillSkinnedShadowMapCallBucketParams.resize(m_parameters.shadowMapNumCascades);

    m_buildShadowMapListsParams.resize(m_parameters.shadowMapNumCascades);
}

void Renderer::initPointLightShadowMaps() {
    m_pointLightVarianceShader.linkShaderFiles("shaders/vertex_fs.glsl", "shaders/fragment_evsm_cube.glsl");
    //m_pointLightShadowMapShader.linkVertexGeometry("shaders/vertex_depth.glsl", "shaders/geometry_cubemap.glsl");
    //m_pointLightShadowMapShaderSkinned.linkVertexGeometry("shaders/vertex_depth_skin.glsl", "shaders/geometry_cubemap.glsl");

    m_pointLightShadowMaps.assign(m_parameters.maxPointLightShadowMaps, nullptr);

    m_pointLightShadowDepthMaps.assign(m_parameters.maxPointLightShadowMaps, nullptr);

    m_pointLightShadowMapLightIndices.assign(m_parameters.maxPointLightShadowMaps, 0);

    m_pointLightShadowMapLightKeys.assign(m_parameters.maxPointLightShadowMaps, 0.0f);

    size_t numPointLightShadowCallBuckets = 6 * m_parameters.maxPointLightShadowMaps;

    m_pointLightShadowCallBuckets.resize(numPointLightShadowCallBuckets);
    m_pointLightShadowCallBucketsSkinned.resize(numPointLightShadowCallBuckets);

    m_pointLightShadowMapMatrices.resize(numPointLightShadowCallBuckets);

    m_pointShadowFrustumCullers.resize(numPointLightShadowCallBuckets);
    m_pointShadowListBuilders.resize(numPointLightShadowCallBuckets);

    m_fillPointShadowCallBucketParams.resize(numPointLightShadowCallBuckets);
    m_fillSkinnedPointShadowCallBucketParams.resize(numPointLightShadowCallBuckets);

    m_buildPointShadowMapListsParams.resize(numPointLightShadowCallBuckets);

    for (uint32_t i = 0; i < m_parameters.maxPointLightShadowMaps; ++i) {
        addPointLightShadowMap(i);
    }

//    m_numPointLightShadowMaps = 0;
    m_inUsePointLightShadowMaps = 0;
}

void Renderer::addPointLightShadowMap(uint32_t index) {
    if(index >= m_parameters.maxPointLightShadowMaps) return;
    if(m_parameters.maxPointLightShadowMaps < 0) {
        m_pointLightShadowDepthMaps.push_back(new Texture());
        m_pointLightShadowMaps.push_back(new Texture());
        m_pointLightShadowMapLightIndices.push_back(0);
        m_pointLightShadowMapLightKeys.push_back(0.0f);
        m_pointLightShadowMapMatrices.insert(m_pointLightShadowMapMatrices.end(), 6, glm::mat4());
    }

    TextureParameters parameters = {};
    parameters.cubemap = true;
    parameters.useDepthComponent = true;
    parameters.useEdgeClamping = true;
    parameters.width = m_parameters.pointLightShadowMapResolution;
    parameters.height = m_parameters.pointLightShadowMapResolution;

    m_pointLightShadowDepthMaps[index] = new Texture();
VKR_DEBUG_CALL(    m_pointLightShadowDepthMaps[index]->setParameters(parameters); )
VKR_DEBUG_CALL(    m_pointLightShadowDepthMaps[index]->allocateData(nullptr); )

    parameters.useFloatComponents = true;
    parameters.bitsPerComponent = 32;
    parameters.useDepthComponent = false;
    parameters.useLinearFiltering = true;
    parameters.numComponents = 4;

    m_pointLightShadowMaps[index] = new Texture();
VKR_DEBUG_CALL(    m_pointLightShadowMaps[index]->setParameters(parameters); )
VKR_DEBUG_CALL(    m_pointLightShadowMaps[index]->allocateData(nullptr); )

}

void Renderer::initSSAO() {
    if(m_parameters.enableMSAA) {
        m_ssaoShader.linkShaderFiles("shaders/vertex_fs.glsl", "shaders/fragment_ssao.glsl", "", "#version 400\n#define ENABLE_MSAA\n");
    } else {
        m_ssaoShader.linkShaderFiles("shaders/vertex_fs.glsl", "shaders/fragment_ssao.glsl", "", "#version 400\n");
    }

    // kernel generation, hemisphere oriented toward positive Z, weighted more closely to center
    m_ssaoKernel.resize(64);
    for(size_t i = 0; i < m_ssaoKernel.size(); ++i) {
        glm::vec3 sample = glm::ballRand(1.0);  // random point in unit sphere
        sample.z = glm::abs(sample.z);          // flip orientation to hemisphere
        sample *= glm::mix(0.1, 1.0, (float) i / (float) m_ssaoKernel.size());  // weight towards center as i increases
        m_ssaoKernel[i] = sample;
    }

    // Generate random noise texture used to orient tangent, thereby rotating the kernel
    int rotationTextureSize = 4;
    std::vector<glm::vec3> ssaoRandomRotationVectors(rotationTextureSize*rotationTextureSize);
    for(size_t i = 0; i < ssaoRandomRotationVectors.size(); ++i) {
        ssaoRandomRotationVectors[i] = glm::vec3(glm::circularRand(1.0), 0.0);
    }
    TextureParameters ssaoNoiseTextureParameters = {};
    ssaoNoiseTextureParameters.arrayLayers = 1;
    ssaoNoiseTextureParameters.numComponents = 3;
    ssaoNoiseTextureParameters.useFloatComponents = true;
    ssaoNoiseTextureParameters.width = rotationTextureSize;
    ssaoNoiseTextureParameters.height = rotationTextureSize;
    ssaoNoiseTextureParameters.useEdgeClamping = false;
    m_ssaoNoiseTexture.setParameters(ssaoNoiseTextureParameters);
    m_ssaoNoiseTexture.allocateData(ssaoRandomRotationVectors.data());

    // setup SSAO render target
    TextureParameters ssaoRenderTextureParameters = {};
    ssaoRenderTextureParameters.arrayLayers = 1;
    ssaoRenderTextureParameters.numComponents = 1;
    ssaoRenderTextureParameters.useFloatComponents = false;
    ssaoRenderTextureParameters.useEdgeClamping = true;
    ssaoRenderTextureParameters.useLinearFiltering = true;
    m_ssaoTargetTexture.setParameters(ssaoRenderTextureParameters);
    m_ssaoRenderLayer.setTextureAttachment(0, &m_ssaoTargetTexture);

    m_ssaoFilterTexture.setParameters(ssaoRenderTextureParameters);
    m_ssaoFilterRenderLayer.setTextureAttachment(0, &m_ssaoFilterTexture);
}

void fbm(int widthX, int widthY, int widthZ, int octaves, float scale, float tileBlend, bool tileX, bool tileY, bool tileZ, float* noiseOut) {
    float influence = 1.0f;
    glm::vec3 width(widthX, widthY, widthZ);
    glm::vec3 blendWidth = tileBlend * width;
    glm::vec3 blendStart = width - blendWidth;
    for (int o = 0; o < octaves; ++o) {
        for (int i = 0; i < widthX; ++i) for (int j = 0; j < widthY; ++j) for (int k = 0; k < widthZ; ++k) {
            glm::vec3 pos(i, j, k);
            float noise = glm::simplex(scale * pos);
            //noiseOut[(i*widthY + j)*widthZ+k] += influence * glm::simplex(scale * glm::vec3(i, j, k));
            glm::bvec3 cmp = glm::greaterThan(pos, blendWidth) && glm::bvec3(tileX, tileY, tileZ);
            if (glm::any(cmp)) {
                glm::vec3 pos1 = pos - glm::vec3(cmp) * width;
                glm::vec3 interp = glm::max((pos - blendStart)/blendWidth, glm::vec3(0));

                if (cmp.x) {
                    noise = glm::mix(noise, glm::simplex(scale * glm::vec3(pos1.x, pos.y, pos.z)), interp.x);
                }
                if (cmp.y) {
                    float n1 = glm::simplex(scale * glm::vec3(pos.x, pos1.y, pos.z));
                    if (cmp.x) {
                        n1 = glm::mix(n1, glm::simplex(scale * glm::vec3(pos1.x, pos1.y, pos.z)), interp.x);
                    }
                    noise = glm::mix(noise, n1, interp.y);
                }
                if (cmp.z) {
                    float n1 = glm::simplex(scale * glm::vec3(pos.x, pos.y, pos1.z));
                    if (cmp.x) {
                        n1 = glm::mix(n1, glm::simplex(scale * glm::vec3(pos1.x, pos.y, pos1.z)), interp.x);
                    }
                    if (cmp.y) {
                        float n2 = glm::simplex(scale * glm::vec3(pos.x, pos1.y, pos1.z));
                        if (cmp.x) {
                            n2 = glm::mix(n2, glm::simplex(scale * glm::vec3(pos1.x, pos1.y, pos1.z)), interp.x);
                        }
                        n1 = glm::mix(n1, n2, interp.y);
                    }
                    noise = glm::mix(noise, n1, interp.z);
                }
            }
            noiseOut[(i*widthY + j)*widthZ+k] += influence * noise;
        }
        influence *= 0.5f;
        scale *= 2.0f;
    }

    float maxval = 0.0;
    for (int i = 0; i < widthX*widthY*widthZ; ++i) maxval = std::max(std::abs(noiseOut[i]), maxval);
    if (maxval > 1.0f) {
        for (int i = 0; i < widthX*widthY*widthZ; ++i) noiseOut[i] /= maxval;
    }
}

void worley(int widthX, int widthY, int widthZ, int cellsX, int cellsY, int cellsZ, float* noiseOut) {
    std::vector<glm::vec3> cellPoints(cellsX*cellsY*cellsZ);
    glm::vec3 cellDimensions = glm::vec3(widthX, widthY, widthZ) / glm::vec3(cellsX, cellsY, cellsZ);
    for (int i = 0; i < cellsX; ++i) for (int j = 0; j < cellsY; ++j) for (int k = 0; k < cellsZ; ++k) {
        cellPoints[(i*cellsY + j)*cellsZ + k] = glm::vec3(i, j, k) * cellDimensions + glm::linearRand(glm::vec3(0.0), cellDimensions);
    }
    glm::ivec3 iCells(cellsX, cellsY, cellsZ);
    glm::vec3 width(widthX, widthY, widthZ);
    for (int i = 0; i < widthX; ++i) for (int j = 0; j < widthY; ++j) for (int k = 0; k < widthZ; ++k) {
        float sqdist = 0.0f;
        glm::ivec3 cell = glm::ivec3(glm::vec3(i, j, k) / cellDimensions);

        for (int ii = -1; ii <= 1; ++ii) for (int jj = -1; jj <= 1; ++jj) for (int kk = -1; kk <= 1; ++kk) {
            glm::ivec3 ccell = cell + glm::ivec3(ii, jj, kk);

            // wrap at borders to create seamless texture
            glm::ivec3 cmp = glm::ivec3(glm::greaterThanEqual(ccell, iCells)) - glm::ivec3(glm::lessThan(ccell, glm::ivec3(0)));
            ccell -= cmp * iCells;
            glm::vec3 offset = glm::vec3(cmp) * width;

            glm::vec3 v = (cellPoints[(ccell.x*cellsY + ccell.y)*cellsZ + ccell.z] + offset - glm::vec3(i, j, k)) / (cellDimensions);
            float d2 = glm::dot(v, v);
            if ((ii == -1 && jj == -1 && kk == -1) || d2 < sqdist) {
                sqdist = d2;
            }
        }
        noiseOut[(i*widthY + j)*widthZ + k] = glm::sqrt(sqdist);
    }
}

void worley(int widthX, int widthY, int cellsX, int cellsY, float* noiseOut) {
    std::vector<glm::vec2> cellPoints(cellsX*cellsY);
    glm::vec2 cellDimensions = glm::vec2(widthX, widthY) / glm::vec2(cellsX, cellsY);
    for (int i = 0; i < cellsX; ++i) for (int j = 0; j < cellsY; ++j) {
        cellPoints[i*cellsY + j] = glm::vec2(i, j) * cellDimensions + glm::linearRand(glm::vec2(0.0), cellDimensions);
    }
    glm::ivec2 iCells(cellsX, cellsY);
    glm::vec2 width(widthX, widthY);
    for (int i = 0; i < widthX; ++i) for (int j = 0; j < widthY; ++j) {
        float sqdist = 0.0f;
        glm::ivec2 cell = glm::ivec2(glm::vec2(i, j) / cellDimensions);

        // 3x3 neighborhood of the cell, one of these cells has the closest sample point
        for (int ii = -1; ii <= 1; ++ii) for (int jj = -1; jj <= 1; ++jj) {
            glm::ivec2 ccell = cell + glm::ivec2(ii, jj);

            // wrap at borders to create seamless texture
            glm::ivec2 cmp = glm::ivec2(glm::greaterThanEqual(ccell, iCells)) - glm::ivec2(glm::lessThan(ccell, glm::ivec2(0)));
            ccell -= cmp * iCells;
            glm::vec2 offset = glm::vec2(cmp) * width;

            glm::vec2 v = (cellPoints[ccell.x*cellsY + ccell.y] + offset - glm::vec2(i, j)) / (cellDimensions);
            float d2 = glm::dot(v, v);
            if ((ii == -1 && jj == -1) || d2 < sqdist) {
                sqdist = d2;
            }
        }

        // write nearest distance
        noiseOut[i*widthY + j] = glm::sqrt(sqdist);
    }
}

void worley3DCube(int width, int cells, float* noiseOut) {
    return worley(width, width, width, cells, cells, cells, noiseOut);
}

void fbm3DCube(int width, int octaves, float scale, float tileBlend, bool tileXZ, bool tileY, float* noiseOut) {
    return fbm(width, width, width, octaves, scale, tileBlend, tileXZ, tileY, tileXZ, noiseOut);
}

void quantizeFloats(const float* floatsIn, unsigned char* ucharsOut, int n) {
    for (int i = 0; i < n; ++i) ucharsOut[i] = static_cast<unsigned char>(glm::clamp((0.5f + 0.5f * floatsIn[i]) * 255, 0.0f, 255.0f));
}

void interleaveBytes(const unsigned char** pUCharsIn, unsigned char* uCharsOut, int nPerInputBuffer, int nInputBuffers) {
    for (int i = 0; i < nPerInputBuffer; ++i) for (int j = 0; j < nInputBuffers; ++j) {
        uCharsOut[i*nInputBuffers+j] = pUCharsIn[j][i];
    }
}

void Renderer::createCloudTexture() {
    std::vector<float> cloudNoiseBaseF(cloudBaseNoiseWidth*cloudBaseNoiseWidth*cloudBaseNoiseWidth, 0.0);
    std::vector<unsigned char> fbmnoise(cloudNoiseBaseF.size());
    fbm3DCube(cloudBaseNoiseWidth, cloudBaseNoiseOctaves, cloudBaseNoiseFrequency, 0.25, true, false, cloudNoiseBaseF.data());
    quantizeFloats(cloudNoiseBaseF.data(), fbmnoise.data(), (int) cloudNoiseBaseF.size());

    std::vector<float> worlyNoiseF(cloudNoiseBaseF.size());

    worley3DCube(cloudBaseNoiseWidth, cloudBaseWorleyLayerPoints[0], worlyNoiseF.data());
    //std::transform(worlyNoiseF.begin(), worlyNoiseF.end(), worlyNoiseF.begin(), [] (float f) { return 1 - f; });
    std::vector<unsigned char> worley0(worlyNoiseF.size());
    quantizeFloats(worlyNoiseF.data(), worley0.data(), (int) worlyNoiseF.size());

    worley3DCube(cloudBaseNoiseWidth, cloudBaseWorleyLayerPoints[1], worlyNoiseF.data());
    //std::transform(worlyNoiseF.begin(), worlyNoiseF.end(), worlyNoiseF.begin(), [] (float f) { return 1 - f; });
    std::vector<unsigned char> worley1(worlyNoiseF.size());
    quantizeFloats(worlyNoiseF.data(), worley1.data(), (int) worlyNoiseF.size());

    worley3DCube(cloudBaseNoiseWidth, cloudBaseWorleyLayerPoints[2], worlyNoiseF.data());
    //std::transform(worlyNoiseF.begin(), worlyNoiseF.end(), worlyNoiseF.begin(), [] (float f) { return 1 - f; });
    std::vector<unsigned char> worley2(worlyNoiseF.size());
    quantizeFloats(worlyNoiseF.data(), worley2.data(), (int) worlyNoiseF.size());

    const unsigned char* baseNoiseBuffers[] = {fbmnoise.data(), worley0.data(), worley1.data(), worley2.data()};
    std::vector<unsigned char> baseNoise(4*cloudNoiseBaseF.size());
    interleaveBytes(baseNoiseBuffers, baseNoise.data(), (int) cloudNoiseBaseF.size(), 4);


    TextureParameters param = {};
    param.is3D = true;
    param.width = cloudBaseNoiseWidth;
    param.height = cloudBaseNoiseWidth;
    param.arrayLayers = cloudBaseNoiseWidth;
    param.numComponents = 4;
    param.useLinearFiltering = true;

    VKR_DEBUG_CALL(m_cloudNoiseBaseTexture.setParameters(param));
    VKR_DEBUG_CALL(m_cloudNoiseBaseTexture.allocateData(baseNoise.data()));

    int detailNoiseWidth = 128;
    std::vector<float> cloudNoiseDetailF(detailNoiseWidth*detailNoiseWidth*detailNoiseWidth, 0.0f);
    std::vector<unsigned char> cloudNoiseDetail(detailNoiseWidth*detailNoiseWidth*detailNoiseWidth);
    fbm(detailNoiseWidth, detailNoiseWidth, detailNoiseWidth, 1, 0.1f, 0.25, true, false, true, cloudNoiseDetailF.data());
    quantizeFloats(cloudNoiseDetailF.data(), cloudNoiseDetail.data(), detailNoiseWidth*detailNoiseWidth*detailNoiseWidth);

    param.width = detailNoiseWidth;
    param.height = detailNoiseWidth;
    param.arrayLayers = detailNoiseWidth;
    param.numComponents = 1;

    VKR_DEBUG_CALL(m_cloudNoiseDetailTexture.setParameters(param));
    VKR_DEBUG_CALL(m_cloudNoiseDetailTexture.allocateData(cloudNoiseDetail.data()));

    /*std::vector<float> worleyNoiseF(128*128);
    std::vector<float> worleyNoiseF2(128*128);
    std::vector<unsigned char> worleyNoise(worleyNoiseF.size());
    worley(128, 128, 1, 5, 5, 1, worleyNoiseF.data());
    worley(128, 128, 1, 12, 12, 1, worleyNoiseF2.data());
    std::transform(worleyNoiseF.begin(), worleyNoiseF.end(), worleyNoiseF.begin(), [] (float f) { return 1.0f - f; });
    std::transform(worleyNoiseF2.begin(), worleyNoiseF2.end(), worleyNoiseF2.begin(), [] (float f) { return 1.0f - f; });
    std::transform(worleyNoiseF.begin(), worleyNoiseF.end(), worleyNoiseF2.begin(), worleyNoiseF.begin(), [] (float f0, float f1) { return 0.75 * f0 + 0.25 * f1; });
    fbm(128, 128, 1, 1, 1.0/128.0, 0.25, true, true, false, worleyNoiseF.data());
    quantizeFloats(worleyNoiseF.data(), worleyNoise.data(), 128*128);*/
    param.arrayLayers = 1;
    param.numComponents = 4;
    param.width = cloudBaseNoiseWidth;
    param.height = cloudBaseNoiseWidth;
    param.is3D = false;
    m_worleyTexture.setParameters(param);
    m_worleyTexture.allocateData(baseNoise.data());

}

bool operator<(const Renderer::CallHeader& lh, const Renderer::CallHeader& rh) {
    return lh.pMesh < rh.pMesh || (lh.pMesh == rh.pMesh &&
            (lh.pMaterial < rh.pMaterial || (lh.pMaterial == rh.pMaterial &&
            (lh.pSkeletonDesc < rh.pSkeletonDesc))));
}
