#include "skeleton.h"

Skeleton::Skeleton(const SkeletonDescription* pDescription) :
    m_pDescription(pDescription) {
    m_joints.resize(m_pDescription->getNumJoints());
    for (size_t i = 0; i < m_joints.size(); ++i) {
        const SkeletonDescription::JointInfo& jointInfo = m_pDescription->getJointInfo(i);
        m_joints[i].setIndex(i).setName(jointInfo.name);

        if (jointInfo.parent != SkeletonDescription::JOINT_NONE) {
            m_joints[i].setParent(&m_joints[jointInfo.parent]);
        }

        m_joints[i].m_offsetBind        = jointInfo.bindOffset;
        m_joints[i].m_rotationBind      = jointInfo.bindRotation;
        m_joints[i].m_matrixBind        = jointInfo.bindMatrix;
        m_joints[i].m_matrixBindInverse = jointInfo.inverseBindMatrix;
        m_joints[i].m_matrixBindLocal   = jointInfo.bindMatrixLocal;
    }
    m_skinningMatrices.resize(m_pDescription->getNumJoints(), glm::mat4(1.0f));
    m_lastSkinningMatrices.resize(m_pDescription->getNumJoints(), glm::mat4(1.0f));
}

Skeleton::~Skeleton() {}

const Joint* Skeleton::getJoint(size_t i) const {
    if(i >= m_joints.size()) return nullptr;
    return &m_joints[i];
}

const Joint* Skeleton::getJoint(const std::string& name) const {
    size_t index = m_pDescription->getJointIndex(name);
    if (index != SkeletonDescription::JOINT_NONE) {
        return &m_joints[m_pDescription->getJointIndex(name)];
    }
    return nullptr;
}

Joint* Skeleton::getJoint(size_t i) {
    if(i >= m_joints.size()) return nullptr;
    return &m_joints[i];
}

Joint* Skeleton::getJoint(const std::string& name) {
    size_t index = m_pDescription->getJointIndex(name);
    if (index != SkeletonDescription::JOINT_NONE) {
        return &m_joints[m_pDescription->getJointIndex(name)];
    }
    return nullptr;
}


void Skeleton::clearPose() {
    for(Joint& j : m_joints) {
        j.clearLocalPose();
    }
}

void Skeleton::setPose(const SkeletonPose& pose) {
    assert (m_pDescription == pose.getSkeletonDescription());
    for (size_t i = 0; i < m_joints.size(); ++i) {
        m_joints[i].setLocalPoseOffset(pose.getJointOffset(i));
        m_joints[i].setLocalPoseRotation(pose.getJointRotation(i));
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
