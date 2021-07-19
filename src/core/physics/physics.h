#ifndef PHYSICS_H_INCLUDED
#define PHYSICS_H_INCLUDED

#include <map>
#include <vector>

#include <btBulletDynamicsCommon.h>

#include <glm/glm.hpp>

class CollisionShape {

public:

    virtual ~CollisionShape() { }

    virtual btCollisionShape* getBtShape() const = 0;

};

struct RigidBodyParameters {

    RigidBodyParameters(float mass, btCollisionShape* pBtShape, const glm::mat4& initialWorldTransform) :
        mass(mass),
        pBtShape(pBtShape),
        initialWorldTransform(initialWorldTransform) {
    }

    float mass;
    float friction = 0.0f;
    float restitution = 0.0f;

    int typeSpecifier = 0;

    btCollisionShape* pBtShape;

    glm::vec3 shapeOffset = glm::vec3(0.0f);

    glm::vec3 angularFactor = glm::vec3(1.0f);

    glm::mat4 initialWorldTransform;

    glm::vec3 initialVelocity = glm::vec3(0.0f);
    glm::vec3 initialAngularVelocity = glm::vec3(0.0f);
};

class PhysicsSystem;

class RigidBody {

    friend class PhysicsSystem;
    friend class RigidBodyMotionState;

public:
    // Create a rigid body with given initial velocity and angular velocity
    RigidBody(float mass, btCollisionShape* pBtShape, glm::mat4 worldTransform, glm::vec3 velocity, glm::vec3 angularVelocity) :
        m_parameters(mass, pBtShape, worldTransform),
        m_worldTransform(worldTransform),
        m_velocity(velocity),
        m_angularVelocity(angularVelocity)
    {
        m_parameters.mass = mass;
        m_parameters.pBtShape = pBtShape;
        m_parameters.initialWorldTransform = worldTransform;
        m_parameters.initialVelocity = velocity;
        m_parameters.initialAngularVelocity = angularVelocity;
    }

    RigidBody(const RigidBodyParameters& parameters) :
        m_parameters(parameters),
        m_worldTransform(parameters.initialWorldTransform),
        m_velocity(parameters.initialVelocity),
        m_angularVelocity(parameters.initialAngularVelocity) {
    }

    // Create a rigid body, initially at rest
    RigidBody(float mass, btCollisionShape* pBtShape, glm::mat4 worldTransform) :
        RigidBody(mass, pBtShape, worldTransform, glm::vec3(0.0f), glm::vec3(0.0f)) {
    }

    // Create a static rigid body
    RigidBody(btCollisionShape* pBtShape, glm::mat4 worldTransform) :
        RigidBody(0.0f, pBtShape, worldTransform, glm::vec3(0.0f), glm::vec3(0.0f)) {
    }

    RigidBodyParameters getParameters() const {
        return m_parameters;
    }

    glm::mat4 getWorldTransform() const {
        return m_worldTransform;
    }

    glm::vec3 getVelocity() const {
        return m_velocity;
    }

    glm::vec3 getAngularVelocity() const {
        return m_angularVelocity;
    }

    glm::vec3 getNetForce() const {
        return m_netForce;
    }

    glm::vec3 getNetTorque() const {
        return m_netTorque;
    }


    void applyForce(const glm::vec3& force, const glm::vec3& offset);
    void applyTorque(const glm::vec3& torque);
    void applyImpulse(const glm::vec3& impulse);

private:


    RigidBodyParameters m_parameters;

    glm::mat4 m_worldTransform;

    glm::vec3 m_velocity;

    glm::vec3 m_angularVelocity;

    glm::vec3 m_netForce = glm::vec3(0.0f);

    glm::vec3 m_netTorque = glm::vec3(0.0f);

    PhysicsSystem* m_pSystem = nullptr;
    size_t m_rigidBodyID;

    void setOwningSystem(PhysicsSystem* pSystem, size_t rigidBodyID) {
        m_pSystem = pSystem;
        m_rigidBodyID = rigidBodyID;
    }

};

class RigidBodyMotionState : public btMotionState {

public:

    RigidBodyMotionState(PhysicsSystem* pSystem, size_t bodyIndex) :
        m_pSystem(pSystem),
        m_bodyIndex(bodyIndex) {
    }

    void getWorldTransform(btTransform& worldTrans) const override;

    void setWorldTransform(const btTransform& worldTrans) override;

private:

    PhysicsSystem* m_pSystem;

    size_t m_bodyIndex;

};

class PhysicsSystem {

    friend class RigidBodyMotionState;

public:

    ~PhysicsSystem() {
        if (m_initialized) cleanup();
    }

    void init();

    void cleanup();

    void update(float dt);

    // Set the ownership of the shape to the physics system, so it can be deleted at shutdown
    btCollisionShape* addCollisionShape(btCollisionShape* pBtShape) {
        m_pBtCollisionShapes.push_back(pBtShape);
        return pBtShape;
    }

    size_t addRigidBody(const RigidBody& rigidBody);

    const RigidBody* getRigidBody(size_t index) const {
        return &m_rigidBodies[index];
    }

    RigidBody* getRigidBody(size_t index) {
        return &m_rigidBodies[index];
    }

    bool castRay(const glm::vec3& origin, const glm::vec3& ray);

    void applyForce(size_t rigidBodyID, const glm::vec3& force, const glm::vec3& offset);
    void applyTorque(size_t rigidBodyID, const glm::vec3& torque);
    void applyImpulse(size_t rigidBodyID, const glm::vec3& impulse);

    template<typename T>
    void addCollisionCallback(int typeSpec0, int typeSpec1, const T& callback);

private:

    struct CollisionCallbackBase {
        int typeSpec0;
        int typeSpec1;

        virtual ~CollisionCallbackBase() { }
        virtual void onCollision(const glm::vec3& contactPoint) const = 0;
    };

    template<typename T>
    struct CollisionCallback : CollisionCallbackBase {
        T callback;

        CollisionCallback(const T& cb) : callback(cb) { }

        void onCollision(const glm::vec3& contactPoint) const {
            return callback(contactPoint);
        }
    };


    btDiscreteDynamicsWorld* m_pBtDynamicsWorld = nullptr;

    btDefaultCollisionConfiguration* m_pBtConfiguration = nullptr;
    btCollisionDispatcher* m_pBtDispatcher = nullptr;
    btDbvtBroadphase* m_pBtBroadphase = nullptr;
    btSequentialImpulseConstraintSolver* m_pBtSolver = nullptr;

    bool m_initialized = false;

    std::vector<btCollisionShape*> m_pBtCollisionShapes;

    std::vector<btRigidBody*> m_pBtRigidBodies;

    std::vector<CollisionShape*> m_pCollisionShapes;

    std::vector<RigidBodyMotionState*> m_pMotionStates;

    std::vector<RigidBody> m_rigidBodies;

    std::vector<const CollisionCallbackBase*> m_pCollisionCallbacks;
    std::map<int, std::map<int, size_t>> m_callbackIndexMap;
    std::map<const btPersistentManifold*, bool> m_collisionHandled;

    const CollisionCallbackBase* findCollisionCallback(int typeSpec0, int typeSpec1) const {
        if (auto it = m_callbackIndexMap.find(std::min(typeSpec0, typeSpec1)); it != m_callbackIndexMap.end()) {
            const std::map<int, size_t>& innerMap = it->second;
            if (auto it2 = innerMap.find(std::max(typeSpec0, typeSpec1)); it2 != innerMap.end()) {
                return m_pCollisionCallbacks[it2->second];
            }
        }
        return nullptr;
    }

};

template<typename T>
void PhysicsSystem::addCollisionCallback(int typeSpec0, int typeSpec1, const T& callback) {
    CollisionCallback<T>* pCallback = new CollisionCallback<T>(callback);
    pCallback->typeSpec0 = std::min(typeSpec0, typeSpec1);
    pCallback->typeSpec1 = std::max(typeSpec0, typeSpec1);

    m_callbackIndexMap[pCallback->typeSpec0][pCallback->typeSpec1] = m_pCollisionCallbacks.size();
    m_pCollisionCallbacks.push_back(pCallback);
}

#endif // PHYSICS_H_INCLUDED
