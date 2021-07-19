#ifndef RENDERER_H_
#define RENDERER_H_

#include <vector>

#include <GL/glew.h>

#include <glm/glm.hpp>

#include "core/app/window.h"
#include "core/job_scheduler.h"
#include "core/scene/scene.h"

#include "instance_list_builder.h"
#include "frustum_culler.h"
#include "render_layer.h"
#include "shader.h"


/**
 *  Renderer class encapsulates as nearly as possible all the frame-to-frame rendering logic
 *  It holds the instances of shaders and render layers as needed
 *  The main interface of VKRender
 **/

struct RendererParameters {
    bool enableShadows = true;
    bool enableShadowFiltering = true;
    bool enablePointLights = true;
    bool enableBloom = true;
    bool enableEVSM = true;
    bool enableSSAO = true;
    bool enableMSAA = false;
    bool enableTAA = true;
    bool enableMotionBlur = true;
    bool enableBackgroundLayer = true;

    //bool testToggle = true;

    uint32_t numBloomLevels = 3;

    bool useCubemapGeometryShader = false;

    uint32_t shadowMapResolution = 1024;
    uint32_t shadowMapFilterResolution = 512;
    uint32_t shadowMapNumCascades = 4;
    float shadowMapCascadeScale = 0.4f;
    float shadowMapCascadeBlurSize = 0.35f;
    float shadowMapLightBleedCorrectionBias = 0.1f;
    float shadowMapLightBleedCorrectionPower = 1.8f;
    float shadowMapMaxDistance = 50.0f;


    uint32_t maxPointLightShadowMaps = 5;  // 0 = disabled

    uint32_t pointLightShadowMapResolution = 512;
    float pointLightShadowMapNear = 0.2;

    float ambientPower = 2.0f;

    float pointLightMinIntensity = 0.1f;

    float exposure = 1.0f;
    float gamma = 2.2f;

    uint32_t initialWidth = 1280;
    uint32_t initialHeight = 800;
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

    // Initialize the resources for rendering: shaders, render layers/targets, etc
    //virtual
    void init(const RendererParameters& parameters = RendererParameters());

    // Free resources
    void cleanup();

    // Set size and initialize fullscreen render targets
    // These will NOT be initialized until this method is called, so call it early
    // Does not need to be called every frame unless the viewport is changing
    //virtual
    void setViewport(uint32_t width, uint32_t height);

    // Call this every frame before render() to load calls
    // or don't if the game is paused or something, then old calls will still be loaded
    //virtual
    //void setupForRender(const Scene* pScene);


    struct RendererJobParam {
        Renderer* pRenderer;
        const Scene* pScene;
        AppWindow* pWindow;
        JobScheduler* pScheduler;
        JobScheduler::CounterHandle signalCounterHandle;
        void (*pAfterSceneRenderCallback) (void) = nullptr;
    };

    static void preRenderJob(uintptr_t param);

    static void renderJob(uintptr_t param);

    bool jitterOn = true;
    float sharpenAmount = 1.0;
    bool sharpenTAA = false;

    void setTAA(bool onoff) {
        m_parameters.enableTAA = onoff;
    }
    void toggleTAA() {
        m_parameters.enableTAA = !m_parameters.enableTAA;
        m_historyValid = false;
    }
    void toggleBackground() {
        m_parameters.enableBackgroundLayer = !m_parameters.enableBackgroundLayer;
        m_cloudHistoryValid = false;
    }

    float cloudiness = 0.42;
    float cloudDensity = 0.15;
    float cloudExtinction = 0.064;
    float cloudScattering = 0.098;
    float cloudNoiseBaseScale = 0.0000647850;
    float cloudNoiseDetailScale = 0.0000513456;
    float cloudDetailNoiseInfluence = 0.25;
    glm::vec4 cloudBaseNoiseContribution = {1.24, -0.34, -0.19, 0};
    glm::vec4 cloudBaseNoiseThreshold = {0, 0.55, 0.8, 0};
    float cloudBottomHeight = 1500.0;
    float cloudTopHeight = 3600.0;
    int cloudSteps = 64;
    int cloudShadowSteps = 8;
    float cloudShadowStepSize = 100.0;
    float planetRadius = 1e6;
    int cloudBaseNoiseWidth = 128;
    float cloudBaseNoiseFrequency = 0.01;
    int cloudBaseNoiseOctaves = 4;
    glm::ivec3 cloudBaseWorleyLayerPoints = {16, 32, 48};
    bool rebuildCloudTextures = false;
    bool reloadCloudShader = false;

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
        std::vector<CallInfo> callInfos;

        std::vector<bool> usageFlags;

        std::vector<float> instanceTransformFloats;

        GLuint instanceTransformBuffer;

        uint32_t numInstances = 0;

        bool buffersInitialized = false;

        CallBucket() :
            callInfos(),
            usageFlags(),
            instanceTransformFloats(),
            instanceTransformBuffer(),
            numInstances(0),
            buffersInitialized(false) {
        }
    };

    // Member variables: (a lot)

    RendererParameters m_parameters;

    InstanceListBuilder m_defaultListBuilder;
    InstanceListBuilder m_motionVectorsListBuilder;
    InstanceListBuilder m_transparencyListBuilder;
    std::vector<InstanceListBuilder> m_shadowMapListBuilders;
    std::vector<InstanceListBuilder> m_pointShadowListBuilders;

    FrustumCuller m_defaultFrustumCuller;
    FrustumCuller m_pointLightFrustumCuller;
    std::vector<FrustumCuller> m_shadowMapFrustumCullers;
    std::vector<FrustumCuller> m_pointShadowFrustumCullers;

    CallBucket m_defaultCallBucket;
    CallBucket m_skinnedCallBucket;
    CallBucket m_motionVectorsCallBucket;
    CallBucket m_motionVectorsCallBucketSkinned;
    CallBucket m_transparencyCallBucket;
    CallBucket m_transparencyCallBucketSkinned;
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
    Shader m_cameraMotionShader;
    Shader m_objectMotionShader;
    Shader m_objectMotionShaderSkinned;
    Shader m_backgroundMotionShader;
    Shader m_taaShader;
    Shader m_sharpenShader;
    Shader m_motionBlurShader;
    Shader m_taaMotionBlurCompositeShader;
    Shader m_transparencyShader;
    Shader m_transparencyShaderSkinned;
    Shader m_transparencyCompositeShader;
    Shader m_cloudsAccumShader;

    RenderLayer m_shadowMapRenderLayer;
    RenderLayer m_shadowMapFilterRenderLayer;
    RenderLayer m_pointLightShadowRenderLayer;
    RenderLayer m_sceneRenderLayer;
    RenderLayer m_gBufferRenderLayer;
    RenderLayer m_ssaoRenderLayer;
    RenderLayer m_ssaoFilterRenderLayer;
    RenderLayer m_bloomRenderLayer[2];
    RenderLayer m_motionRenderLayer;
    RenderLayer m_historyRenderLayer;
    RenderLayer m_motionBlurRenderLayer;
    RenderLayer m_transparencyRenderLayer;
    RenderLayer m_cloudsRenderLayer;
    RenderLayer m_cloudsAccumRenderLayer;

    Texture m_lightDepthTexture;
    Texture m_shadowMapArrayTexture;
    Texture m_shadowMapFilterTextures[2];
    Texture m_sceneTexture;
    //Texture m_gBufferPositionViewSpaceTexture;
    Texture m_gBufferNormalViewSpaceTexture;
    Texture m_gBufferAlbedoMetallicTexture;
    Texture m_gBufferEmissionRoughnessTexture;
    Texture m_gBufferDepthTexture;
    Texture m_ssaoNoiseTexture;
    Texture m_ssaoTargetTexture;
    Texture m_ssaoFilterTexture;
    Texture m_cloudNoiseBaseTexture;
    Texture m_cloudNoiseDetailTexture;
    Texture m_worleyTexture;
    Texture m_historyBuffer[2];
    Texture m_motionBuffer;
    Texture m_motionBlurTexture;
    Texture m_transparencyAccumTexture;
    Texture m_transparencyRevealageTexture;
    Texture m_cloudsRenderTexture;
    Texture m_cloudsHistoryTexture[2];
    //Texture m_fsScratchTexture;
    std::vector<Texture*> m_bloomTextures;
    std::vector<Texture*> m_pointLightShadowMaps;
    std::vector<Texture*> m_pointLightShadowDepthMaps;

    //RenderBuffer m_shadowMapDepthRenderBuffer;
    RenderBuffer m_motionVectorsDepthBuffer;
    //RenderBuffer m_gBufferDepthRenderBuffer;
    RenderBuffer m_sceneDepthStencilRenderBuffer;

    std::vector<glm::vec3> m_ssaoKernel;

    std::vector<glm::mat4> m_shadowMapCascadeMatrices;
    std::vector<glm::mat4> m_viewShadowMapCascadeMatrices;
    std::vector<float> m_shadowMapCascadeSplitDepths;
    std::vector<float> m_shadowMapCascadeBlurRanges;

    std::vector<glm::vec3> m_pointLightPositions;
    std::vector<glm::vec3> m_pointLightIntensities;
    std::vector<float> m_pointLightBoundingSphereRadii;
    std::vector<int> m_pointLightShadowMapIndices;

    std::vector<uint32_t> m_pointLightShadowMapLightIndices;
    std::vector<float> m_pointLightShadowMapLightKeys;

    std::vector<glm::mat4> m_pointLightShadowMapMatrices;

    std::vector<BoundingSphere> m_pointLightBoundingSpheres;

    //uint32_t m_numPointLightShadowMaps;
    uint32_t m_inUsePointLightShadowMaps;

    glm::mat4 m_cameraViewMatrix;
    glm::mat4 m_cameraProjectionMatrix;
    glm::mat4 m_lightViewMatrix;
    glm::mat4 m_viewToLightMatrix;
    glm::mat4 m_viewInverse;
    glm::mat4 m_viewNormals;
    glm::mat4 m_projectionInverse;
    glm::mat4 m_projectionNoJitter;
    glm::mat4 m_projectionNoJitterInverse;
    //glm::mat4 m_reprojectionMatrix;
    glm::mat4 m_lastView;
    glm::mat4 m_lastViewProj;
    glm::mat4 m_inverseViewProj;
    glm::mat4 m_viewProj;

    glm::vec3 m_cameraPosition;
    glm::vec3 m_lightDirection;
    glm::vec3 m_lightIntensity;
    glm::vec3 m_lightDirectionViewSpace;
    glm::vec3 m_ambientLightIntensity;

    float m_tanCameraFovX, m_tanCameraFovY;

    glm::vec2 m_viewJitter;

    Mesh m_fullScreenQuad;
    Mesh m_pointLightSphere;

    uint64_t m_frameCount;

    int m_nJitterSamples;
    std::vector<glm::vec2> m_jitterSamples;

    JobScheduler* m_pScheduler;

    //const Camera* m_pActiveCamera = nullptr;

    const Mesh* m_pBoundMesh = nullptr;

    const Texture* m_pBoundDiffuseTexture = nullptr;
    const Texture* m_pBoundNormalsTexture = nullptr;
    const Texture* m_pBoundEmissionTexture = nullptr;
    const Texture* m_pBoundMetallicRoughnessTexture = nullptr;

    uint32_t m_viewportWidth;
    uint32_t m_viewportHeight;

    uint32_t m_cloudsTextureWidth;
    uint32_t m_cloudsTextureHeight;

    bool m_directionalLightEnabled;
    bool m_directionalShadowsEnabled;
    bool m_isInitialized = false;
    bool m_isViewportInitialized = false;
    bool m_historyValid = false;
    bool m_cloudHistoryValid = false;

    // Methods:

    void setDirectionalLighting(const glm::vec3& direction, const glm::vec3& intensity);

    void addPointLight(const glm::vec3& position, const glm::vec3& intensity, bool shadowMap);
    void clearPointLights();

    void addPointLightShadowMap(uint32_t index);

    //void getCallBucketInstances(CallBucket& bucket, std::list<CallHeader>& headers, std::list<std::list<uint32_t>> instanceLists, const std::vector<bool>& cullResults, const Scene* pScene, glm::mat4 frustumMatrix, bool isSkinned);
    //void fillCallBucket(CallBucket& bucket, const std::vector<InstanceList>& instanceLists, size_t numInstances, const Scene* pScene, glm::mat4 globalMatrix, bool useNormalsMatrix);
    //void fillCallBucket(CallBucket& bucket, const std::list<CallHeader>& headers, const std::list<std::list<uint32_t>> instanceLists, const Scene* pScene, glm::mat4 globalMatrix, bool useNormalsMatrix);
    void fillCallBucketInstanceBuffer(CallBucket& bucket);
    void updateInstanceBuffers();

    void computeMatrices(const Camera* pCamera);
    void computeShadowMapMatrices(const glm::vec3& sceneAABBMin, const glm::vec3& sceneAABBMax, const Camera* pCamera);
    void computePointLightShadowMatrices();

    // it do what it say
    // Walks through call buckets and calls all the render* subroutines
    // Eventually draws a pretty picture to the main frambuffer
    //virtual
    void render();

    void renderCompositePass();
    void renderDeferredPass(); // check
    void renderBackground();  // check
    void renderGBuffer();  // check
    void renderShadowMap();  // check
    void renderPointLightShadows();  // check
    void renderSSAOMap();  // check
    void renderBloom();  // check
    void renderBackgroundMotionVectors();  // check
    void renderMotionVectors();  // check
    void renderTAA();  // check
    void renderMotionBlur();  // check
    void renderTransparency();  // check

    void filterShadowMap();

    void initDeferredPass();
    void initBackground();
    void initFullScreenQuad();
    void initGBuffer();
    void initShadowMap();
    void initPointLightShadowMaps();
    void initSSAO();
    void initBloom();
    void initMotionVectors();
    void initTAA();
    void initMotionBlur();
    void initTransparency();

    void createCloudTexture();



    struct FillCallBucketParam {
        CallBucket* bucket;
        const std::vector<InstanceList>* instanceLists;
        size_t numInstances;
        glm::mat4 globalMatrix;
        glm::mat4 lastGlobalMatrix;
        glm::mat4 normalsMatrix;
        bool useNormalsMatrix;
        bool hasSkinningMatrices;
        bool useLastFrameMatrix = false;
    };

    /*struct FillInstanceBuffersParam {
        Renderer* pRenderer;
        Window* pWindow;
        std::vector<CallBucket*> pBuckets;
        size_t nBuckets;
    };*/

    struct BuildInstanceListsParam {
        InstanceListBuilder* pListBuilder;
        FrustumCuller* pCuller;
        const Scene* pScene;
        glm::mat4 frustumMatrix;
        bool filterNonShadowCasters;
        bool (*predicate) (const Model*) = nullptr;
    };

    struct FrustumCullParam {
        FrustumCuller* pCuller;
        const Scene* pScene;
        glm::mat4 frustumMatrix;
    };

    std::vector<FrustumCullParam> m_frustumCullParams;

    FillCallBucketParam m_fillDefaultCallBucketParam;
    FillCallBucketParam m_fillSkinnedCallBucketParam;
    FillCallBucketParam m_fillMotionVectorsCallBucketParam;
    FillCallBucketParam m_fillSkinnedMotionVectorsCallBucketParam;
    FillCallBucketParam m_fillTransparencyCallBucketParam;
    FillCallBucketParam m_fillSkinnedTransparencyCallBucketParam;
    std::vector<FillCallBucketParam> m_fillShadowMapCallBucketParams;
    std::vector<FillCallBucketParam> m_fillSkinnedShadowMapCallBucketParams;
    std::vector<FillCallBucketParam> m_fillPointShadowCallBucketParams;
    std::vector<FillCallBucketParam> m_fillSkinnedPointShadowCallBucketParams;

    //FillInstanceBuffersParam m_fillInstanceBuffersParam;

    BuildInstanceListsParam m_buildDefaultListsParam;
    BuildInstanceListsParam m_buildMotionVectorsListsParam;
    BuildInstanceListsParam m_buildTransparencyListsParam;
    std::vector<BuildInstanceListsParam> m_buildShadowMapListsParams;
    std::vector<BuildInstanceListsParam> m_buildPointShadowMapListsParams;

    JobScheduler::CounterHandle m_listBuildersCounter = JobScheduler::COUNTER_NULL;
    JobScheduler::CounterHandle m_frustumCullCounter = JobScheduler::COUNTER_NULL;


    static void dispatchFrustumCullerJobs(uintptr_t param);
    static void dispatchListBuilderJobs(uintptr_t param);
    static void dispatchCallBucketJobs(uintptr_t param);

    static void fillCallBucketJob(uintptr_t param);
    static void buildInstanceListsJob(uintptr_t param);
    static void frustumCullJob(uintptr_t param);

};

// CallHeaders are used to key into a STL map, so this is needed
//bool operator<(const Renderer::CallHeader& lh, const Renderer::CallHeader& rh);

#endif // RENDERER_H_
