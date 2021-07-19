#ifndef JOINT_H_
#define JOINT_H_

#include <string>

#include <glm/glm.hpp>
#include <glm/gtx/quaternion.hpp>

class Joint {

    friend class Skeleton;

public:

    static constexpr size_t JOINT_UNINITIALIZED = std::numeric_limits<size_t>::max();

    Joint();

    void computeWorldMatrix();

    void clearLocalPose();

    const std::string& getName() const {
        return m_name;
    }

    size_t getIndex() const {
        return m_index;
    }

    Joint* const getParent() {
        return m_pParent;
    }

    glm::mat4 getBindMatrix() const {
        return m_matrixBind;
    }

    glm::mat4 getInverseBindMatrix() const {
        return m_matrixBindInverse;
    }

    glm::mat4 getLocalBindMatrix() const {
        return m_matrixBindLocal;
    }

    glm::mat4 getLocalMatrix() const {
        return m_matrixLocal;
    }

    glm::mat4 getWorldMatrix() const {
        return m_matrixWorld;
    }

    glm::quat getWorldRotation() const {
        return m_rotationWorld;
    }

    glm::vec3 getWorldPosition() const {
        return m_offsetWorld;
    }

    glm::quat getLocalPoseRotation() const {
        return m_rotationLocal;
    }

    glm::vec3 getLocalPoseOffset() const {
        return m_offsetLocal;
    }

    Joint& setParent(Joint* parent) {
        m_pParent = parent;
        return *this;
    }

    Joint& setName(const std::string& name) {
        m_name = name;
        return *this;
    }

    Joint& setIndex(size_t index) {
        m_index = index;
        return *this;
    }

    void setLocalPoseRotation(const glm::quat& rotation) {
        m_rotationLocal = rotation;
    }

    void setLocalPoseOffset(const glm::vec3& offset) {
        m_offsetLocal = offset;
    }

private:

    std::string m_name;
    size_t m_index;

    Joint* m_pParent;

    glm::mat4 m_matrixBind;
    glm::mat4 m_matrixBindLocal;
    glm::mat4 m_matrixBindInverse;

    glm::mat4 m_matrixWorld;
    glm::mat4 m_matrixLocal;

    glm::vec3 m_offsetBind;
    glm::quat m_rotationBind;

    glm::vec3 m_offsetLocal;
    glm::quat m_rotationLocal;

    glm::vec3 m_offsetWorld;
    glm::quat m_rotationWorld;

};



#endif // JOINT_H_
