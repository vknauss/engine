#ifndef SHADOW_MAP_FILTER_PASS_H_INCLUDED
#define SHADOW_MAP_FILTER_PASS_H_INCLUDED

#include "core/render/render_layer.h"
#include "core/render/render_pass.h"
#include "core/render/shader.h"

class ShadowMapFilterPass : public RenderPass {

public:

private:

    Shader m_horizontalFilterShader, m_verticalFilterShader;

    RenderLayer m_renderLayer;
    Texture m_filterTextures[2];

    uint32_t m_filterTextureWidth;

};

#endif // SHADOW_MAP_FILTER_PASS_H_INCLUDED
