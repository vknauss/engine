#ifndef ENTITY_H_INCLUDED
#define ENTITY_H_INCLUDED

#include <type_traits>

#include "game_world.h"

struct Entity {
    entt::entity id = entt::null;
    GameWorld* pGameWorld = nullptr;

    bool isValid() const {
        return pGameWorld && pGameWorld->m_registry.valid(id);
    }

    template<typename T>
    T& getComponent() {
        return pGameWorld->m_registry.get<T>(id);
    }

    template<typename T>
    const T& getComponent() const {
        return pGameWorld->m_registry.get<T>(id);
    }

    template<typename T, typename ... Args, typename std::enable_if<!std::is_empty<T>::value>::type* = nullptr>
    T& updateComponent(Args ... args) {
        return pGameWorld->m_registry.replace<T>(id, args...);
    }

    template<typename T, typename ... Args, typename std::enable_if<!std::is_empty<T>::value>::type* = nullptr>
    T& addComponent(Args ... args) {
        return pGameWorld->m_registry.emplace<T>(id, args...);
    }

    // For use with empty types
    template<typename T, typename std::enable_if<std::is_empty<T>::value>::type* = nullptr>
    void addComponent() {
        pGameWorld->m_registry.emplace<T>(id);
    }

    template<typename T>
    void removeComponent() {
        pGameWorld->m_registry.remove<T>(id);
    }

    template<typename T>
    bool hasComponent() const {
        return pGameWorld->m_registry.all_of<T>(id);
    }

    bool operator==(const Entity& e) {
        return e.id == id && e.pGameWorld == pGameWorld;
    }
    bool operator!=(const Entity& e) {
        return e.id != id || e.pGameWorld != pGameWorld;
    }
};

#endif // ENTITY_H_INCLUDED
