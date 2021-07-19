#include "core/render/passes/bloom_pass.h"

#include "core/render/fullscreen_quad.h"

void BloomPass::init() {
    m_hdrExtractShader.linkShaderFiles("shaders/vertex_fs.glsl", "shaders/fragment_hdr_extract.glsl");

    // 5-tap gaussian: [0.06136, 0.24477, 0.38774, 0.24477, 0.06136]
    // half: [0.38774, 0.24477, 0.06136]
    // combine weights: [0.38774, 0.24477 + 0.06136] = [0.38774, 0.30613]
    // offsets: [0.0, lerp(1.0, 2.0, 0.06136 / 0.30613)] = [0.0, 1.20043772253]
    const char* filterHeader =
        "#version 330\n"
        "const int WIDTH = 3;\n"
        "const float WEIGHTS[3] = float[3](0.20236, 0.303053, 0.095766);"
        "const float OFFSETS[3] = float[3](0.0, 1.409199051, 3.297934549);";

    m_filterShader.linkShaderFiles("shaders/vertex_fs.glsl", "shaders/fragment_filter_stub.glsl", "", filterHeader);

    TextureParameters param = {};
    param.numComponents = 4;
    param.useFloatComponents = true;
    param.useLinearFiltering = true;
    param.useEdgeClamping = true;

    m_renderTextures.resize(2*numLevels);

    for (Texture& texture : m_renderTextures) texture.setParameters(param);
}

void BloomPass::onViewportResize(uint32_t width, uint32_t height) {
    m_viewportWidth = width;
    m_viewportHeight = height;

    for (auto i = 0u; i < numLevels; ++i) {
        TextureParameters param = m_renderTextures[2*i].getParameters();
        param.width = width;
        param.height = height;

        m_renderTextures[2*i  ].setParameters(param);
        m_renderTextures[2*i+1].setParameters(param);
        m_renderTextures[2*i  ].allocateData(nullptr);
        m_renderTextures[2*i+1].allocateData(nullptr);
        if (width  > 1u) width  /= 2u;
        if (height > 1u) height /= 2u;
    }
}

void BloomPass::setState() {
    glDepthMask(GL_FALSE);
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    m_renderLayer[0].setTextureAttachment(0, &m_renderTextures[0]);
    m_renderLayer[0].setEnabledDrawTargets({0});
    m_renderLayer[0].bind();
    glViewport(0, 0, m_viewportWidth, m_viewportHeight);
    glClearColor(0, 0, 0, 0);
    glClear(GL_COLOR_BUFFER_BIT);
}

void BloomPass::render() {
    m_hdrExtractShader.bind();
    m_hdrExtractShader.setUniform("texSampler", 0);

    m_pSceneTexture->bind(0);

    FullscreenQuad::draw();

    // do blurring for bloom effect

    m_filterShader.bind();
    m_filterShader.setUniform("tex_sampler", 0);

    //int nBloomIterations = 1;
    for (auto i = 0u; i < numLevels; ++i) {
        TextureParameters cLevelParameters = m_renderTextures[2*i].getParameters();

        m_renderLayer[i%2].setTextureAttachment(0, &m_renderTextures[2*i]);
        m_renderLayer[i%2].setTextureAttachment(1, &m_renderTextures[2*i+1]);

        if (i > 0) {
            TextureParameters pLevelParameters = m_renderTextures[2*i-1].getParameters();

            m_renderLayer[(i-1)%2].setEnabledReadTarget(0);
            m_renderLayer[i%2].setEnabledDrawTargets({0});

            m_renderLayer[(i-1)%2].bind(GL_READ_FRAMEBUFFER);
            m_renderLayer[i%2].bind(GL_DRAW_FRAMEBUFFER);

            glViewport(0, 0, cLevelParameters.width, cLevelParameters.height);
            glClear(GL_COLOR_BUFFER_BIT);

            glBlitFramebuffer(
                0, 0, pLevelParameters.width, pLevelParameters.height,
                0, 0, cLevelParameters.width, cLevelParameters.height,
                GL_COLOR_BUFFER_BIT, GL_LINEAR);

            m_renderLayer[(i-1)%2].setTextureAttachment(0, nullptr);
            m_renderLayer[(i-1)%2].setTextureAttachment(1, nullptr);
        }

        // First pass filters horizontally
        m_filterShader.setUniform("coordOffset", glm::vec2(1.0f/cLevelParameters.width, 0.0f));

        m_renderLayer[i%2].setEnabledDrawTargets({1});
        m_renderLayer[i%2].validate();
        m_renderLayer[i%2].bind();

        glViewport(0, 0, cLevelParameters.width, cLevelParameters.height);
        glClear(GL_COLOR_BUFFER_BIT);

        m_renderTextures[2*i].bind(0);

        FullscreenQuad::draw();

        // Second pass filters vertically
        m_filterShader.setUniform("coordOffset", glm::vec2(0.0f, 1.0f/cLevelParameters.height));

        m_renderLayer[i%2].setEnabledDrawTargets({0});
        m_renderLayer[i%2].validate();
        m_renderLayer[i%2].bind();

        glViewport(0, 0, cLevelParameters.width, cLevelParameters.height);
        glClear(GL_COLOR_BUFFER_BIT);

        m_renderTextures[2*i+1].bind(0);

        FullscreenQuad::draw();
    }
    m_renderLayer[0].setTextureAttachment(1, nullptr);

    // Upsample and accumulate results into level-0 texture with additive blending
    glBlendFunc(GL_ONE, GL_ONE);

    for (auto i = (int)numLevels-1; i > 0; --i) {
        m_renderLayer[0].setTextureAttachment(0, &m_renderTextures[2*(i-1)]);
        m_renderLayer[0].setEnabledDrawTargets({0});
        m_renderLayer[0].bind();

        glViewport(0, 0,
                   m_renderTextures[2*(i-1)].getParameters().width,
                   m_renderTextures[2*(i-1)].getParameters().height);

        FullscreenQuad::drawTextured(&m_renderTextures[2*i]);
    }

    m_pSceneRenderLayer->bind();
    glViewport(0, 0, m_viewportWidth, m_viewportHeight);

    FullscreenQuad::drawTextured(&m_renderTextures[0]);
}

void BloomPass::cleanup() {
    m_renderTextures.clear();
    numLevels = 0;
}

void BloomPass::setSceneRenderLayer(RenderLayer* pSceneRenderLayer, Texture* pSceneTexture) {
    m_pSceneRenderLayer = pSceneRenderLayer;
    m_pSceneTexture = pSceneTexture;
}
