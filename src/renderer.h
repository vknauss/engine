#ifndef RENDERER_H_
#define RENDERER_H_

#include <vector>

#include <GL/glew.h>

#include <glm/glm.hpp>

#include "camera.h"
#include "material.h"
#include "mesh.h"
#include "renderable.h"
#include "render_layer.h"
#include "scene.h"
#include "shader.h"
#include "skeleton.h"
#include "texture.h"

#include "job_scheduler.h"

#include "instance_list.h"
#include "instance_list_builder.h"
#include "frustum_culler.h"

#include "window.h"

/**
 *  Renderer class encapsulates as nearly as possible all the frame-to-frame rendering logic
 *  It holds the instances of shaders and render layers as needed
 *  The main interface of VKRender
 **/

struct RendererParameters {
    bool enableShadowFiltering = true;
    bool enableEVSM = true;
    bool enableSSAO = true;
    bool enableMSAA = true;

    bool testToggle = true;

    int numBloomLevels = 3;

    bool useCubemapGeometryShader = false;

    int shadowMapResolution = 1024;
    int shadowMapFilterResolution = 512;
    int shadowMapNumCascades = 4;
    float shadowMapCascadeScale = 0.4f;
    float shadowMapCascadeBlurSize = 0.35f;
    float shadowMapLightBleedCorrectionBias = 0.0f;
    float shadowMapLightBleedCorrectionPower = 1.0f;
    float shadowMapMaxDistance = 50.0f;

    int maxPointLightShadowMaps = 5;  // 0 = disabled, negative = unlimited
    int pointLightShadowMapResolution = 512;
    float pointLightShadowMapNear = 0.2;

    float ambientPower = 2.0f;

    float pointLightMinIntensity = 0.1f;

    float exposure = 1.0f;
    float gamma = 2.2f;

    int maxSkinningBones = 80;
};

class Renderer {

public:

    // The geometry for a particular render call
    // Used to group instance data into buckets
    struct CallHeader {
        const Mesh* pMesh;
        const Material* pMaterial;
        const SkeletonDescription* pSkeletonDesc;
    };

    // Constructor is empty, call init() to initialize
    explicit Renderer(JobScheduler* pScheduler);

    // Destructor is empty, all members are managed statically
    ~Renderer();

    RendererParameters getParameters() const {
        return m_parameters;
    }

    void setParameters(const RendererParameters& parameters);

    // Initialize the resources for rendering: shaders, render layers/targets, etc
    //virtual
    void init();

    // Free resources
    void cleanup();

    // Set size and initialize fullscreen render targets
    // These will NOT be initialized until this method is called, so call it early
    // Does not need to be called every frame unless the viewport is changing
    //virtual
    void setViewport(int width, int height);

    // Call this every frame before render() to load calls
    // or don't if the game is paused or something, then old calls will still be loaded
    //virtual
    //void setupForRender(const Scene* pScene);



    struct RendererJobParam {
        Renderer* pRenderer;
        Scene* pScene;
        Window* pWindow;
        JobScheduler* pScheduler;
    };

    static void dispatchRenderJobs(uintptr_t param);


private:

    // Types:

    // Info for a particular call, pretty straighforward
    // Holds the SSBO with the instance data
    struct CallInfo {
        CallHeader header;

        uint32_t numInstances;
        uint32_t transformBufferOffset;
    };

    // A bucket corresponds to a group of calls that all get rendered in the
    // same way, i.e. to the same layer with the same shader and data types
    struct CallBucket {
        //std::vector<CallInfo*> callInfos;
        std::vector<CallInfo> callInfos;
        std::vector<bool> usageFlags;
        //std::map<CallHeader, uint32_t> headerMap;
        //std::map<const Model*, uint32_t> modelMap;

        std::vector<float> instanceTransformFloats;

        GLuint instanceTransformBuffer;

        uint32_t numInstances = 0;
        //uint32_t numMeshes = 0;

        //uint32_t fcounter = 0;

        bool buffersInitialized = false;


        CallBucket() :
            callInfos(),
            usageFlags(),
            //modelMap(),
            instanceTransformFloats(),
            instanceTransformBuffer(),
            numInstances(0),
            //fcounter(0),
            buffersInitialized(false) {

            //instanceTransformFloats.reserve(512);
        }

    };

    // Member variables: (a lot)

    RendererParameters m_parameters;

    InstanceListBuilder m_defaultListBuilder;
    std::vector<InstanceListBuilder> m_shadowMapListBuilders;
    std::vector<InstanceListBuilder> m_pointShadowListBuilders;

    FrustumCuller m_defaultFrustumCuller;
    FrustumCuller m_pointLightFrustumCuller;
    std::vector<FrustumCuller> m_shadowMapFrustumCullers;
    std::vector<FrustumCuller> m_pointShadowFrustumCullers;

    CallBucket m_defaultCallBucket;
    CallBucket m_skinnedCallBucket;
    std::vector<CallBucket> m_shadowMapCallBuckets;
    std::vector<CallBucket> m_skinnedShadowMapCallBuckets;
    std::vector<CallBucket> m_pointLightShadowCallBuckets;
    std::vector<CallBucket> m_pointLightShadowCallBucketsSkinned;

    Shader m_depthOnlyShader;
    Shader m_depthOnlyShaderSkinned;
    Shader m_depthToVarianceShader;
    Shader m_pointLightShadowMapShader;
    Shader m_pointLightShadowMapShaderSkinned;
    Shader m_pointLightVarianceShader;
    Shader m_fullScreenFilterShader;
    Shader m_horizontalFilterShader;
    Shader m_fullScreenShader;
    Shader m_gBufferShader;
    Shader m_gBufferShaderSkinned;
    Shader m_ssaoShader;
    Shader m_passthroughShader;
    Shader m_deferredAmbientEmissionShader;
    Shader m_deferredDirectionalLightShader;
    Shader m_deferredPointLightShader;
    Shader m_deferredPointLightShaderShadow;
    Shader m_hdrExtractShader;
    Shader m_backgroundShader;

    RenderLayer m_shadowMapRenderLayer;
    RenderLayer m_shadowMapFilterRenderLayer;
    RenderLayer m_pointLightShadowRenderLayer;
    RenderLayer m_sceneRenderLayer;
    RenderLayer m_gBufferRenderLayer;
    RenderLayer m_ssaoRenderLayer;
    RenderLayer m_ssaoFilterRenderLayer;
    RenderLayer m_bloomRenderLayer[2];

    Texture m_lightDepthTexture;
    Texture m_shadowMapArrayTexture;
    Texture m_shadowMapFilterTextures[2];
    Texture m_sceneTexture;
    Texture m_gBufferPositionViewSpaceTexture;
    Texture m_gBufferNormalViewSpaceTexture;
    Texture m_gBufferAlbedoMetallicTexture;
    Texture m_gBufferEmissionRoughnessTexture;
    Texture m_ssaoNoiseTexture;
    Texture m_ssaoTargetTexture;
    Texture m_ssaoFilterTexture;
    std::vector<Texture*> m_bloomTextures;
    std::vector<Texture*> m_pointLightShadowMaps;
    std::vector<Texture*> m_pointLightShadowDepthMaps;

    RenderBuffer m_shadowMapDepthRenderBuffer;
    RenderBuffer m_gBufferDepthRenderBuffer;
    RenderBuffer m_sceneDepthStencilRenderBuffer;

    std::vector<glm::vec3> m_ssaoKernel;

    std::vector<glm::mat4> m_shadowMapProjectionMatrices;
    std::vector<float> m_shadowMapCascadeSplitDepths;
    std::vector<float> m_shadowMapCascadeBlurRanges;

    std::vector<glm::vec3> m_pointLightPositions;
    std::vector<glm::vec3> m_pointLightIntensities;
    std::vector<float> m_pointLightBoundingSphereRadii;
    std::vector<int> m_pointLightShadowMapIndices;

    std::vector<int> m_pointLightShadowMapLightIndices;
    std::vector<float> m_pointLightShadowMapLightKeys;

    std::vector<glm::mat4> m_pointLightShadowMapMatrices;

    std::vector<BoundingSphere> m_pointLightBoundingSpheres;

    int m_numPointLightShadowMaps;
    int m_inUsePointLightShadowMaps;

    glm::mat4 m_cameraViewMatrix;
    glm::mat4 m_cameraProjectionMatrix;
    glm::mat4 m_lightViewMatrix;
    glm::mat4 m_viewToLightMatrix;
    glm::mat4 m_viewInverse;

    glm::vec3 m_lightDirection;
    glm::vec3 m_lightIntensity;
    glm::vec3 m_lightDirectionViewSpace;
    glm::vec3 m_ambientLightIntensity;

    std::vector<glm::mat4> m_skinningMatrices;
    std::vector<glm::mat3> m_skinningMatricesIT;

    Mesh m_fullScreenQuad;
    Mesh m_pointLightSphere;

    JobScheduler* m_pScheduler;

    const Camera* m_pActiveCamera = nullptr;

    const Mesh* m_pBoundMesh = nullptr;

    const Texture* m_pBoundDiffuseTexture = nullptr;
    const Texture* m_pBoundNormalsTexture = nullptr;
    const Texture* m_pBoundEmissionTexture = nullptr;
    const Texture* m_pBoundMetallicRoughnessTexture = nullptr;

    int m_viewportWidth;
    int m_viewportHeight;

    bool m_directionalLightEnabled;
    bool m_isInitialized = false;
    bool m_isViewportInitialized = false;

    // Methods:

    void setActiveCamera(const Camera* camera);
    void setDirectionalLighting(const glm::vec3& direction, const glm::vec3& intensity);

    void addPointLight(const glm::vec3& position, const glm::vec3& intensity, bool shadowMap);
    void clearPointLights();

    void addPointLightShadowMap();

    //void getCallBucketInstances(CallBucket& bucket, std::list<CallHeader>& headers, std::list<std::list<uint32_t>> instanceLists, const std::vector<bool>& cullResults, const Scene* pScene, glm::mat4 frustumMatrix, bool isSkinned);
    //void fillCallBucket(CallBucket& bucket, const std::vector<InstanceList>& instanceLists, size_t numInstances, const Scene* pScene, glm::mat4 globalMatrix, bool useNormalsMatrix);
    //void fillCallBucket(CallBucket& bucket, const std::list<CallHeader>& headers, const std::list<std::list<uint32_t>> instanceLists, const Scene* pScene, glm::mat4 globalMatrix, bool useNormalsMatrix);
    void fillCallBucketInstanceBuffer(CallBucket& bucket);

    void computeMatrices();
    void computeShadowMapMatrices(const glm::vec3& sceneAABBMin, const glm::vec3& sceneAABBMax);
    void computePointLightShadowMatrices();

    // it do what it say
    // Walks through call buckets and calls all the render* subroutines
    // Eventually draws a pretty picture to the main frambuffer
    //virtual
    void render();

    void renderCompositePass();
    void renderDeferredPass();
    void renderGBuffer();
    void renderShadowMap();
    void renderPointLightShadows();
    void renderSSAOMap();
    void renderBloom();

    void filterShadowMap();

    void initDeferredPass();
    void initFullScreenQuad();
    void initGBuffer();
    void initShadowMap();
    void initPointLightShadowMaps();
    void initSSAO();
    void initBloom();


    static int computeFrustumCulling(const glm::mat4& frustumMatrix, const std::vector<Renderable>& renderablesIn, std::vector<uint32_t>& toRenderIndices);
    static int computeFrustumCulling(const glm::mat4& frustumMatrix, const std::vector<Renderable>& renderablesIn, std::vector<bool>& cullResults);
    static int computeSphereCulling(const glm::vec3& position, float radius, const std::vector<Renderable>& renderablesIn, std::vector<uint32_t>& toRenderIndices);
    static int computeSphereCulling(const glm::vec3& position, float radius, const std::vector<Renderable>& renderablesIn, std::vector<bool>& cullResults);


    struct FillCallBucketParam {
        CallBucket* bucket;
        const std::vector<InstanceList>* instanceLists;
        size_t numInstances;
        glm::mat4 globalMatrix;
        bool useNormalsMatrix;
        bool hasSkinningMatrices;
    };

    struct FillInstanceBuffersParam {
        Renderer* pRenderer;
        Window* pWindow;
        std::vector<CallBucket*> pBuckets;
        size_t nBuckets;
    };

    struct BuildInstanceListsParam {
        InstanceListBuilder* pListBuilder;
        FrustumCuller* pCuller;
        const Scene* pScene;
        glm::mat4 frustumMatrix;
        bool filterNonShadowCasters;
    };

    FillCallBucketParam m_fillDefaultCallBucketParam;
    FillCallBucketParam m_fillSkinnedCallBucketParam;
    std::vector<FillCallBucketParam> m_fillShadowMapCallBucketParams;
    std::vector<FillCallBucketParam> m_fillSkinnedShadowMapCallBucketParams;
    std::vector<FillCallBucketParam> m_fillPointShadowCallBucketParams;
    std::vector<FillCallBucketParam> m_fillSkinnedPointShadowCallBucketParams;

    FillInstanceBuffersParam m_fillInstanceBuffersParam;

    BuildInstanceListsParam m_buildDefaultListsParam;
    std::vector<BuildInstanceListsParam> m_buildShadowMapListsParams;
    std::vector<BuildInstanceListsParam> m_buildPointShadowMapListsParams;

    static void renderJob(uintptr_t param);

    static void dispatchListBuilderJobs(uintptr_t param);
    static void dispatchCallBucketJobs(uintptr_t param);

    static void fillCallBucketJob(uintptr_t param);
    static void fillInstanceBuffersJob(uintptr_t param);
    static void buildInstanceListsJob(uintptr_t param);

};

// CallHeaders are used to key into a STL map, so this is needed
bool operator<(const Renderer::CallHeader& lh, const Renderer::CallHeader& rh);

#endif // RENDERER_H_
