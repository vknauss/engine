#ifndef SHADOW_CASCADE_PASS_H_INCLUDED
#define SHADOW_CASCADE_PASS_H_INCLUDED

#include "geometry_render_pass.h"

#include "core/render/render_layer.h"

// A pass for rendering a single cascaded-shadow-maps cascade layer to an array texture
// really dummy simple
class ShadowCascadePass : public GeometryRenderPass {

public:

    void setState() override;

    void setTextureSize(uint32_t textureSize);

    void setLayer(uint32_t layer);

    void setRenderTarget(RenderLayer* pRenderLayer, Texture* pDepthArrayTexture);

protected:

    InstanceListBuilder::filterPredicate getFilterPredicate() const override {
        return [] (const Model* pModel) { return !pModel->getMaterial() || pModel->getMaterial()->isShadowCastingEnabled(); };
    }

private:

    uint32_t m_textureSize = 0;
    uint32_t m_layer = 0;

    RenderLayer* m_pRenderLayer = nullptr;
    Texture* m_pDepthArrayTexture = nullptr;

};

#endif // SHADOW_CASCADE_PASS_H_INCLUDED
