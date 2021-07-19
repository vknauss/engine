#ifndef GAME_WORLD_H_INCLUDED
#define GAME_WORLD_H_INCLUDED

#include <entt/entt.hpp>

//#include "core/physics/physics.h"
//#include "core/scene/scene.h"

struct Entity;

enum EntityDestroyMode { ENTITY_DESTROY_HIERARCHY, ENTITY_DESTROY_REPARENT, ENTITY_DESTROY_CLEAR_PARENT };

class GameWorld {

    // I don't really want to expose entt directly to all my code
    friend struct Entity;
    friend class EditorGUI;

public:

    GameWorld();

    Entity createEntity();

    void destroyEntity(Entity e, EntityDestroyMode destroyMode = ENTITY_DESTROY_CLEAR_PARENT);

    bool isEntityValid(Entity e) const;

    void setParent(Entity e, Entity parent);
    void clearParent(Entity e);

    /*void setScene(Scene* pScene) {
        m_pScene = pScene;
    }

    void setPhysics(PhysicsSystem* pPhysics) {
        m_pPhysics = pPhysics;
    }

    Scene* getScene() {
        return m_pScene;
    }

    const Scene* getScene() const {
        return m_pScene;
    }

    PhysicsSystem* getPhysics() {
        return m_pPhysics;
    }

    const PhysicsSystem* getPhysics() const {
        return m_pPhysics;
    }*/

    // walk the node hierarchy and update the world transforms based on the local transform
    // called automatically in preRenderUpdate(), but if world transforms are needed before then it can be called earlier
    void updateHierarchy();

    void updateBoundingSpheres();

    // Sync the entities' Transform components with the RigidBody transforms
    void postPhysicsUpdate();

    // Sync the entities' Renderable transforms with the Transform components
    // Set doUpdateHierarchy to false if the hierarchy has been updated manually and is still valid
    void preRenderUpdate(bool doUpdateHierarchy=true);

    entt::registry& getRegistry() {
        return m_registry;
    }

    const entt::registry& getRegistry() const {
        return m_registry;
    }

private:

    entt::registry m_registry;

    //Scene* m_pScene;
    //PhysicsSystem* m_pPhysics;

    /*void onSetParent(entt::registry& r, entt::entity e);
    void onUpdateParent(entt::registry& r, entt::entity e);
    void onUnsetParent(entt::registry& r, entt::entity e);*/

};

#endif // GAME_WORLD_H_INCLUDED
