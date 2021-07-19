#include "core/resources/skeleton_description.h"

size_t SkeletonDescription::createJoint(const std::string& name) {
    JointInfo info;
    info.name = name;
    info.index = m_jointInfos.size();
    info.parent = JOINT_NONE;

    m_jointInfos.push_back(info);
    m_jointNameMap[name] = info.index;

    return info.index;
}

void SkeletonDescription::fixRelativeOrder() {
    std::vector<JointInfo> newJointInfos(m_jointInfos.size());
    std::vector<bool> inserted(m_jointInfos.size(), false);
    std::vector<size_t> newIndex(m_jointInfos.size(), 0);
    size_t c_index = 0;
    while (c_index < m_jointInfos.size()) {
        for (size_t i = 0; i < m_jointInfos.size(); ++i) {
            if (inserted[i]) continue;
            if (m_jointInfos[i].parent == JOINT_NONE || inserted[m_jointInfos[i].parent]) {
                newJointInfos[c_index] = m_jointInfos[i];
                newJointInfos[c_index].index = c_index;
                newIndex[i] = c_index;
                inserted[i] = true;
                newJointInfos[c_index].parent = newIndex[m_jointInfos[i].parent];
                ++c_index;
            }
        }
    }
}

void SkeletonDescription::computeBindTransforms() {
    for (size_t i = 0; i < m_jointInfos.size(); ++i) {
        if (m_jointInfos[i].bindAsMatrix) {
            if (m_jointInfos[i].parent != JOINT_NONE) {
                m_jointInfos[i].bindMatrixLocal = m_jointInfos[m_jointInfos[i].parent].inverseBindMatrix * m_jointInfos[i].bindMatrix;
            } else {
                m_jointInfos[i].bindMatrixLocal = m_jointInfos[i].bindMatrix;
            }
        }
        if (m_jointInfos[i].bindAsMatrix || m_jointInfos[i].bindAsMatrixLocal) {
            m_jointInfos[i].bindOffset = glm::vec3(m_jointInfos[i].bindMatrixLocal[3]);
            m_jointInfos[i].bindRotation = glm::quat_cast(glm::mat3(m_jointInfos[i].bindMatrixLocal));
        } else {
            m_jointInfos[i].bindMatrixLocal = glm::translate(glm::mat4(1.0f), m_jointInfos[i].bindOffset) * glm::mat4(glm::mat3_cast(m_jointInfos[i].bindRotation));
        }
        if (!m_jointInfos[i].bindAsMatrix) {
            if(m_jointInfos[i].parent != JOINT_NONE) {
                m_jointInfos[i].bindMatrix = m_jointInfos[m_jointInfos[i].parent].bindMatrix * m_jointInfos[i].bindMatrixLocal;
            } else {
                m_jointInfos[i].bindMatrix = m_jointInfos[i].bindMatrixLocal;
            }
        }
        m_jointInfos[i].inverseBindMatrix = glm::inverse(m_jointInfos[i].bindMatrix);
    }
}

bool SkeletonDescription::validate() const {
    std::map<std::string, bool> nameMap;
    for (const JointInfo& info : m_jointInfos) {
        if (nameMap[info.name] || (info.parent != JOINT_NONE && info.parent > info.index)) return false;
        nameMap[info.name] = true;
    }
    return true;
}
