#ifndef MODEL_H_
#define MODEL_H_

#include "material.h"
#include "mesh.h"
#include "core/animation/skeleton.h"
#include "core/scene/bounding_sphere.h"

class Model {

public:

    Model();

    Model(Mesh* pMesh, Material* pMaterial, SkeletonDescription* pSkeletonDesc, const BoundingSphere& boundingSphere);

    ~Model();

    Model& setMesh(Mesh* pMesh);

    Model& setMaterial(Material* pMaterial);

    Model& setSkeletonDescription(SkeletonDescription* pSkeletonDesc);

    Model& setBoundingSphere(const BoundingSphere& b);

    Model& setJointBoundingSpheres(const std::vector<BoundingSphere>& boundingSpheres);

    Mesh* getMesh() {
        return m_pMesh;
    }

    Material* getMaterial() {
        return m_pMaterial;
    }

    SkeletonDescription* getSkeletonDescription() {
        return m_pSkeletonDesc;
    }

    const Mesh* getMesh() const {
        return m_pMesh;
    }

    const Material* getMaterial() const {
        return m_pMaterial;
    }

    const SkeletonDescription* getSkeletonDescription() const {
        return m_pSkeletonDesc;
    }

    const BoundingSphere& getBoundingSphere() const {
        return m_boundingSphere;
    }

    const std::vector<BoundingSphere>& getJointBoundingSpheres() const {
        return m_jointBoundingSpheres;
    }

private:

    Mesh* m_pMesh;
    Material* m_pMaterial;
    SkeletonDescription* m_pSkeletonDesc;

    BoundingSphere m_boundingSphere;
    std::vector<BoundingSphere> m_jointBoundingSpheres;

};

#endif // MODEL_H_
