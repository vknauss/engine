#ifndef VOLUMETRIC_CLOUDS_PASS_H_INCLUDED
#define VOLUMETRIC_CLOUDS_PASS_H_INCLUDED

#include <glm/glm.hpp>

#include "core/render/render_layer.h"
#include "core/render/render_pass.h"
#include "core/render/shader.h"

#include "core/scene/camera.h"

class VolumetricCloudsPass : public RenderPass {

public:

    void init() override;

    void onViewportResize(uint32_t width, uint32_t height) override;

    void setState() override;

    void render() override;

    void cleanup() override;

    void setDirectionalLight(const glm::vec3& intensity, const glm::vec3& direction);
    void setInverseViewMatrix(const glm::mat4& inverseView);
    void setCamera(const Camera* pCamera);

    void setMotionBuffer(Texture* pMotionBuffer);

    void setOutputRenderTarget(RenderLayer* pRenderLayer);

    float cloudiness = 0.42;
    float density = 0.15;
    float extinction = 0.064;
    float scattering = 0.098;

    float cloudBottomHeight = 1500.0;
    float cloudTopHeight = 3600.0;
    float planetRadius = 1e6;

    int steps = 64;
    int shadowSteps = 8;
    float shadowStepSize = 100.0;

    uint32_t baseNoiseTextureSize = 128;
    uint32_t baseNoiseTextureChannels = 3;
    int baseNoiseOctaves = 4;
    float baseNoiseFrequency = 0.01;
    int baseNoiseWorleyPoints[2] {16, 32};
    glm::vec4 baseNoiseContribution = {1.24, -0.34, -0.19, 0};
    glm::vec4 baseNoiseThreshold = {0, 0.55, 0.8, 0};
    float baseNoiseScale = 0.0000647850;

    uint32_t detailNoiseTextureSize = 32;
    uint32_t detailNoiseTextureChannels = 1;
    int detailNoiseOctaves[1] {4};
    float detailNoiseFrequency[1] {0.1};
    float detailNoiseScale = 0.0000513456;
    float detailNoiseInfluence = 0.25;

private:

    Shader m_cloudsShader, m_accumShader;

    RenderLayer m_renderLayer, m_accumRenderLayer;

    RenderLayer* m_pOutputRenderLayer = nullptr;

    Texture m_renderTexture;
    Texture m_historyTextures[2];
    uint32_t m_cHistoryIndex = 0;
    uint32_t m_cOffsetIndex = 0;

    Texture m_baseNoiseTexture, m_detailNoiseTexture;

    Texture* m_pMotionBuffer = nullptr;

    uint32_t m_renderTextureWidth, m_renderTextureHeight;
    uint32_t m_viewportWidth, m_viewportHeight;

    glm::mat4 m_viewInverse;

    glm::vec3 m_lightDirection, m_lightIntensity;

    glm::vec3 m_cameraPosition;
    glm::vec2 m_cameraHalfScreenSize;

    bool m_historyValid = false;

};

#endif // VOLUMETRIC_CLOUDS_PASS_H_INCLUDED
