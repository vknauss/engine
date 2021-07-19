#include "physics.h"

#include <glm/gtc/matrix_transform.hpp>

static glm::vec3 vec3ToGLM(const btVector3& btVec3) {
    return glm::vec3(btVec3.x(), btVec3.y(), btVec3.z());
}

static btVector3 vec3ToBt(const glm::vec3& glmVec3) {
    return btVector3(glmVec3.x, glmVec3.y, glmVec3.z);
}

void RigidBody::applyForce(const glm::vec3& force, const glm::vec3& offset) {
    if (m_pSystem) {
        m_pSystem->applyForce(m_rigidBodyID, force, offset);
    }
}

void RigidBody::applyTorque(const glm::vec3& torque) {
    if (m_pSystem) {
        m_pSystem->applyTorque(m_rigidBodyID, torque);
    }
}

void RigidBody::applyImpulse(const glm::vec3& impulse) {
    if (m_pSystem) {
        m_pSystem->applyImpulse(m_rigidBodyID, impulse);
    }
}

void RigidBodyMotionState::getWorldTransform(btTransform& worldTrans) const {
    const RigidBody& body = m_pSystem->m_rigidBodies[m_bodyIndex];
    glm::mat4 tfm = body.m_worldTransform * glm::translate(glm::mat4(1.0f), body.m_parameters.shapeOffset);
    worldTrans.setFromOpenGLMatrix(&tfm[0][0]);
}


void RigidBodyMotionState::setWorldTransform(const btTransform& worldTrans) {
    RigidBody& body = m_pSystem->m_rigidBodies[m_bodyIndex];
    btRigidBody* pBtBody = m_pSystem->m_pBtRigidBodies[m_bodyIndex];

    glm::mat4 tfm;
    worldTrans.getOpenGLMatrix(&tfm[0][0]);
    body.m_worldTransform = tfm * glm::translate(glm::mat4(1.0f), -body.m_parameters.shapeOffset);

    body.m_velocity = vec3ToGLM(pBtBody->getLinearVelocity());
    body.m_angularVelocity = vec3ToGLM(pBtBody->getAngularVelocity());
    body.m_netForce = vec3ToGLM(pBtBody->getTotalForce());
    body.m_netTorque = vec3ToGLM(pBtBody->getTotalTorque());
}

void PhysicsSystem::init() {
    if (!m_initialized) {
        m_pBtConfiguration = new btDefaultCollisionConfiguration();
        m_pBtDispatcher = new btCollisionDispatcher(m_pBtConfiguration);
        m_pBtBroadphase = new btDbvtBroadphase();
        m_pBtSolver = new btSequentialImpulseConstraintSolver();

        m_pBtDynamicsWorld = new btDiscreteDynamicsWorld(m_pBtDispatcher, m_pBtBroadphase, m_pBtSolver, m_pBtConfiguration);

        m_initialized = true;
    }
}

void PhysicsSystem::cleanup() {
    if (m_initialized) {
        for (btRigidBody* pBtBody : m_pBtRigidBodies) delete pBtBody;
        m_pBtRigidBodies.clear();

        for (btCollisionShape* pBtShape : m_pBtCollisionShapes) delete pBtShape;
        m_pBtCollisionShapes.clear();

        for (RigidBodyMotionState* pMotionState : m_pMotionStates) delete pMotionState;
        m_pMotionStates.clear();

        for (const CollisionCallbackBase* pCallback : m_pCollisionCallbacks) delete pCallback;
        m_pCollisionCallbacks.clear();
        m_callbackIndexMap.clear();

        m_rigidBodies.clear();

        delete m_pBtDynamicsWorld;
        m_pBtDynamicsWorld = nullptr;
        delete m_pBtSolver;
        m_pBtSolver = nullptr;
        delete m_pBtBroadphase;
        m_pBtBroadphase = nullptr;
        delete m_pBtDispatcher;
        m_pBtDispatcher = nullptr;
        delete m_pBtConfiguration;
        m_pBtConfiguration = nullptr;

        m_initialized = false;
    }
}

#include <iostream>

void PhysicsSystem::update(float dt) {
    m_pBtDynamicsWorld->stepSimulation(dt);

    int numManifolds = m_pBtDispatcher->getNumManifolds();

    std::map<const btPersistentManifold*, bool> collisionsHandled;

    for (int i = 0; i < numManifolds; ++i) {
        const btPersistentManifold* pBtManifold = m_pBtDispatcher->getManifoldByIndexInternal(i);

        if (pBtManifold->getNumContacts()) {
            collisionsHandled[pBtManifold] = true;
            if (m_collisionHandled[pBtManifold]) {
                continue;
            }

            const CollisionCallbackBase* pCallback = findCollisionCallback(pBtManifold->getBody0()->getUserIndex(), pBtManifold->getBody1()->getUserIndex());
            if (pCallback) {
                for (int j = 0; j < pBtManifold->getNumContacts(); ++j) {
                    pCallback->onCollision(vec3ToGLM(pBtManifold->getContactPoint(j).getPositionWorldOnA()));
                }
            }
        }
    }

    m_collisionHandled = collisionsHandled;
}

//#include <iostream>
size_t PhysicsSystem::addRigidBody(const RigidBody& rigidBody) {
    size_t index = m_rigidBodies.size();
    m_rigidBodies.push_back(rigidBody);
    m_rigidBodies.back().setOwningSystem(this, index);

    RigidBodyMotionState* pMotionState = new RigidBodyMotionState(this, index);

    btVector3 localInertia;
    rigidBody.m_parameters.pBtShape->calculateLocalInertia(rigidBody.m_parameters.mass, localInertia);

    btRigidBody::btRigidBodyConstructionInfo info(rigidBody.m_parameters.mass, pMotionState, rigidBody.m_parameters.pBtShape, localInertia);
    info.m_friction = rigidBody.m_parameters.friction;
    info.m_restitution = rigidBody.m_parameters.restitution;

    btRigidBody* pBtBody = new btRigidBody(info);
    pBtBody->setAngularFactor(vec3ToBt(rigidBody.m_parameters.angularFactor));
    pBtBody->setAngularVelocity(vec3ToBt(rigidBody.m_parameters.initialAngularVelocity));
    pBtBody->setLinearVelocity(vec3ToBt(rigidBody.m_parameters.initialVelocity));

    pBtBody->setUserIndex(rigidBody.m_parameters.typeSpecifier);

    m_pBtDynamicsWorld->addRigidBody(pBtBody);

    m_pMotionStates.push_back(pMotionState);
    m_pBtRigidBodies.push_back(pBtBody);

    return index;
}

bool PhysicsSystem::castRay(const glm::vec3& origin, const glm::vec3& ray) {
    btCollisionWorld::ClosestRayResultCallback callback(vec3ToBt(origin), vec3ToBt(origin+ray));
    m_pBtDynamicsWorld->rayTest(vec3ToBt(origin), vec3ToBt(origin+ray), callback);
    return callback.hasHit();
}

void PhysicsSystem::applyForce(size_t rigidBodyID, const glm::vec3& force, const glm::vec3& offset) {
    m_pBtRigidBodies[rigidBodyID]->activate();
    m_pBtRigidBodies[rigidBodyID]->applyForce(vec3ToBt(force), vec3ToBt(offset));
}

void PhysicsSystem::applyTorque(size_t rigidBodyID, const glm::vec3& torque) {
    m_pBtRigidBodies[rigidBodyID]->activate();
    m_pBtRigidBodies[rigidBodyID]->applyTorque(vec3ToBt(torque));
}

void PhysicsSystem::applyImpulse(size_t rigidBodyID, const glm::vec3& impulse) {
    m_pBtRigidBodies[rigidBodyID]->activate(true);
    m_pBtRigidBodies[rigidBodyID]->applyImpulse(vec3ToBt(impulse), btVector3(0, 0, 0));
}
