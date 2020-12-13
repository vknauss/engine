#include "frustum_culler.h"

#include <algorithm>

size_t FrustumCuller::cullSpheres(const BoundingSphere* pBoundingSpheres, size_t count, const glm::mat4& frustumMatrix) {
    glm::mat4 fmt = glm::transpose(frustumMatrix);

    struct Plane {
        glm::vec3 normal;
        float offset;
    } frustumPlanes[6];

    for(int i = 0; i < 6; ++i) {
        glm::vec4 v = ((i%2)*2.0f-1.0f) * fmt[i/2] + fmt[3];
        float l = glm::length(glm::vec3(v));
        v /= l;
        frustumPlanes[i].normal = glm::vec3(v);
        frustumPlanes[i].offset = v[3];
    }

    size_t numPassed = count;
    m_cullResults.resize(count);
    for(size_t i = 0; i < count; ++i) {
        m_cullResults[i] = true;
        for(const Plane& p : frustumPlanes) {
            if(glm::dot(p.normal, pBoundingSpheres[i].getPosition()) + p.offset < -pBoundingSpheres[i].getRadius()) {
                m_cullResults[i] = false;
                --numPassed;
                break;
            }
        }
    }

    return numPassed;
}

size_t FrustumCuller::cullSceneRenderables(const Scene* pScene, const glm::mat4& frustumMatrix) {
    m_boundingSpheres.resize(pScene->m_renderables.size());
    m_cullResults.assign(m_boundingSpheres.size(), false);

    std::transform(pScene->m_renderables.begin(), pScene->m_renderables.end(),
                   m_boundingSpheres.begin(),
                   [] (const Renderable& r) { return r.computeWorldBoundingSphere(); });

   return cullSpheres(m_boundingSpheres.data(), m_boundingSpheres.size(), frustumMatrix);
}
