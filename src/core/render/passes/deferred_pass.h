#ifndef DEFERRED_PASS_H_INCLUDED
#define DEFERRED_PASS_H_INCLUDED

#include <vector>

#include <glm/glm.hpp>

#include "core/render/render_pass.h"
#include "core/render/render_layer.h"
#include "core/resources/texture.h"
#include "core/resources/mesh.h"
#include "gbuffer_pass.h"

#include "deferred_directional_light_pass.h"
#include "deferred_point_light_pass.h"

class DeferredPass : public RenderPass {

public:

    void init() override;

    void onViewportResize(uint32_t width, uint32_t height) override;

    void setState() override;

    void render() override;

    void cleanup() override;

    void setGBufferPass(GBufferPass* pGBufferPass);

    // passed along to the directional light pass
    void setShadowMapPass(ShadowMapPass* pShadowMapPass);

    // passed along to the point light pass
    void setPointShadowPass(PointShadowPass* pPointShadowPass);

    void setSSAOTexture(Texture* pSSAOTexture);

    void setLightBleedCorrection(float bias, float power);

    void setAmbientLight(const glm::vec3& intensity, float power);

    void setDirectionalLight(const glm::vec3& intensity, const glm::vec3& direction);

    //void setPointLights(const std::vector<PointLight>& pointLights);
    void setPointLights(const PointLight* pPointLights, size_t numPointLights);

    void setMatrices(const glm::mat4& viewProjection, const glm::mat4& view, const glm::mat4& projectionInverse, const glm::mat4& viewInverse);

    RenderLayer* getSceneRenderLayer() {
        return &m_renderLayer;
    }

    Texture* getSceneTexture() {
        return &m_sceneTexture;
    }

    RenderBuffer* getSceneRenderBuffer() {
        return &m_depthStencilRenderBuffer;
    }

private:

    RenderLayer m_renderLayer;

    Texture m_sceneTexture;

    RenderBuffer m_depthStencilRenderBuffer;

    Shader m_deferredUnlitShader;

    DeferredDirectionalLightPass m_directionalLightPass;
    DeferredPointLightPass m_pointLightPass;

    GBufferPass* m_pGBufferPass = nullptr;

    Texture* m_pSSAOTexture = nullptr;

    std::vector<Texture*> m_pPointShadowMaps;

    uint32_t m_viewportWidth, m_viewportHeight;

    glm::vec3 m_ambientLightIntensity;
    float m_ambientPower;

};


#endif // DEFERRED_PASS_H_INCLUDED
