#ifndef RENDERER_H_
#define RENDERER_H_

#include <vector>

#include <GL/glew.h>

#include <glm/glm.hpp>

#include "core/app/window.h"
#include "core/job_scheduler.h"
#include "core/scene/scene.h"

#include "core/render/render_pass.h"
#include "core/render/passes/background_motion_vectors_pass.h"
#include "core/render/passes/bloom_pass.h"
#include "core/render/passes/deferred_pass.h"
#include "core/render/passes/gbuffer_pass.h"
#include "core/render/passes/motion_blur_pass.h"
#include "core/render/passes/motion_vectors_pass.h"
#include "core/render/passes/point_shadow_pass.h"
#include "core/render/passes/shadow_map_pass.h"
#include "core/render/passes/ssao_pass.h"
#include "core/render/passes/taa_pass.h"
#include "core/render/passes/transparency_pass.h"
#include "core/render/passes/transparency_composite_pass.h"
#include "core/render/passes/volumetric_clouds_pass.h"

class Renderer {

  public:
    // Constructor is empty, call init() to initialize
    explicit Renderer(JobScheduler* pScheduler);

    ~Renderer();

    // Initialize the resources for rendering: shaders, render layers/targets, etc
    void init(uint32_t width, uint32_t height);

    void setRenderToTexture(bool enabled);

    const Texture* getRenderTexture() const {
        return m_pRenderTexture;
    }

    // Free resources
    void cleanup();

    // Set size and initialize fullscreen render targets
    // These will NOT be initialized until this method is called, so call it early
    // Does not need to be called every frame unless the viewport is changing
    void setViewport(uint32_t width, uint32_t height);

    struct RendererJobParam {
        Renderer* pRenderer;
        //const Scene* pScene;
        const GameWorld* pGameWorld;
        const Camera* pCamera;

        const DirectionalLight* pDirectionalLight;
        glm::vec3 ambientLightIntensity;

        AppWindow* pWindow;
        JobScheduler* pScheduler;
        JobScheduler::CounterHandle signalCounterHandle;
        void (*pAfterSceneRenderCallback) (void) = nullptr;
    };

    static void preRenderJob(uintptr_t param);

    static void renderJob(uintptr_t param);

  private:

    BackgroundMotionVectorsPass m_backgroundMotionVectorsPass;
    BloomPass m_bloomPass;
    DeferredPass m_deferredPass;
    GBufferPass m_gBufferPass;
    MotionBlurPass m_motionBlurPass;
    MotionVectorsPass m_motionVectorsPass;
    PointShadowPass m_pointShadowPass;
    ShadowMapPass m_shadowMapPass;
    SSAOPass m_ssaoPass;
    TAAPass m_taaPass;
    TransparencyPass m_transparencyPass;
    TransparencyCompositePass m_transparencyCompositePass;
    VolumetricCloudsPass m_volumetricCloudsPass;

    GeometryRenderPass::UpdateParam m_gBufferUpdateParam,
                       m_transparencyUpdateParam;
    MotionVectorsPass::PreRenderParam m_motionVectorsPreRenderParam;
    PointShadowPass::PreRenderParam m_pointShadowsPreRenderParam;
    ShadowMapPass::PreRenderParam m_shadowMapPreRenderParam;

    Texture* m_pRenderTexture = nullptr;
    RenderLayer m_renderToTextureLayer;

    // Used by GBuffer, transparency, motion vectors passes
    FrustumCuller m_frustumCuller;
    //FrustumCuller::CullSceneParam m_cullSceneParam;
    FrustumCuller::CullEntitiesParam m_cullParam;

    glm::mat4 m_cameraViewMatrix;
    glm::mat4 m_cameraProjectionMatrix;
    glm::mat4 m_viewProj;
    glm::mat4 m_inverseViewProj;
    glm::mat4 m_viewInverse;
    glm::mat4 m_viewNormals;
    glm::mat4 m_projectionInverse;
    glm::mat4 m_projectionNoJitter;
    glm::mat4 m_projectionNoJitterInverse;
    glm::mat4 m_lastView;
    glm::mat4 m_lastViewProj;

    JobScheduler* m_pScheduler;

    uint32_t m_viewportWidth, m_viewportHeight;

    bool m_initialized = false;
    bool m_viewportInitialized = false;

    void render();

    //void updatePasses(const Scene* pScene);
    void updatePasses(const Camera* pCamera,
                      const DirectionalLight* pDirectionalLight,
                      const glm::vec3& ambientLightIntensity,
                      const PointLight* pPointLights,
                      size_t numPointLights);

    void computeMatrices(const Camera* pCamera);

};

#endif // RENDERER_H_
