#ifndef FRUSTUM_CULLER_H_
#define FRUSTUM_CULLER_H_

#include <vector>

#include <glm/glm.hpp>

#include "bounding_sphere.h"
#include "scene.h"

class FrustumCuller {

    friend class InstanceListBuilder;

    std::vector<BoundingSphere> m_boundingSpheres;
    std::vector<bool> m_cullResults;

public:

    size_t cullSpheres(const BoundingSphere* pBoundingSpheres, size_t count, const glm::mat4& frustumMatrix);

    size_t cullSceneRenderables(const Scene* pScene, const glm::mat4& frustumMatrix);

    const std::vector<bool>& getCullResults() const {
        return m_cullResults;
    }

};

#endif // FRUSTUM_CULLER_H_
