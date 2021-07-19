#ifndef SKELETON_H_
#define SKELETON_H_

#include <map>
#include <string>
#include <vector>
#include <glm/gtc/matrix_transform.hpp>

#include "joint.h"
#include "core/resources/skeleton_description.h"

class SkeletonPose;

class Skeleton {

public:

    Skeleton(const SkeletonDescription* pDescription);

    ~Skeleton();

    const Joint* getJoint(size_t i) const;
    const Joint* getJoint(const std::string& name) const;

    Joint* getJoint(size_t i);
    Joint* getJoint(const std::string& name);

    void clearPose();

    void setPose(const SkeletonPose& pose);

    void applyCurrentPose();

    void computeSkinningMatrices();

    void copyLastSkinningMatrices() {
        m_lastSkinningMatrices = m_skinningMatrices;
    }

    const SkeletonDescription* getDescription() const {
        return m_pDescription;
    }

    const std::vector<glm::mat4>& getSkinningMatrices() const {
        return m_skinningMatrices;
    }

    const std::vector<glm::mat4>& getLastSkinningMatrices() const {
        return m_lastSkinningMatrices;
    }

private:

    const SkeletonDescription* m_pDescription;

    std::vector<Joint> m_joints;
    std::vector<glm::mat4> m_skinningMatrices;
    std::vector<glm::mat4> m_lastSkinningMatrices;
};

class SkeletonPose {

public:

    explicit SkeletonPose(const SkeletonDescription* pDescription) :
        m_pDescription(pDescription)
    {
        if (m_pDescription) {
            m_jointRotations.assign(m_pDescription->getNumJoints(), glm::quat(1, 0, 0, 0));
            m_jointOffsets.assign(m_pDescription->getNumJoints(), glm::vec3(0, 0, 0));
        }
    }

    SkeletonPose(const SkeletonPose& pose) :
        m_pDescription(pose.m_pDescription),
        m_jointRotations(pose.m_jointRotations),
        m_jointOffsets(pose.m_jointOffsets) {
    }

    SkeletonPose(SkeletonPose&& pose) :
        m_pDescription(std::move(pose.m_pDescription)),
        m_jointRotations(std::move(pose.m_jointRotations)),
        m_jointOffsets(std::move(pose.m_jointOffsets)) {
    }

    // Copy the Skeleton's current pose
    SkeletonPose(const Skeleton& skeleton) :
        m_pDescription(skeleton.getDescription()),
        m_jointRotations(skeleton.getDescription()->getNumJoints()),
        m_jointOffsets(skeleton.getDescription()->getNumJoints()) {
        for (auto i = 0u; i < skeleton.getDescription()->getNumJoints(); ++i) {
            m_jointOffsets[i] = skeleton.getJoint(i)->getLocalPoseOffset();
            m_jointRotations[i] = skeleton.getJoint(i)->getLocalPoseRotation();
        }
    }

    ~SkeletonPose() {
    }

    SkeletonPose& setJointRotation(size_t joint, const glm::quat& rotation) {
        m_jointRotations[joint] = rotation;

        return *this;
    }

    SkeletonPose& setJointOffset(size_t joint, const glm::vec3& offset) {
        m_jointOffsets[joint] = offset;

        return *this;
    }

    glm::quat getJointRotation(size_t joint) const {
        return m_jointRotations[joint];
    }

    glm::vec3 getJointOffset(size_t joint) const {
        return m_jointOffsets[joint];
    }

    const SkeletonDescription* getSkeletonDescription() const {
        return m_pDescription;
    }

    SkeletonPose& lerp(const SkeletonPose& other, float x) {
        assert(m_pDescription == other.m_pDescription);

        for (size_t i = 0; i < m_pDescription->getNumJoints(); ++i) {
            m_jointRotations[i] = glm::slerp(m_jointRotations[i], other.m_jointRotations[i], x);
            m_jointOffsets[i] = glm::mix(m_jointOffsets[i], other.m_jointOffsets[i], x);
        }

        return *this;
    }

    SkeletonPose& operator=(const SkeletonPose& pose) {
        m_pDescription = pose.m_pDescription;
        m_jointRotations = pose.m_jointRotations;
        m_jointOffsets = pose.m_jointOffsets;

        return *this;
    }

    SkeletonPose& operator=(SkeletonPose&& pose) {
        m_pDescription = std::move(pose.m_pDescription);
        m_jointRotations = std::move(pose.m_jointRotations);
        m_jointOffsets = std::move(pose.m_jointOffsets);

        return *this;
    }

private:

    const SkeletonDescription* m_pDescription;

    std::vector<glm::quat> m_jointRotations;
    std::vector<glm::vec3> m_jointOffsets;
};


#endif // SKELETON_H_
