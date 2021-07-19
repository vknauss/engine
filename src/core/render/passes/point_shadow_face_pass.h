#ifndef POINT_SHADOW_FACE_PASS_H_INCLUDED
#define POINT_SHADOW_FACE_PASS_H_INCLUDED

#include "geometry_render_pass.h"

#include "core/render/render_layer.h"

class PointShadowFacePass : public GeometryRenderPass {

public:

    void setState() override;

    void setRenderTarget(RenderLayer* pRenderLayer, Texture* pDepthCubeTexture);

    void setTextureSize(uint32_t textureSize);

    void setFace(uint32_t face);

protected:

    InstanceListBuilder::filterPredicate getFilterPredicate() const override {
        return [] (const Model* pModel) { return !pModel->getMaterial() || pModel->getMaterial()->isShadowCastingEnabled(); };
    }

private:

    uint32_t m_textureSize;
    uint32_t m_face;

    RenderLayer* m_pRenderLayer;
    Texture* m_pDepthCubeTexture;

};

#endif // POINT_SHADOW_FACE_PASS_H_INCLUDED
