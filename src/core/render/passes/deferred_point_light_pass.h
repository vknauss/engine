#ifndef DEFERRED_POINT_LIGHT_PASS_H_INCLUDED
#define DEFERRED_POINT_LIGHT_PASS_H_INCLUDED

#include <vector>

#include "core/render/render_pass.h"
#include "core/render/shader.h"
#include "point_shadow_pass.h"
#include "core/scene/point_light.h"

class DeferredPointLightPass : public RenderPass {

public:

    void init() override;

    void onViewportResize(uint32_t width, uint32_t height) override;

    void setState() override;

    void render() override;

    void cleanup() override;

    void setPointShadowPass(PointShadowPass* pPointShadowPass);

    void setRenderLayer(RenderLayer* pRenderLayer);

    //void setPointLights(const std::vector<PointLight>& pointLights);
    void setPointLights(const PointLight* pPointLights, size_t numPointLights);

    glm::mat4 viewProj;
    glm::mat4 viewInverse;
    glm::mat4 projectionInverse;
    glm::mat4 cameraViewMatrix;

    float lightBleedCorrectionBias;
    float lightBleedCorrectionPower;

private:

    Shader m_stencilVolumesShader;
    Shader m_deferredPointLightShader;
    Shader m_deferredPointLightShaderShadow;

    RenderLayer* m_pRenderLayer = nullptr;

    PointShadowPass* m_pPointShadowPass = nullptr;

    Mesh m_pointLightSphere;

    //std::vector<PointLight> m_pointLights;
    const PointLight* m_pPointLights = nullptr;
    size_t m_numPointLights = 0;

    uint32_t m_viewportWidth, m_viewportHeight;

};

#endif // DEFERRED_POINT_LIGHT_PASS_H_INCLUDED
