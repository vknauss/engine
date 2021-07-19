#ifndef RENDERABLE_H_
#define RENDERABLE_H_

#include <glm/glm.hpp>

#include "core/resources/model.h"
#include "core/resources/lod_component.h"

class Renderable {

public:

    Renderable();

    ~Renderable();

    Renderable& setModel(const Model* pModel);

    Renderable& setLODComponent(const LODComponent* pLODComponent);

    Renderable& setSkeleton(Skeleton* pSkeleton);

    Renderable& setWorldTransform(const glm::mat4& worldTransform);

    Renderable& copyLastTransform() {
        m_lastWorldTransform = m_worldTransform;
        return *this;
    }

    const glm::mat4& getWorldTransform() const {
        return m_worldTransform;
    }

    const glm::mat4& getLastWorldTransform() const {
        return m_lastWorldTransform;
    }

    BoundingSphere computeWorldBoundingSphere() const;

    const Model* const getModel() const {
        return m_pModel;
    }

    const LODComponent* const getLODComponent() const {
        return m_pLODComponent;
    }

    const Skeleton* const getSkeleton() const {
        return m_pSkeleton;
    }

    Skeleton* getSkeleton() {
        return m_pSkeleton;
    }

private:

    const Model* m_pModel;
    const LODComponent* m_pLODComponent;
    Skeleton* m_pSkeleton;

    glm::mat4 m_worldTransform;
    glm::mat4 m_lastWorldTransform;
};

namespace Component {

struct Renderable {

    Model* pModel;
    Skeleton* pSkeleton;

    struct SkeletalFlag {};
};

}

#endif // RENDERABLE_H_
