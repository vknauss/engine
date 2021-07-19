#ifndef MOTION_VECTORS_PASS_H_INCLUDED
#define MOTION_VECTORS_PASS_H_INCLUDED

#include "core/render/render_pass.h"
#include "background_motion_vectors_pass.h"
#include "object_motion_vectors_pass.h"

class MotionVectorsPass : public RenderPass {

public:

    void initForScheduler(JobScheduler* pScheduler);

    void init() override;

    void onViewportResize(uint32_t width, uint32_t height) override;

    void setState() override;

    void render() override;

    void cleanup() override;

    // Optional, only set this if there is a background pass
    // The data from the background motion buffer will be copied in
    void setBackgroundPass(BackgroundMotionVectorsPass* pBackgroundPass);

    // Required, set once
    void setGBufferDepthTexture(Texture* pGBufferDepthTexture);

    // Set each frame
    void setMatrices(const glm::mat4& lastViewProj, const glm::mat4& inverseViewProj);

    Texture* getMotionBuffer() {
        return &m_motionBuffer;
    }

    struct PreRenderParam {
        MotionVectorsPass* pPass;
        //const Scene* pScene;
        const GameWorld* pGameWorld;
        FrustumCuller* pCuller;

        glm::mat4 cameraMatrix;
        glm::mat4 lastCameraMatrix;

        JobScheduler::CounterHandle signalCounter;
    };

    static void preRenderJob(uintptr_t param);

private:

    RenderLayer m_renderLayer;

    Texture m_motionBuffer;
    RenderBuffer m_depthBuffer;

    Shader m_cameraMotionShader;

    ObjectMotionVectorsPass m_objectPass;
    GeometryRenderPass::UpdateParam m_objectPassUpdateParam;

    BackgroundMotionVectorsPass* m_pBackgroundPass = nullptr;
    Texture* m_pGBufferDepthTexture = nullptr;

    JobScheduler* m_pScheduler = nullptr;

    uint32_t m_viewportWidth, m_viewportHeight;

    glm::mat4 m_lastViewProj,
              m_viewProjInverse;

};

#endif // MOTION_VECTORS_PASS_H_INCLUDED
