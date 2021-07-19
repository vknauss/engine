#ifndef SHADOW_MAP_PASS_H_INCLUDED
#define SHADOW_MAP_PASS_H_INCLUDED

#include <vector>

#include <glm/glm.hpp>

#include "core/render/render_pass.h"
#include "core/render/render_layer.h"

#include "shadow_cascade_pass.h"

class ShadowMapPass : public RenderPass {

    friend class DeferredDirectionalLightPass;

public:

    void initForScheduler(JobScheduler* pScheduler);

    void init() override;

    void setState() override;

    void render() override;

    void cleanup() override;

    // These are intended to be called before init
    void setNumCascades(uint32_t numCascades);
    void setTextureSize(uint32_t textureSize);
    void setFilterTextureSize(uint32_t filterTextureSize);
    void setMaxDistance(float maxDistance);
    void setCascadeScale(float scale);
    void setCascadeBlurSize(float blurSize);

    // Call every frame
    void setMatrices(const glm::mat4& cameraViewInverse, const glm::mat4& lightViewMatrix);

    struct PreRenderParam {
        ShadowMapPass* pPass;
        //const Scene* pScene;
        const GameWorld* pGameWorld;
        const Camera* pCamera;

        JobScheduler::CounterHandle signalCounter;
    };

    static void preRenderJob(uintptr_t param);

private:

    RenderLayer m_depthRenderLayer;
    RenderLayer m_varianceRenderLayer;
    RenderLayer m_filterRenderLayer;

    Texture m_depthArrayTexture;
    Texture m_evsmArrayTexture;
    Texture m_filterTextures[2];

    Shader m_depthOnlyShader;
    Shader m_depthOnlyShaderSkinned;
    Shader m_depthToVarianceShader;
    Shader m_filterShader;

    uint32_t m_textureSize;
    uint32_t m_filterTextureSize;

    uint32_t m_numCascades;

    std::vector<FrustumCuller> m_cascadeFrustumCullers;
    //std::vector<FrustumCuller::CullSceneParam> m_frustumCullerJobParams;
    std::vector<FrustumCuller::CullEntitiesParam> m_frustumCullerJobParams;

    std::vector<ShadowCascadePass> m_cascadePasses;
    std::vector<GeometryRenderPass::UpdateParam> m_cascadePassUpdateParams;

    std::vector<glm::mat4> m_cascadeMatrices;
    std::vector<glm::mat4> m_viewCascadeMatrices;

    std::vector<float> m_cascadeSplitDepths;
    std::vector<float> m_cascadeBlurRanges;

    glm::mat4 m_lightViewMatrix;
    glm::mat4 m_cameraViewInverse;
    glm::mat4 m_viewToLightMatrix;

    float m_maxDistance;
    float m_cascadeScale;
    float m_cascadeBlurSize;

    JobScheduler* m_pScheduler;

    //void computeMatrices(const Scene* pScene);
    void computeMatrices(const glm::vec3& sceneAABBMin, const glm::vec3& sceneAABBMax, const Camera* pCamera);


};

#endif // SHADOW_MAP_PASS_H_INCLUDED
