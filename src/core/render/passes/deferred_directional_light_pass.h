#ifndef DEFERRED_DIRECTIONAL_LIGHT_PASS_H_INCLUDED
#define DEFERRED_DIRECTIONAL_LIGHT_PASS_H_INCLUDED

#include "core/render/render_pass.h"
#include "core/render/shader.h"
#include "shadow_map_pass.h"

// A sub-pass to DeferredPass
class DeferredDirectionalLightPass : public RenderPass {

public:

    void init() override;

    void setState() override;

    void render() override;

    void setShadowMapPass(ShadowMapPass* pShadowMapPass);

    // Decided to just make these public instead of adding setters for all of them
    // shoot me
    glm::vec3 lightDirection;
    glm::vec3 lightIntensity;
    glm::mat4 projectionInverse;
    glm::mat4 cameraViewMatrix;

    float lightBleedCorrectionBias;
    float lightBleedCorrectionPower;

private:

    ShadowMapPass* m_pShadowMapPass = nullptr;

    Shader m_shader;
};

#endif // DEFERRED_DIRECTIONAL_LIGHT_PASS_H_INCLUDED
