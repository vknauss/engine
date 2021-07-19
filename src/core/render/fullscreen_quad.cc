#include "fullscreen_quad.h"

Mesh* FullscreenQuad::fullscreenQuad;
Shader* FullscreenQuad::textureShader;
bool FullscreenQuad::initialized = false;

void FullscreenQuad::draw() {
    if (!initialized) initialize();
    fullscreenQuad->bind();
    fullscreenQuad->draw();
}

void FullscreenQuad::drawTextured(const Texture* pTexture, FullscreenQuad::DrawTexturedParam param) {
    if (!initialized) initialize();
    textureShader->bind();
    textureShader->setUniform("texSampler", 0);
    textureShader->setUniform("enableToneMapping", (int) param.toneMap);
    textureShader->setUniform("enableGammaCorrection", (int) param.gamma);
    textureShader->setUniform("gamma", param.gammaVal);
    textureShader->setUniform("exposure", param.exposure);
    pTexture->bind(0);
    draw();
}

#include <iostream>

void FullscreenQuad::initialize() {
    float fsqData[] = {
        // vertices
        -1, -1, 1,
         1, -1, 1,
        -1,  1, 1,
         1,  1, 1,
        // uvs
        0, 0,
        1, 0,
        0, 1,
        1, 1
    };

    fullscreenQuad = new Mesh;
    fullscreenQuad->setVertexCount(4);
    fullscreenQuad->setDrawType(GL_TRIANGLE_STRIP);
    fullscreenQuad->createVertexAttribute(0, 3);
    fullscreenQuad->createVertexAttribute(1, 2);
    fullscreenQuad->allocateVertexBuffer(fsqData);

    textureShader = new Shader;
    textureShader->linkShaderFiles("shaders/vertex_fs.glsl", "shaders/fragment_fstex.glsl");

    initialized = true;

    std::cout << "Initialized quad" << std::endl;
}
