#include "core/render/passes/ssao_pass.h"

#include <glm/gtc/random.hpp>

#include "core/render/fullscreen_quad.h"

void SSAOPass::init() {
    m_shader.linkShaderFiles("shaders/vertex_fs.glsl", "shaders/fragment_ssao.glsl", "", "#version 400\n");

    // 5-tap gaussian: [0.06136, 0.24477, 0.38774, 0.24477, 0.06136]
    // half: [0.38774, 0.24477, 0.06136]
    // combine weights: [0.38774, 0.24477 + 0.06136] = [0.38774, 0.30613]
    // offsets: [0.0, lerp(1.0, 2.0, 0.06136 / 0.30613)] = [0.0, 1.20043772253]
    const char* filterHeader =
        "#version 330\n"
        "const int WIDTH = 2;\n"
        "const float WEIGHTS[2] = float[2](0.38774, 0.30613);"
        "const float OFFSETS[2] = float[2](0.0, 1.20043772253);";

    m_filterShader.linkShaderFiles("shaders/vertex_fs.glsl", "shaders/fragment_filter_stub.glsl", "", filterHeader);

    // kernel generation, hemisphere oriented toward positive Z, weighted more closely to center
    m_kernel.resize(64);
    for (auto i = 0u; i < m_kernel.size(); ++i) {
        glm::vec3 sample = glm::ballRand(1.0);  // random point in unit sphere
        sample.z = glm::abs(sample.z);          // flip orientation to hemisphere
        sample *= glm::mix(0.1, 1.0, (float) i / (float) m_kernel.size());  // weight towards center as i increases
        m_kernel[i] = sample;
    }

    // Generate random noise texture used to orient tangent, thereby rotating the kernel
    int rotationTextureSize = 4;
    std::vector<glm::vec3> rotationVecs(rotationTextureSize*rotationTextureSize);
    for (auto i = 0u; i < rotationVecs.size(); ++i) {
        rotationVecs[i] = glm::vec3(glm::circularRand(1.0), 0.0);
    }

    TextureParameters params = {};
    params.arrayLayers = 1;
    params.numComponents = 3;
    params.useFloatComponents = true;
    params.width = rotationTextureSize;
    params.height = rotationTextureSize;

    m_noiseTexture.setParameters(params);
    m_noiseTexture.allocateData(rotationVecs.data());

    // setup render target
    params = {};
    params.arrayLayers = 1;
    params.numComponents = 1;
    params.useEdgeClamping = true;
    params.useLinearFiltering = true;

    m_renderTexture.setParameters(params);
    m_filterTexture.setParameters(params);
}

void SSAOPass::onViewportResize(uint32_t width, uint32_t height) {
    m_renderTextureWidth = width;
    m_renderTextureHeight = height;

    TextureParameters param = m_renderTexture.getParameters();
    param.width = m_renderTextureWidth;
    param.height = m_renderTextureHeight;

    m_renderTexture.setParameters(param);
    m_filterTexture.setParameters(param);

    m_renderTexture.allocateData(nullptr);
    m_filterTexture.allocateData(nullptr);

    m_renderLayer.setTextureAttachment(0, &m_renderTexture);

    m_filterRenderLayer.setTextureAttachment(0, &m_filterTexture);
    m_filterRenderLayer.setTextureAttachment(1, &m_renderTexture);
}

void SSAOPass::setState() {
    glDisable(GL_BLEND);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_STENCIL_TEST);


    m_renderLayer.setEnabledDrawTargets({0});
    m_renderLayer.bind();
    glViewport(0, 0, m_renderTextureWidth, m_renderTextureHeight);
    glClearColor(1.0, 0.0, 0.0, 0.0);
    glClear(GL_COLOR_BUFFER_BIT);
}

void SSAOPass::render() {
    m_shader.bind();
    m_shader.setUniformArray("ssaoKernelSamples", m_kernel.size(), m_kernel.data());
    m_shader.setUniform("projection", m_projection);
    m_shader.setUniform("inverseProjection", m_projectionInverse);
    m_shader.setUniform("gBufferDepth", 0);
    m_shader.setUniform("gBufferNormalViewSpace", 1);
    m_shader.setUniform("randomRotationTexture", 2);

    m_pGBufferDepth->bind(0);
    m_pGBufferNormals->bind(1);
    m_noiseTexture.bind(2);

    FullscreenQuad::draw();

    glClearColor(0.0, 0.0, 0.0, 0.0);

    // filter ssao texture

    m_filterShader.bind();
    m_filterShader.setUniform("tex_sampler", 0);

    // horizontal
    m_filterShader.setUniform("coordOffset", glm::vec2(1.0f/m_renderTextureWidth, 0.0f));

    m_filterRenderLayer.setEnabledDrawTargets({0});
    m_filterRenderLayer.bind();
    glViewport(0, 0, m_renderTextureWidth, m_renderTextureHeight);
    glClear(GL_COLOR_BUFFER_BIT);

    m_renderTexture.bind(0);
    FullscreenQuad::draw();

    // vertical
    m_filterShader.setUniform("coordOffset", glm::vec2(0.0f, 1.0f/m_renderTextureHeight));

    m_filterRenderLayer.setEnabledDrawTargets({1});
    m_filterRenderLayer.bind();
    glViewport(0, 0, m_renderTextureWidth, m_renderTextureHeight);
    glClear(GL_COLOR_BUFFER_BIT);

    m_filterTexture.bind(0);
    FullscreenQuad::draw();
}

void SSAOPass::setGBufferTextures(Texture* pGBufferDepth, Texture* pGBufferNormals) {
    m_pGBufferDepth = pGBufferDepth;
    m_pGBufferNormals = pGBufferNormals;
}

void SSAOPass::setMatrices(const glm::mat4& projection, const glm::mat4& inverseProjection) {
    m_projection = projection;
    m_projectionInverse = inverseProjection;
}
