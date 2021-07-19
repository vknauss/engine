#ifndef OBJECT_MOTION_VECTORS_PASS_H
#define OBJECT_MOTION_VECTORS_PASS_H

#include "geometry_render_pass.h"

#include "core/render/render_layer.h"


class ObjectMotionVectorsPass : public GeometryRenderPass {

public:

    void onViewportResize(uint32_t width, uint32_t height);

    void setState() override;

    void setRenderTarget(RenderLayer* pRenderLayer);

protected:

    bool loadShaders() override;

    InstanceListBuilder::filterPredicate getFilterPredicate() const override {
        return [] (const Model* pModel) { return pModel->getMaterial() && pModel->getMaterial()->useObjectMotionVectors(); };
    }

    bool useLastFrameMatrix() const override {
        return true;
    }

private:

    RenderLayer* m_pRenderLayer;

    uint32_t m_viewportWidth, m_viewportHeight;

};

#endif // OBJECT_MOTION_VECTORS_PASS_H
