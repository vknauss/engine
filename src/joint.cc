#include "joint.h"

#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtx/transform.hpp>

Joint::Joint() :
    m_name(),
    m_index(JOINT_UNINITIALIZED),
    m_pParent(nullptr),
    m_matrixBind(1.0f),
    m_matrixBindLocal(1.0f),
    m_matrixBindInverse(1.0f),
    m_matrixWorld(1.0f),
    m_matrixLocal(1.0f),
    m_offsetBind(0.0f),
    m_rotationBind(1.0f, 0.0f, 0.0f, 0.0f),
    m_offsetLocal(0.0f),
    m_rotationLocal(1.0f, 0.0f, 0.0f, 0.0f),
    m_offsetWorld(0.0f),
    m_rotationWorld(1.0f, 0.0f, 0.0f, 0.0f) {
}

void Joint::computeWorldMatrix() {
    m_rotationLocal = glm::normalize(m_rotationLocal);
    m_matrixLocal = glm::translate(m_offsetLocal) * glm::mat4_cast(m_rotationLocal);
    if(m_pParent) {
        m_matrixWorld = m_pParent->getWorldMatrix() * m_matrixBindLocal * m_matrixLocal;
        m_rotationWorld = m_pParent->getWorldRotation() * m_rotationBind * m_rotationLocal;
        m_offsetWorld = glm::vec3(m_matrixWorld[3]);
    } else {
        m_matrixWorld =  m_matrixBind * m_matrixLocal;
        m_offsetWorld = m_offsetBind + m_offsetLocal;
        m_rotationWorld = m_rotationBind * m_rotationLocal;
    }
}

void Joint::clearLocalPose() {
    m_offsetLocal = glm::vec3(0);
    m_rotationLocal = glm::quat(1, 0, 0, 0);
}
