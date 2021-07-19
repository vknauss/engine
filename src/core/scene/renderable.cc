#include "renderable.h"

#include <algorithm>

Renderable::Renderable() :
    m_pModel(nullptr),
    m_pLODComponent(nullptr),
    m_pSkeleton(nullptr),
    m_worldTransform(glm::mat4(1.0))
{
}

Renderable::~Renderable() {
}

Renderable& Renderable::setModel(const Model* pModel) {
    m_pModel = pModel;

    m_pLODComponent = nullptr;  // ensure no renderable has both

    return *this;
}

Renderable& Renderable::setLODComponent(const LODComponent* pLODComponent) {
    m_pLODComponent = pLODComponent;

    m_pModel = nullptr;

    return *this;
}

Renderable& Renderable::setSkeleton(Skeleton* pSkeleton) {
    m_pSkeleton = pSkeleton;

    return *this;
}

Renderable& Renderable::setWorldTransform(const glm::mat4& worldTransform) {
    m_worldTransform = worldTransform;

    return *this;
}

BoundingSphere Renderable::computeWorldBoundingSphere() const {
    BoundingSphere b = {glm::vec3(0), 0.0f};
    float scale = std::sqrt(std::max({
        glm::dot(m_worldTransform[0], m_worldTransform[0]),
        glm::dot(m_worldTransform[1], m_worldTransform[1]),
        glm::dot(m_worldTransform[2], m_worldTransform[2])}));
    if (!m_pSkeleton) {
        b = { glm::vec3(m_worldTransform * glm::vec4(m_pModel->getBoundingSphere().position, 1.0f)),
              scale * m_pModel->getBoundingSphere().radius };
    } else {
        const std::vector<BoundingSphere>& jointBoundingSpheres = m_pModel->getJointBoundingSpheres();
        for (size_t i = 0; i < jointBoundingSpheres.size(); ++i) {
            glm::mat4 jointMatrix = m_worldTransform * m_pSkeleton->getSkinningMatrices()[i];
            BoundingSphere b1 = {
                glm::vec3(jointMatrix * glm::vec4(jointBoundingSpheres[i].position, 1.0f)),
                scale * jointBoundingSpheres[i].radius };
            if (i == 0) b = b1;
            else b.add(b1);
        }
    }
    return b;
}