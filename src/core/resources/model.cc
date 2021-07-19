#include "model.h"

Model::Model() :
    m_pMesh(nullptr),
    m_pMaterial(nullptr),
    m_pSkeletonDesc(nullptr),
    m_boundingSphere() {
}

Model::Model(Mesh* pMesh, Material* pMaterial, SkeletonDescription* pSkeletonDesc, const BoundingSphere& boundingSphere) :
    m_pMesh(pMesh),
    m_pMaterial(pMaterial),
    m_pSkeletonDesc(pSkeletonDesc),
    m_boundingSphere(boundingSphere) {
}

Model::~Model() {
}

Model& Model::setMesh(Mesh* pMesh) {
    m_pMesh = pMesh;

    return *this;
}

Model& Model::setMaterial(Material* pMaterial) {
    m_pMaterial = pMaterial;

    return *this;
}

Model& Model::setSkeletonDescription(SkeletonDescription* pSkeletonDesc) {
    m_pSkeletonDesc = pSkeletonDesc;

    return *this;
}

Model& Model::setBoundingSphere(const BoundingSphere& b) {
    m_boundingSphere = b;

    return *this;
}

Model& Model::setJointBoundingSpheres(const std::vector<BoundingSphere>& boundingSpheres) {
    assert(m_pSkeletonDesc && boundingSpheres.size() == m_pSkeletonDesc->getNumJoints());
    m_jointBoundingSpheres.assign(boundingSpheres.begin(), boundingSpheres.end());
    return *this;
}
