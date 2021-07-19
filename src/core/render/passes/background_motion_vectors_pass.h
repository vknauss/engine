#ifndef BACKGROUND_MOTION_VECTORS_PASS_H
#define BACKGROUND_MOTION_VECTORS_PASS_H

#include <glm/glm.hpp>

#include "core/render/render_pass.h"
#include "core/render/render_layer.h"
#include "core/render/shader.h"

class BackgroundMotionVectorsPass : public RenderPass {

    friend class MotionVectorsPass;

public:

    void init() override;

    void onViewportResize(uint32_t width, uint32_t height) override;

    void setState() override;

    void render() override;

    void setMatrices(const glm::mat4& projectionNoJitter,
                     const glm::mat4& inverseProjectionNoJitter,
                     const glm::mat4& lastView,
                     const glm::mat4& inverseView);

    Texture* getMotionBuffer() {
        return &m_motionBuffer;
    }

private:

    RenderLayer m_renderLayer;
    Texture m_motionBuffer;
    Shader m_shader;

    uint32_t m_viewportWidth, m_viewportHeight;

    glm::mat4 m_projection,
              m_projectionInverse,
              m_lastView,
              m_viewInverse;

};

#endif // BACKGROUND_MOTION_VECTORS_PASS_H
