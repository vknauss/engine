#ifndef SKELETON_DESCRIPTION_H_INCLUDED
#define SKELETON_DESCRIPTION_H_INCLUDED

#include <map>
#include <string>
#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

class SkeletonDescription {

public:

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

    static constexpr size_t JOINT_NONE = std::numeric_limits<size_t>::max();

    size_t createJoint(const std::string& name);

    // Use these methods to create the skeleton hierarchy
    void setParent(size_t joint, size_t parent) {
        m_jointInfos[joint].parent = parent;
    }

    // setBind* are used to indicate the bind(rest) pose of the skeleton
    // The bind pose may be specified as a local (parent-relative) rotation
    // and offset using setBindRotation and setBindOffset,
    // as a local-space transformation matrix using setBindMatrixLocal
    // or as a global(model-space) transformation matrix using setBindMatrix
    // If none of these are called, the bind pose will default to an identity
    // quaternion and zero vector, i.e. an identity transformation
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

    // Set the name for the skeleton description
    void setName(const std::string& name) {
        m_name = name;
    }

    const JointInfo& getJointInfo(size_t joint) const {
        return m_jointInfos[joint];
    }

    size_t getJointIndex(const std::string& name) const {
        if (m_jointNameMap.count(name)) {
            return m_jointNameMap.at(name);
        } else {
            return JOINT_NONE;
        }
    }

    size_t getNumJoints() const {
        return m_jointInfos.size();
    }

    const std::string& getName() const {
        return m_name;
    }

    // This will infinite loop if the joint hierarchy contains a loop. Avoid this.
    void fixRelativeOrder();

    // Call this after creating all joints and for each calling one of:
    //  - Both setBindRotation and setBindOffset (or only setBindRotation if offset should be zero)
    //  - setBindMatrix with a World-space matrix
    //  - setBindMatrixLocal with a Local(Parent)-space matrix
    void computeBindTransforms();

    // Check for name conflicts and out-of-bounds parent indices
    bool validate() const;

private:

    std::map<std::string, size_t> m_jointNameMap;

    std::vector<JointInfo> m_jointInfos;

    std::string m_name;

};

#endif // SKELETON_DESCRIPTION_H_INCLUDED
