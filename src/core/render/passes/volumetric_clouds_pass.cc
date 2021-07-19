#include "core/render/passes/volumetric_clouds_pass.h"

#include <fstream>

#include <glm/gtc/noise.hpp>
#include <glm/gtc/random.hpp>

#include "core/render/fullscreen_quad.h"

#include "core/util/timer.h"

static void fbm(int widthX, int widthY, int widthZ, int octaves, float scale, float tileBlend, bool tileX, bool tileY, bool tileZ, float* noiseOut) {
    float influence = 1.0f;
    glm::vec3 width(widthX, widthY, widthZ);
    glm::vec3 blendWidth = tileBlend * width;
    glm::vec3 blendStart = width - blendWidth;
    for (int o = 0; o < octaves; ++o) {
        for (int i = 0; i < widthX; ++i) for (int j = 0; j < widthY; ++j) for (int k = 0; k < widthZ; ++k) {
            glm::vec3 pos(i, j, k);
            float noise = glm::simplex(scale * pos);
            //noiseOut[(i*widthY + j)*widthZ+k] += influence * glm::simplex(scale * glm::vec3(i, j, k));
            glm::bvec3 cmp = glm::greaterThan(pos, blendWidth) && glm::bvec3(tileX, tileY, tileZ);
            if (glm::any(cmp)) {
                glm::vec3 pos1 = pos - glm::vec3(cmp) * width;
                glm::vec3 interp = glm::max((pos - blendStart)/blendWidth, glm::vec3(0));

                if (cmp.x) {
                    noise = glm::mix(noise, glm::simplex(scale * glm::vec3(pos1.x, pos.y, pos.z)), interp.x);
                }
                if (cmp.y) {
                    float n1 = glm::simplex(scale * glm::vec3(pos.x, pos1.y, pos.z));
                    if (cmp.x) {
                        n1 = glm::mix(n1, glm::simplex(scale * glm::vec3(pos1.x, pos1.y, pos.z)), interp.x);
                    }
                    noise = glm::mix(noise, n1, interp.y);
                }
                if (cmp.z) {
                    float n1 = glm::simplex(scale * glm::vec3(pos.x, pos.y, pos1.z));
                    if (cmp.x) {
                        n1 = glm::mix(n1, glm::simplex(scale * glm::vec3(pos1.x, pos.y, pos1.z)), interp.x);
                    }
                    if (cmp.y) {
                        float n2 = glm::simplex(scale * glm::vec3(pos.x, pos1.y, pos1.z));
                        if (cmp.x) {
                            n2 = glm::mix(n2, glm::simplex(scale * glm::vec3(pos1.x, pos1.y, pos1.z)), interp.x);
                        }
                        n1 = glm::mix(n1, n2, interp.y);
                    }
                    noise = glm::mix(noise, n1, interp.z);
                }
            }
            noiseOut[(i*widthY + j)*widthZ+k] += influence * noise;
        }
        influence *= 0.5f;
        scale *= 2.0f;
    }

    float maxval = 0.0;
    for (int i = 0; i < widthX*widthY*widthZ; ++i) maxval = std::max(std::abs(noiseOut[i]), maxval);
    if (maxval > 1.0f) {
        for (int i = 0; i < widthX*widthY*widthZ; ++i) noiseOut[i] /= maxval;
    }
}

static void worley(int widthX, int widthY, int widthZ, int cellsX, int cellsY, int cellsZ, float* noiseOut) {
    std::vector<glm::vec3> cellPoints(cellsX*cellsY*cellsZ);
    glm::vec3 cellDimensions = glm::vec3(widthX, widthY, widthZ) / glm::vec3(cellsX, cellsY, cellsZ);
    for (int i = 0; i < cellsX; ++i) for (int j = 0; j < cellsY; ++j) for (int k = 0; k < cellsZ; ++k) {
        cellPoints[(i*cellsY + j)*cellsZ + k] = glm::vec3(i, j, k) * cellDimensions + glm::linearRand(glm::vec3(0.0), cellDimensions);
    }
    glm::ivec3 iCells(cellsX, cellsY, cellsZ);
    glm::vec3 width(widthX, widthY, widthZ);
    for (int i = 0; i < widthX; ++i) for (int j = 0; j < widthY; ++j) for (int k = 0; k < widthZ; ++k) {
        float sqdist = 0.0f;
        glm::ivec3 cell = glm::ivec3(glm::vec3(i, j, k) / cellDimensions);

        for (int ii = -1; ii <= 1; ++ii) for (int jj = -1; jj <= 1; ++jj) for (int kk = -1; kk <= 1; ++kk) {
            glm::ivec3 ccell = cell + glm::ivec3(ii, jj, kk);

            // wrap at borders to create seamless texture
            glm::ivec3 cmp = glm::ivec3(glm::greaterThanEqual(ccell, iCells)) - glm::ivec3(glm::lessThan(ccell, glm::ivec3(0)));
            ccell -= cmp * iCells;
            glm::vec3 offset = glm::vec3(cmp) * width;

            glm::vec3 v = (cellPoints[(ccell.x*cellsY + ccell.y)*cellsZ + ccell.z] + offset - glm::vec3(i, j, k)) / (cellDimensions);
            float d2 = glm::dot(v, v);
            if ((ii == -1 && jj == -1 && kk == -1) || d2 < sqdist) {
                sqdist = d2;
            }
        }
        noiseOut[(i*widthY + j)*widthZ + k] = glm::sqrt(sqdist);
    }
}

static void worley(int widthX, int widthY, int cellsX, int cellsY, float* noiseOut) {
    std::vector<glm::vec2> cellPoints(cellsX*cellsY);
    glm::vec2 cellDimensions = glm::vec2(widthX, widthY) / glm::vec2(cellsX, cellsY);
    for (int i = 0; i < cellsX; ++i) for (int j = 0; j < cellsY; ++j) {
        cellPoints[i*cellsY + j] = glm::vec2(i, j) * cellDimensions + glm::linearRand(glm::vec2(0.0), cellDimensions);
    }
    glm::ivec2 iCells(cellsX, cellsY);
    glm::vec2 width(widthX, widthY);
    for (int i = 0; i < widthX; ++i) for (int j = 0; j < widthY; ++j) {
        float sqdist = 0.0f;
        glm::ivec2 cell = glm::ivec2(glm::vec2(i, j) / cellDimensions);

        // 3x3 neighborhood of the cell, one of these cells has the closest sample point
        for (int ii = -1; ii <= 1; ++ii) for (int jj = -1; jj <= 1; ++jj) {
            glm::ivec2 ccell = cell + glm::ivec2(ii, jj);

            // wrap at borders to create seamless texture
            glm::ivec2 cmp = glm::ivec2(glm::greaterThanEqual(ccell, iCells)) - glm::ivec2(glm::lessThan(ccell, glm::ivec2(0)));
            ccell -= cmp * iCells;
            glm::vec2 offset = glm::vec2(cmp) * width;

            glm::vec2 v = (cellPoints[ccell.x*cellsY + ccell.y] + offset - glm::vec2(i, j)) / (cellDimensions);
            float d2 = glm::dot(v, v);
            if ((ii == -1 && jj == -1) || d2 < sqdist) {
                sqdist = d2;
            }
        }

        // write nearest distance
        noiseOut[i*widthY + j] = glm::sqrt(sqdist);
    }
}

static void worley3DCube(int width, int cells, float* noiseOut) {
    return worley(width, width, width, cells, cells, cells, noiseOut);
}

static void fbm3DCube(int width, int octaves, float scale, float tileBlend, bool tileXZ, bool tileY, float* noiseOut) {
    return fbm(width, width, width, octaves, scale, tileBlend, tileXZ, tileY, tileXZ, noiseOut);
}

static void quantizeFloats(const float* floatsIn, unsigned char* ucharsOut, int n) {
    for (int i = 0; i < n; ++i) ucharsOut[i] = static_cast<unsigned char>(glm::clamp((0.5f + 0.5f * floatsIn[i]) * 255, 0.0f, 255.0f));
}

static void interleaveBytes(const unsigned char** pUCharsIn, unsigned char* uCharsOut, int nPerInputBuffer, int nInputBuffers) {
    for (int i = 0; i < nPerInputBuffer; ++i) for (int j = 0; j < nInputBuffers; ++j) {
        uCharsOut[i*nInputBuffers+j] = pUCharsIn[j][i];
    }
}

static void createNoiseTextures(uint32_t baseNoiseSize, uint32_t baseNoiseChannels,
                                uint32_t detailNoiseSize, uint32_t detailNoiseChannels,
                                int baseNoiseOctaves, float baseNoiseFrequency, int* baseNoiseWorleyPoints,
                                int* detailNoiseOctaves, float* detailNoiseFrequency,
                                std::vector<unsigned char>& baseNoise, std::vector<unsigned char>& detailNoise) {
    baseNoise.resize(
        baseNoiseSize * baseNoiseSize * baseNoiseSize * baseNoiseChannels);

    detailNoise.resize(
        detailNoiseSize * detailNoiseSize * detailNoiseSize * detailNoiseChannels);

    std::vector<float> baseNoiseF(
        baseNoiseSize * baseNoiseSize * baseNoiseSize,
        0.0);

    std::vector<unsigned char> fbmnoise(baseNoiseF.size());

    fbm3DCube(baseNoiseSize, baseNoiseOctaves, baseNoiseFrequency, 0.25, true, false, baseNoiseF.data());

    quantizeFloats(baseNoiseF.data(), fbmnoise.data(), (int) baseNoiseF.size());

    std::vector<std::vector<unsigned char>> worleyNoise(
        baseNoiseChannels - 1,
        std::vector<unsigned char>(baseNoiseF.size()));

    std::vector<const unsigned char*> baseNoiseBuffers(baseNoiseChannels);

    baseNoiseBuffers[0] = fbmnoise.data();

    for (auto i = 0u; i < baseNoiseChannels-1; ++i) {
        std::vector<float> worleyNoiseF(baseNoiseF.size());

        worley3DCube(baseNoiseSize, baseNoiseWorleyPoints[i], worleyNoiseF.data());

        quantizeFloats(worleyNoiseF.data(), worleyNoise[i].data(), (int) worleyNoiseF.size());

        baseNoiseBuffers[i+1] = worleyNoise[i].data();
    }

    interleaveBytes(baseNoiseBuffers.data(), baseNoise.data(), (int) baseNoiseF.size(), (int) baseNoiseChannels);

    std::vector<std::vector<unsigned char>> detailNoiseLayers(
        detailNoiseChannels,
        std::vector<unsigned char>(detailNoiseSize * detailNoiseSize * detailNoiseSize));

    std::vector<const unsigned char*> detailNoiseBuffers(detailNoiseChannels);

    for (auto i = 0u; i < detailNoiseChannels; ++i) {
        std::vector<float> detailNoiseF(detailNoiseLayers[i].size(), 0.0f);

        fbm3DCube(detailNoiseSize, detailNoiseOctaves[i], detailNoiseFrequency[i], 0.25, true, false, detailNoiseF.data());

        quantizeFloats(detailNoiseF.data(), detailNoiseLayers[i].data(), (int) detailNoiseF.size());

        detailNoiseBuffers[i] = detailNoiseLayers[i].data();
    }

    interleaveBytes(detailNoiseBuffers.data(), detailNoise.data(), (int) detailNoiseLayers[0].size(), (int) detailNoiseChannels);
}

void VolumetricCloudsPass::init() {
    m_cloudsShader.linkShaderFiles("shaders/vertex_fs.glsl", "shaders/fragment_background.glsl");
    m_accumShader.linkShaderFiles("shaders/vertex_fs.glsl", "shaders/fragment_clouds_accum.glsl");

    TextureParameters params = {};
    params.numComponents = 4;
    params.useFloatComponents = true;
    params.useLinearFiltering = true;
    params.useEdgeClamping = true;

    m_renderTexture.setParameters(params);

    m_historyTextures[0].setParameters(params);
    m_historyTextures[1].setParameters(params);

    bool needGenNoise = false;
    std::ifstream filestr("data/cloudnoise.bin", std::ios::binary);
    if (filestr.is_open()) {
        std::vector<unsigned char> raw(std::istreambuf_iterator<char>(filestr), {});

        size_t baseNoiseRawSize = baseNoiseTextureSize *
                                  baseNoiseTextureSize *
                                  baseNoiseTextureSize *
                                  baseNoiseTextureChannels;
        size_t detailNoiseRawSize = detailNoiseTextureSize *
                                    detailNoiseTextureSize *
                                    detailNoiseTextureSize *
                                    detailNoiseTextureChannels;

        if (raw.size() == baseNoiseRawSize + detailNoiseRawSize) {
            TextureParameters param = {};
            param.is3D = true;
            param.useLinearFiltering = true;

            param.width = baseNoiseTextureSize;
            param.height = baseNoiseTextureSize;
            param.arrayLayers = baseNoiseTextureSize;
            param.numComponents = baseNoiseTextureChannels;

            m_baseNoiseTexture.setParameters(param);
            m_baseNoiseTexture.allocateData(&raw[0]);

            param.width = detailNoiseTextureSize;
            param.height = detailNoiseTextureSize;
            param.arrayLayers = detailNoiseTextureSize;
            param.numComponents = detailNoiseTextureChannels;

            m_detailNoiseTexture.setParameters(param);
            m_detailNoiseTexture.allocateData(&raw[baseNoiseRawSize]);
        } else {
            needGenNoise = true;
        }
    } else {
        needGenNoise = true;
    }
    if (needGenNoise) {
        std::vector<unsigned char> baseNoise, detailNoise;
        createNoiseTextures(baseNoiseTextureSize, baseNoiseTextureChannels,
                            detailNoiseTextureSize, detailNoiseTextureChannels,
                            baseNoiseOctaves, baseNoiseFrequency, baseNoiseWorleyPoints,
                            detailNoiseOctaves, detailNoiseFrequency,
                            baseNoise, detailNoise);

        TextureParameters param = {};
        param.is3D = true;
        param.useLinearFiltering = true;

        param.width = baseNoiseTextureSize;
        param.height = baseNoiseTextureSize;
        param.arrayLayers = baseNoiseTextureSize;
        param.numComponents = baseNoiseTextureChannels;

        m_baseNoiseTexture.setParameters(param);
        m_baseNoiseTexture.allocateData(baseNoise.data());

        param.width = detailNoiseTextureSize;
        param.height = detailNoiseTextureSize;
        param.arrayLayers = detailNoiseTextureSize;
        param.numComponents = detailNoiseTextureChannels;

        m_detailNoiseTexture.setParameters(param);
        m_detailNoiseTexture.allocateData(detailNoise.data());

        std::ofstream filestr("data/cloudnoise.bin", std::ios::binary|std::ios::trunc);
        filestr.write((const char*)baseNoise.data(), baseNoise.size());
        filestr.write((const char*)detailNoise.data(), detailNoise.size());
    }

}

void VolumetricCloudsPass::onViewportResize(uint32_t width, uint32_t height) {
    m_viewportWidth = width;
    m_viewportHeight = height;

    m_renderTextureWidth  = (width %4u==0u) ? width /4u : width /4u + 1u;
    m_renderTextureHeight = (height%4u==0u) ? height/4u : height/4u + 1u;

    TextureParameters params = m_renderTexture.getParameters();
    params.width = m_renderTextureWidth;
    params.height = m_renderTextureHeight;

    m_renderTexture.setParameters(params);
    m_renderTexture.allocateData(nullptr);

    m_renderLayer.setTextureAttachment(0, &m_renderTexture);

    params = m_historyTextures[0].getParameters();
    params.width = width;
    params.height = height;

    m_historyTextures[0].setParameters(params);
    m_historyTextures[1].setParameters(params);
    m_historyTextures[0].allocateData(nullptr);
    m_historyTextures[1].allocateData(nullptr);

    m_accumRenderLayer.setTextureAttachment(0, &m_historyTextures[0]);
    m_accumRenderLayer.setTextureAttachment(1, &m_historyTextures[1]);

    m_historyValid = false;
}

void VolumetricCloudsPass::setState() {
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);

    glDisable(GL_STENCIL_TEST);

    m_renderLayer.setEnabledDrawTargets({0});
    m_renderLayer.bind();
    glViewport(0, 0, m_renderTextureWidth, m_renderTextureHeight);
    glClear(GL_COLOR_BUFFER_BIT);
}

void VolumetricCloudsPass::render() {
    // bayerCoords[i] = {x, y} where bayer4x4[x][y] = i
    static const glm::vec2 bayerCoords[16] = {
        {0, 0}, {2, 2}, {2, 0}, {0, 2},
        {1, 1}, {3, 3}, {3, 1}, {1, 3},
        {1, 0}, {3, 2}, {3, 0}, {1, 2},
        {0, 1}, {2, 3}, {2, 1}, {0, 3}
    };

    m_cloudsShader.bind();
    m_cloudsShader.setUniform("halfScreenSize", m_cameraHalfScreenSize);
    m_cloudsShader.setUniform("inverseView", m_viewInverse);
    m_cloudsShader.setUniform("sunDirection", m_lightDirection);
    m_cloudsShader.setUniform("sunIntensity", m_lightIntensity);
    m_cloudsShader.setUniform("worldPos", m_cameraPosition);

    m_cloudsShader.setUniform("pixelOffset", bayerCoords[m_cOffsetIndex] / glm::vec2(m_viewportWidth, m_viewportHeight));
    m_cloudsShader.setUniform("time", static_cast<float>(Timer::getTime()));

    m_cloudsShader.setUniform("cloudiness", cloudiness);
    m_cloudsShader.setUniform("cloudDensity", density);
    m_cloudsShader.setUniform("fExtinction", extinction);
    m_cloudsShader.setUniform("fScattering", scattering);
    m_cloudsShader.setUniform("cloudNoiseBaseScale", baseNoiseScale);
    m_cloudsShader.setUniform("cloudNoiseDetailScale", detailNoiseScale);
    m_cloudsShader.setUniform("detailNoiseInfluence", detailNoiseInfluence);
    m_cloudsShader.setUniform("rPlanet", planetRadius);
    m_cloudsShader.setUniform("cloudBottom", cloudBottomHeight);
    m_cloudsShader.setUniform("cloudTop", cloudTopHeight);
    m_cloudsShader.setUniform("cloudSteps", steps);
    m_cloudsShader.setUniform("shadowSteps", shadowSteps);
    m_cloudsShader.setUniform("shadowDist", shadowStepSize);
    m_cloudsShader.setUniform("baseNoiseContribution", baseNoiseContribution);
    m_cloudsShader.setUniform("baseNoiseThreshold", baseNoiseThreshold);

    m_baseNoiseTexture.bind(0);
    m_cloudsShader.setUniform("cloudNoiseBase", 0);

    m_detailNoiseTexture.bind(1);
    m_cloudsShader.setUniform("cloudNoiseDetail", 1);

    FullscreenQuad::draw();

    if (m_historyValid) {
        m_accumRenderLayer.setEnabledDrawTargets({m_cHistoryIndex});
        m_accumRenderLayer.bind();

        glViewport(0, 0, m_viewportWidth, m_viewportHeight);
        glClear(GL_COLOR_BUFFER_BIT);

        m_accumShader.bind();
        m_accumShader.setUniform("frame", m_cOffsetIndex);
        m_accumShader.setUniform("currentFrame", 0);
        m_accumShader.setUniform("history", 1);
        m_accumShader.setUniform("motionBuffer", 2);

        m_renderTexture.bind(0);
        m_historyTextures[(m_cHistoryIndex+1)%2].bind(1);
        m_pMotionBuffer->bind(2);

        FullscreenQuad::draw();
    } else {
        m_accumRenderLayer.setEnabledDrawTargets({m_cHistoryIndex});
        m_renderLayer.setEnabledReadTarget(0);

        m_accumRenderLayer.bind(GL_DRAW_FRAMEBUFFER);
        m_renderLayer.bind(GL_READ_FRAMEBUFFER);

        glBlitFramebuffer(0, 0, m_renderTextureWidth, m_renderTextureHeight,
                          0, 0, m_viewportWidth, m_viewportHeight,
                          GL_COLOR_BUFFER_BIT, GL_LINEAR);

        m_historyValid = true;
    }

    // draw the clouds onto the scene texture, but do it "underneath"
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE_MINUS_DST_ALPHA, GL_DST_ALPHA);

    m_pOutputRenderLayer->setEnabledDrawTargets({0});
    m_pOutputRenderLayer->bind();
    glViewport(0, 0, m_viewportWidth, m_viewportHeight);

    FullscreenQuad::drawTextured(&m_historyTextures[m_cHistoryIndex]);

    m_cOffsetIndex = (m_cOffsetIndex+1)%16u;
    m_cHistoryIndex = (m_cHistoryIndex+1)%2u;
}

void VolumetricCloudsPass::cleanup() {
    m_pMotionBuffer = nullptr;
    m_pOutputRenderLayer = nullptr;
}

void VolumetricCloudsPass::setDirectionalLight(const glm::vec3& intensity, const glm::vec3& direction) {
    m_lightDirection = direction;
    m_lightIntensity = intensity;
}

void VolumetricCloudsPass::setInverseViewMatrix(const glm::mat4& inverseView) {
    m_viewInverse = inverseView;
}

void VolumetricCloudsPass::setCamera(const Camera* pCamera) {
    m_cameraPosition = pCamera->getPosition();
    m_cameraHalfScreenSize = glm::vec2(std::tan(0.5f * pCamera->getFOV()));
    m_cameraHalfScreenSize.x *= pCamera->getAspectRatio();
}

void VolumetricCloudsPass::setMotionBuffer(Texture* pMotionBuffer) {
    m_pMotionBuffer = pMotionBuffer;
}

void VolumetricCloudsPass::setOutputRenderTarget(RenderLayer* pRenderLayer) {
    m_pOutputRenderLayer = pRenderLayer;
}
