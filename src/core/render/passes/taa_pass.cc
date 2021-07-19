#include "core/render/passes/taa_pass.h"

#include "core/render/fullscreen_quad.h"

// https://en.wikipedia.org/wiki/Halton_sequence
static void halton(int base, int n, float* out) {
    int num = 0, den = 1;
    for (int i = 0; i < n; ++i) {
        int x = den - num;
        if (x == 1) {
            num = 1;
            den *= base;
        } else {
            int y = den / base;
            while (x <= y) y /= base;
            num = (base + 1) * y - x;
        }
        out[i] = (float) num / (float) den;
    }
}

void TAAPass::init() {
    m_shader.linkShaderFiles("shaders/vertex_fs.glsl", "shaders/fragment_taa.glsl");
    m_motionBlurCompositeShader.linkShaderFiles("shaders/vertex_fs.glsl", "shaders/fragment_composite_motion_taa.glsl");

    TextureParameters historyTexParams = {};
    historyTexParams.numComponents = 4;
    historyTexParams.useEdgeClamping = true;
    historyTexParams.useFloatComponents = true;
    historyTexParams.useLinearFiltering = true;

    m_historyTextures[0].setParameters(historyTexParams);
    m_historyTextures[1].setParameters(historyTexParams);

    float samplesX[7], samplesY[7];
    halton(2, 7, samplesX);
    halton(3, 7, samplesY);

    jitterSamples.resize(7);
    for (int i = 0; i < 7; ++i)
        jitterSamples[i] = glm::vec2(samplesX[i], samplesY[i]) - 0.5f;

    cJitter = jitterSamples[0];
}

void TAAPass::onViewportResize(uint32_t width, uint32_t height) {
    m_viewportWidth = width;
    m_viewportHeight = height;

    TextureParameters param = m_historyTextures[0].getParameters();
    param.width = width;
    param.height = height;

    m_historyTextures[0].setParameters(param);
    m_historyTextures[1].setParameters(param);

    m_historyTextures[0].allocateData(nullptr);
    m_historyTextures[1].allocateData(nullptr);

    m_renderLayer.setTextureAttachment(0, &m_historyTextures[0]);
    m_renderLayer.setTextureAttachment(1, &m_historyTextures[1]);

    m_historyValid = false;
}

void TAAPass::setState() {
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glDisable(GL_BLEND);
    glDisable(GL_STENCIL_TEST);
}

void TAAPass::render() {
    if (m_historyValid) {
        m_renderLayer.setEnabledDrawTargets({m_cHistoryIndex});
        m_renderLayer.bind();
        glViewport(0, 0, m_viewportWidth, m_viewportHeight);
        glClear(GL_COLOR_BUFFER_BIT);

        m_shader.bind();

        m_shader.setUniform("currentBuffer", 0);
        m_shader.setUniform("historyBuffer", 1);
        m_shader.setUniform("motionBuffer", 2);

        m_shader.setUniform("jitter", cJitter);

        m_pSceneTexture->bind(0);
        m_historyTextures[(m_cHistoryIndex+1)%2].bind(1);
        m_pMotionBuffer->bind(2);

        FullscreenQuad::draw();

        if (m_pMotionBlurTexture) {
            m_pSceneRenderLayer->setEnabledDrawTargets({0});
            m_pSceneRenderLayer->bind();

            glViewport(0, 0, m_viewportWidth, m_viewportHeight);
            glClear(GL_COLOR_BUFFER_BIT);

            m_motionBlurCompositeShader.bind();

            m_motionBlurCompositeShader.setUniform("motionBlurTexture", 0);
            m_motionBlurCompositeShader.setUniform("taaTexture", 1);
            m_motionBlurCompositeShader.setUniform("motionBuffer", 2);

            m_motionBlurCompositeShader.setUniform("jitter", cJitter);

            m_pMotionBlurTexture->bind(0);
            m_historyTextures[m_cHistoryIndex].bind(1);
            m_pMotionBuffer->bind(2);

            FullscreenQuad::draw();
        } else {
            m_renderLayer.setEnabledReadTarget(m_cHistoryIndex);
            m_pSceneRenderLayer->setEnabledDrawTargets({0});

            m_renderLayer.bind(GL_READ_FRAMEBUFFER);
            m_pSceneRenderLayer->bind(GL_DRAW_FRAMEBUFFER);

            glBlitFramebuffer(0, 0, m_viewportWidth, m_viewportHeight,
                              0, 0, m_viewportWidth, m_viewportHeight,
                              GL_COLOR_BUFFER_BIT, GL_NEAREST);
        }
    } else {
        m_renderLayer.setEnabledDrawTargets({m_cHistoryIndex});
        m_pSceneRenderLayer->setEnabledReadTarget(0);

        m_renderLayer.bind(GL_DRAW_FRAMEBUFFER);
        m_pSceneRenderLayer->bind(GL_READ_FRAMEBUFFER);

        glBlitFramebuffer(0, 0, m_viewportWidth, m_viewportHeight,
                          0, 0, m_viewportWidth, m_viewportHeight,
                          GL_COLOR_BUFFER_BIT, GL_NEAREST);

        m_historyValid = true;
    }

    m_cHistoryIndex = (m_cHistoryIndex + 1) % 2;
    m_cJitterIndex = (m_cJitterIndex + 1) % jitterSamples.size();
    cJitter = jitterSamples[m_cJitterIndex] / (glm::vec2(m_viewportWidth, m_viewportHeight));
}

void TAAPass::cleanup() {
    m_pMotionBlurTexture = nullptr;
    jitterSamples.clear();
    m_historyValid = false;
    m_cHistoryIndex = 0;
    m_cJitterIndex = 0;
}

void TAAPass::setMotionBlurTexture(Texture* pMotionBlurTexture) {
    m_pMotionBlurTexture = pMotionBlurTexture;
}

void TAAPass::setMotionBuffer(Texture* pMotionBuffer) {
    m_pMotionBuffer = pMotionBuffer;
}

void TAAPass::setSceneRenderLayer(RenderLayer* pSceneRenderLayer) {
    m_pSceneRenderLayer = pSceneRenderLayer;
}

void TAAPass::setSceneTexture(Texture* pSceneTexture) {
    m_pSceneTexture = pSceneTexture;
}
