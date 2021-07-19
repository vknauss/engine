#include "game_world.h"

#include <iostream>

#include "entity.h"
#include "components.h"

#include "core/physics/physics.h"
#include "core/scene/bounding_sphere.h"
#include "core/scene/renderable.h"
#include "core/scene/point_light.h"

// Ensure every Renderable has a Transform and Bounding Sphere
static void onCreateRenderable(entt::registry& r, entt::entity e) {
    (void) r.get_or_emplace<Component::Transform>(e);
    r.emplace<BoundingSphere>(e);
}

GameWorld::GameWorld() {
    // Transforms are initialized with the dirty flag set so everything always initializes correctly
    m_registry.on_construct<Component::Transform>().connect<&entt::registry::emplace_or_replace<Component::Transform::DirtyFlag>>();
    m_registry.on_update<Component::Transform>().connect<&entt::registry::emplace_or_replace<Component::Transform::DirtyFlag>>();

    m_registry.on_construct<Component::Renderable>().connect<&onCreateRenderable>();
    m_registry.on_update<Component::Renderable>().connect<&entt::registry::emplace_or_replace<Component::Transform::DirtyFlag>>();
}

Entity GameWorld::createEntity() {
    Entity e = {m_registry.create(), this};
    e.addComponent<Component::TopLevel>();
    return e;
}

void GameWorld::destroyEntity(Entity e, EntityDestroyMode destroyMode) {
    if (e.hasComponent<Component::Children>()) {
        std::vector<Entity> children = e.getComponent<Component::Children>().entities;
        if (destroyMode == ENTITY_DESTROY_HIERARCHY) {
            for (Entity child : children) {
                destroyEntity(child, destroyMode);
            }
        } else if (destroyMode == ENTITY_DESTROY_REPARENT && e.hasComponent<Component::Parent>()) {
            Entity parent = e.getComponent<Component::Parent>().entity;
            for (Entity child : children) {
                setParent(child, parent);
            }
        } else {
            for (Entity child : children) {
                clearParent(child);
            }
        }
    }
    if (e.hasComponent<Component::Parent>()) clearParent(e);
    m_registry.destroy(e.id);
}

bool GameWorld::isEntityValid(Entity e) const {
    return m_registry.valid(e.id);
}

static void unsetParent(Entity e) {
    Entity parent = e.getComponent<Component::Parent>().entity;
    std::vector<Entity>& pchildren = parent.getComponent<Component::Children>().entities;
    pchildren.erase(std::find(pchildren.begin(), pchildren.end(), e));
    if (pchildren.empty()) parent.removeComponent<Component::Children>();
}

void GameWorld::setParent(Entity e, Entity parent) {
    // ensure we don't introduce a loop
    if (e == parent) return;
    if (parent.hasComponent<Component::Parent>()) {
        Entity pentity = parent;
        while (pentity.isValid()) {
            Entity pparent;
            if (pentity.hasComponent<Component::Parent>())
                pparent = pentity.getComponent<Component::Parent>().entity;
            if (pentity == e) {
                for (Entity child : e.getComponent<Component::Children>().entities) {
                    if (pparent.isValid()) setParent(child, pparent);
                    else clearParent(child);
                }
                break;
            }
            pentity = pparent;
        }
    }
    if (e.hasComponent<Component::Parent>()) {
        unsetParent(e);
        e.getComponent<Component::Parent>().entity = parent;
    } else {
        e.removeComponent<Component::TopLevel>();
        e.addComponent<Component::Parent>().entity = parent;
    }
    if (!parent.hasComponent<Component::Children>())
        parent.addComponent<Component::Children>();
    parent.getComponent<Component::Children>().entities.push_back(e);
    if (e.hasComponent<Component::Transform>())
        e.addComponent<Component::Transform::DirtyFlag>();
}

void GameWorld::clearParent(Entity e) {
    unsetParent(e);
    e.removeComponent<Component::Parent>();
    e.addComponent<Component::TopLevel>();
    if (e.hasComponent<Component::Transform>())
        e.addComponent<Component::Transform::DirtyFlag>();
}

static void recUpdateTransform(Entity e, glm::mat4 ptfm, bool dirty) {
    if (e.hasComponent<Component::Transform>()) {
        Component::Transform& tfm = e.getComponent<Component::Transform>();
        tfm.lastWorld = tfm.world;
        if (e.hasComponent<Component::Transform::DirtyFlag>()) {
            e.removeComponent<Component::Transform::DirtyFlag>();
            dirty = true;
        }
        if (dirty) {
            ptfm *= tfm.local;
            tfm.world = ptfm;
        } else {
            ptfm = tfm.world;
        }
    }
    if (e.hasComponent<Component::Children>()) {
        for (Entity& c : e.getComponent<Component::Children>().entities)
            recUpdateTransform(c, ptfm, dirty);
    }
}

void GameWorld::updateHierarchy() {
    auto topLevelView = m_registry.view<Component::TopLevel>();
    for (auto e : topLevelView) recUpdateTransform(Entity{e, this}, glm::mat4(1.0f), false);
}

void GameWorld::updateBoundingSpheres() {
    auto nonSkeletalView =
        m_registry.view<Component::Transform,
                        Component::Renderable>(
                            entt::exclude<Component::Renderable::SkeletalFlag>);

    for (auto e : nonSkeletalView) {
        const auto& t = nonSkeletalView.get<Component::Transform>(e);
        const auto& r = nonSkeletalView.get<Component::Renderable>(e);

        if (!r.pModel) continue;
        BoundingSphere b = r.pModel->getBoundingSphere();
        b.position = glm::vec3(t.world * glm::vec4(b.position, 1.0f));
        glm::mat3 inner(t.world);
        float scale = std::sqrt(std::max({ glm::dot(inner[0], inner[0]),
                                           glm::dot(inner[1], inner[1]),
                                           glm::dot(inner[2], inner[2]) }));
        b.radius *= scale;
        m_registry.replace<BoundingSphere>(e, b);
    }

    auto skeletalView =
        m_registry.view<Component::Transform,
                        Component::Renderable,
                        Component::Renderable::SkeletalFlag>();

    for (auto e : skeletalView) {
        const auto& t = skeletalView.get<Component::Transform>(e);
        const auto& r = skeletalView.get<Component::Renderable>(e);

        const auto& jointSpheres = r.pModel->getJointBoundingSpheres();
        BoundingSphere b;
        glm::mat3 inner(t.world);
        float scale = std::sqrt(std::max({ glm::dot(inner[0], inner[0]),
                                           glm::dot(inner[1], inner[1]),
                                           glm::dot(inner[2], inner[2]) }));
        for (auto i = 0u; i < jointSpheres.size(); ++i) {
            glm::mat4 jointMatrix = t.world * r.pSkeleton->getSkinningMatrices()[i];
            BoundingSphere b1;
            b1.position = glm::vec3(jointMatrix * glm::vec4(jointSpheres[i].position, 1.0f));
            b1.radius = scale * jointSpheres[i].radius;
            if (i == 0) b = b1;
            else b.add(b1);
        }
        m_registry.replace<BoundingSphere>(e, b);
    }
}

void GameWorld::postPhysicsUpdate() {
    m_registry.view<Component::Transform, Component::RigidBodyID>().each(
        [this] (auto e, auto& tfm, auto& rb) {
            tfm.local = rb.pPhysics->getRigidBody(rb.id)->getWorldTransform();
            m_registry.emplace_or_replace<Component::Transform::DirtyFlag>(e);
        });
}

void GameWorld::preRenderUpdate(bool doUpdateHierarchy) {
    if (doUpdateHierarchy) updateHierarchy();

    // Sort renderables such that all instances of a given model form a contiguous block
    // and any instances of a model without a skeleton occur before any instances of that model with a skeleton
    m_registry.sort<Component::Renderable>(
        [] (const Component::Renderable& r1, const Component::Renderable& r2) {
            return r1.pModel < r2.pModel || (r1.pModel == r2.pModel && !r1.pSkeleton && r2.pSkeleton); },
        entt::insertion_sort {});

    m_registry.view<PointLight, const Component::Transform>().each(
        [] (auto& pl, auto& tfm) { pl.setPosition(tfm.world[3]); });
}
