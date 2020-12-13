#ifndef SKELETON_H_
#define SKELETON_H_

#include <map>
#include <string>
#include <vector>

#include "joint.h"

class SkeletonDescription {

    friend class Skeleton;

    struct JointInfo {
        std::string name;

        glm::quat bindRotation = {1, 0, 0, 0};
        glm::vec3 bindOffset = {0, 0, 0};
        glm::mat4 bindMatrixLocal;
        glm::mat4 bindMatrix;
        glm::mat4 inverseBindMatrix;

        size_t index;
        size_t parent;

        bool bindAsMatrix = false;
        bool bindAsMatrixLocal = false;
    };

    std::map<std::string, size_t> m_jointNameMap;

    std::vector<JointInfo> m_jointInfos;


public:

    static constexpr size_t JOINT_NONE = std::numeric_limits<size_t>::max();

    size_t createJoint(const std::string& name) {
        JointInfo info;
        info.name = name;
        info.index = m_jointInfos.size();
        info.parent = JOINT_NONE;

        m_jointInfos.push_back(info);
        m_jointNameMap[name] = info.index;

        return info.index;
    }

    void setParent(size_t joint, size_t parent) {
        m_jointInfos[joint].parent = parent;
    }

    void setBindRotation(size_t joint, const glm::quat& rotation) {
        m_jointInfos[joint].bindRotation = rotation;
    }

    void setBindOffset(size_t joint, const glm::vec3& offset) {
        m_jointInfos[joint].bindOffset = offset;
    }

    void setBindMatrix(size_t joint, const glm::mat4& matrix) {
        m_jointInfos[joint].bindMatrix = matrix;
        m_jointInfos[joint].bindAsMatrix = true;
    }

    void setBindMatrixLocal(size_t joint, const glm::mat4& matrix) {
        m_jointInfos[joint].bindMatrixLocal = matrix;
        m_jointInfos[joint].bindAsMatrixLocal = true;
    }

    const glm::quat& getJointBindRotation(size_t joint) const {
        return m_jointInfos[joint].bindRotation;
    }

    const glm::vec3& getJointBindOffset(size_t joint) const {
        return m_jointInfos[joint].bindOffset;
    }

    const glm::mat4& getJointBindMatrix(size_t joint) const {
        return m_jointInfos[joint].bindMatrix;
    }

    const glm::mat4& getJointBindMatrixLocal(size_t joint) const {
        return m_jointInfos[joint].bindMatrixLocal;
    }

    const glm::mat4& getJointInverseBindMatrix(size_t joint) const {
        return m_jointInfos[joint].inverseBindMatrix;
    }

    // This will infinite loop if the joint hierarchy contains a loop. Avoid this.
    void fixRelativeOrder() {
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

    void computeBindTransforms() {
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

    size_t getJointIndex(const std::string& name) const {
        if (m_jointNameMap.count(name)) {
            return m_jointNameMap.at(name);
        } else {
            return JOINT_NONE;
        }
    }

    size_t getJointParent(size_t joint) const {
        return m_jointInfos[joint].parent;
    }

    std::string getJointName(size_t joint) const {
        return m_jointInfos[joint].name;
    }

    size_t getNumJoints() const {
        return m_jointInfos.size();
    }

    // Check for name conflicts and out-of-bounds parent indices
    bool validate() const {
        std::map<std::string, bool> nameMap;
        for (const JointInfo& info : m_jointInfos) {
            if (nameMap[info.name] || (info.parent != JOINT_NONE && info.parent > info.index)) return false;
            nameMap[info.name] = true;
        }
        return true;
    }

};

class Skeleton {

public:

    Skeleton(const SkeletonDescription* pDescription);

    ~Skeleton();

    const Joint* getJoint(size_t i) const;
    const Joint* getJoint(std::string name) const;

    Joint* getJoint(size_t i);
    Joint* getJoint(std::string name);

    void clearPose();

    void applyCurrentPose();

    void computeSkinningMatrices();

    const SkeletonDescription* getDescription() const {
        return m_pDescription;
    }

    const std::vector<glm::mat4>& getSkinningMatrices() const {
        return m_skinningMatrices;
    }

private:

    const SkeletonDescription* m_pDescription;

    std::vector<Joint> m_joints;
    std::vector<glm::mat4> m_skinningMatrices;
};


#endif // SKELETON_H_
