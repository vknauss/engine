#ifndef POINT_SHADOW_PASS_H_INCLUDED
#define POINT_SHADOW_PASS_H_INCLUDED

#include <vector>

#include "core/render/render_pass.h"
#include "core/render/shader.h"

#include "point_shadow_face_pass.h"

class PointShadowPass : public RenderPass {

    friend class DeferredPointLightPass;

public:

    void initForScheduler(JobScheduler* pScheduler);

    void init() override;

    void setState() override;

    void render() override;

    void cleanup() override;

    void setMaxPointShadowMaps(uint32_t maxPointShadowMaps);
    void setTextureSize(uint32_t textureSize);

    void setCameraViewMatrix(const glm::mat4& cameraViewMatrix);
    void setCameraFrustumMatrix(const glm::mat4& cameraFrustumMatrix);
    void setPointLights(const PointLight* pPointLights, size_t numPointLights);

    struct PreRenderParam {
        PointShadowPass* pPass;
        //const Scene* pScene;
        const GameWorld* pGameWorld;

        JobScheduler::CounterHandle signalCounter;
    };

    static void preRenderJob(uintptr_t param);

private:

    RenderLayer m_depthRenderLayer;
    RenderLayer m_varianceRenderLayer;

    Shader m_depthOnlyShader;
    Shader m_depthOnlyShaderSkinned;
    Shader m_depthToVarianceShader;

    FrustumCuller m_lightSpheresCuller;

    std::vector<FrustumCuller> m_frustumCullers;
    //std::vector<FrustumCuller::CullSceneParam> m_frustumCullerJobParams;
    std::vector<FrustumCuller::CullEntitiesParam> m_frustumCullerJobParams;

    std::vector<PointShadowFacePass> m_facePasses;
    std::vector<GeometryRenderPass::UpdateParam> m_facePassUpdateParams;

    std::vector<glm::mat4> m_faceMatrices;

    std::vector<Texture> m_depthCubeMaps;
    std::vector<Texture> m_evsmCubeMaps;

    // The index of the shadow map for each light
    // -1 if none
    std::vector<int> m_lightShadowMapIndices;

    // The index of the light each shadow map corresponds to
    std::vector<uint32_t> m_lightIndices;

    std::vector<float> m_lightKeys;

    JobScheduler* m_pScheduler = nullptr;

    const PointLight* m_pPointLights = nullptr;
    size_t m_numPointLights = 0;

    uint32_t m_maxPointShadowMaps;
    uint32_t m_inUsePointShadowMaps;

    uint32_t m_textureSize;

    glm::mat4 m_cameraViewMatrix;
    glm::mat4 m_cameraFrustumMatrix;

};

#endif // POINT_SHADOW_PASS_H_INCLUDED
