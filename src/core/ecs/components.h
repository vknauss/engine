#ifndef COMPONENTS_H_INCLUDED
#define COMPONENTS_H_INCLUDED

#include <string>
#include <vector>
#include <map>

#include <glm/glm.hpp>

#include "core/ecs/entity.h"

class AnimationStateGraph;
class AnimationSystem;
class Scene;
class PhysicsSystem;

namespace Component {

struct Transform {
    glm::mat4 world = glm::mat4(1.0f);
    glm::mat4 local = glm::mat4(1.0f);

    glm::mat4 lastWorld = glm::mat4(1.0f);

    struct DirtyFlag {};
    //struct UpdatedFlag {};
};

struct Name {
    std::string str;
};

// Please use GameWorld::setParent and GameWorld::clearParent instead of adding and updating the Parent and Children components manually.
// Treat these components as read-only
// If you choose to ignore this and do want to create and manage these manually, make sure to follow these rules:
//  - Every entity has either a Parent or TopLevel component, and never both
//  - If an entity has a Parent component, the entity in Parent::entity of that component has a Children component,
//    and the child entity is in the Children::entities vector of that component
struct Parent {
    Entity entity;
    //explicit Parent(Entity e) : entity(e) {}  // Need to use the constructor to make sure the entity field is initialized in the onSetParent callback for building the hierarchy
    // Idk if that makes sense but basically pass the parent entity in to addComponent
};

// You should not create this! Simply set the parent this will be created automatically
struct Children {
    std::vector<Entity> entities;
};

// Just a flag. Will be automatically set when an entity is created and unset when a Parent component is added
// Used since entt doesn't allow exclusion-only views, otherwise I would just use a view over everything without the parent component
struct TopLevel {
};

/*struct RenderableID {
    uint32_t id;
    Scene* pScene;

    RenderableID(Scene* pScene) :
        pScene(pScene) { }
};*/

struct RigidBodyID {
    uint32_t id;
    PhysicsSystem* pPhysics;

    RigidBodyID(PhysicsSystem* pPhysics) :
        pPhysics(pPhysics) { }
};

/*struct PointLightID {
    uint32_t id;
    Scene* pScene;

    PointLightID(Scene* pScene) :
        pScene(pScene) { }
};*/

struct AnimationStateID {
    uint32_t id;
    const AnimationStateGraph* pStateGraph = nullptr;
    AnimationSystem* pAnimation;

    AnimationStateID(AnimationSystem* pAnimation) :
        pAnimation(pAnimation) { }
};


};

#endif // COMPONENTS_H_INCLUDED
