#include "skeleton.h"

Skeleton::Skeleton(const SkeletonDescription* pDescription) :
    m_pDescription(pDescription) {
    m_joints.resize(m_pDescription->getNumJoints());
    for (size_t i = 0; i < m_joints.size(); ++i) {
        m_joints[i].setIndex(i).setName(m_pDescription->getJointName(i));

        if (m_pDescription->getJointParent(i) != SkeletonDescription::JOINT_NONE) {
            m_joints[i].setParent(&m_joints[m_pDescription->getJointParent(i)]);
        }

        m_joints[i].m_offsetBind = m_pDescription->m_jointInfos[i].bindOffset;
        m_joints[i].m_rotationBind = m_pDescription->m_jointInfos[i].bindRotation;
        m_joints[i].m_matrixBind = m_pDescription->m_jointInfos[i].bindMatrix;
        m_joints[i].m_matrixBindInverse = m_pDescription->m_jointInfos[i].inverseBindMatrix;
        m_joints[i].m_matrixBindLocal = m_pDescription->m_jointInfos[i].bindMatrixLocal;
    }
    m_skinningMatrices.resize(m_pDescription->getNumJoints(), glm::mat4(1.0f));
}

Skeleton::~Skeleton() {}

const Joint* Skeleton::getJoint(size_t i) const {
    if(i >= m_joints.size()) return nullptr;
    return &m_joints[i];
}

const Joint* Skeleton::getJoint(std::string name) const {
    return &m_joints[m_pDescription->getJointIndex(name)];
}

Joint* Skeleton::getJoint(size_t i) {
    if(i >= m_joints.size()) return nullptr;
    return &m_joints[i];
}

Joint* Skeleton::getJoint(std::string name) {
    return &m_joints[m_pDescription->getJointIndex(name)];
}


void Skeleton::clearPose() {
    for(Joint& j : m_joints) {
        j.clearLocalPose();
    }
}

void Skeleton::applyCurrentPose() {
    for(Joint& j : m_joints) {
        j.computeWorldMatrix();
    }
}

void Skeleton::computeSkinningMatrices() {
    for (size_t i = 0; i < m_joints.size(); ++i) {
        m_skinningMatrices[i] = m_joints[i].getWorldMatrix() * m_joints[i].getInverseBindMatrix();
    }
}
