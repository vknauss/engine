#ifndef RENDERABLE_H_
#define RENDERABLE_H_

#include <glm/glm.hpp>

#include "model.h"
#include "lod_component.h"

class Renderable {

public:

    Renderable();

    ~Renderable();

    Renderable& setModel(Model* pModel);

    Renderable& setLODComponent(LODComponent* pLODComponent);

    Renderable& setSkeleton(Skeleton* pSkeleton);

    Renderable& setWorldTransform(const glm::mat4& worldTransform);

    const glm::mat4& getWorldTransform() const {
        return m_worldTransform;
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

private:

    Model* m_pModel;
    LODComponent* m_pLODComponent;
    Skeleton* m_pSkeleton;

    glm::mat4 m_worldTransform;
};

#endif // RENDERABLE_H_
