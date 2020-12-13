#include "scene.h"

#include <algorithm>


Scene::Scene() : m_directionalLight(DirectionalLight()), m_ambientLightIntensity(0.0f) {
}

Scene::~Scene() {

}

void Scene::computeAABB(glm::vec3& minBounds, glm::vec3& maxBounds) const {
    minBounds = glm::vec3(std::numeric_limits<float>::max());
    maxBounds = glm::vec3(std::numeric_limits<float>::min());

    for(uint32_t rid = 0; rid < m_renderables.size(); ++rid) {
        if(m_isRenderableIDFree[rid]) continue;
        BoundingSphere b = m_renderables[rid].computeWorldBoundingSphere();
        minBounds = glm::min(minBounds, b.getPosition() - glm::vec3(b.getRadius()));
        maxBounds = glm::max(maxBounds, b.getPosition() + glm::vec3(b.getRadius()));
    }
}

uint32_t Scene::addRenderable(const Renderable& renderable) {
    uint32_t id;
    if(m_freeRenderableIDs.empty()) {
        id = m_renderables.size();
        m_renderables.push_back(renderable);
        m_isRenderableIDFree.push_back(false);
    } else {
        id = m_freeRenderableIDs.front();
        m_freeRenderableIDs.pop_front();
        m_renderables[id] = renderable;
        m_isRenderableIDFree[id] = false;
    }

    uint32_t modelIndex;
    if (renderable.getLODComponent() != nullptr) {
        if(auto kv = m_pLODIndices.find(renderable.getLODComponent()); kv != m_pLODIndices.end()) {
            modelIndex = kv->second;
            if (m_lodInstanceLists[modelIndex].empty()) {
                if (renderable.getLODComponent()->getModelMinLOD()->getSkeletonDescription()) {
                    m_skinnedLODIndices.push_back(modelIndex);
                    if (renderable.getLODComponent()->getModelMinLOD()->getMaterial()->isShadowCastingEnabled()) {
                        m_skinnedShadowCasterLODIndices.push_back(modelIndex);
                    }
                } else {
                    m_nonSkinnedLODIndices.push_back(modelIndex);
                    if (renderable.getLODComponent()->getModelMinLOD()->getMaterial()->isShadowCastingEnabled()) {
                        m_nonSkinnedShadowCasterLODIndices.push_back(modelIndex);
                    }
                }
            }
            m_lodInstanceLists[modelIndex].push_back(id);
        } else {
            modelIndex = m_pLODComponents.size();
            m_pLODComponents.push_back(renderable.getLODComponent());
            m_lodInstanceLists.push_back(std::list<uint32_t>({id}));
            m_pLODIndices.insert(std::make_pair(renderable.getLODComponent(), modelIndex));
            if (renderable.getLODComponent()->getModelMinLOD()->getSkeletonDescription()) {
                m_skinnedLODIndices.push_back(modelIndex);
                if (renderable.getLODComponent()->getModelMinLOD()->getMaterial()->isShadowCastingEnabled()) {
                    m_skinnedShadowCasterLODIndices.push_back(modelIndex);
                }
            } else {
                m_nonSkinnedLODIndices.push_back(modelIndex);
                if (renderable.getLODComponent()->getModelMinLOD()->getMaterial()->isShadowCastingEnabled()) {
                    m_nonSkinnedShadowCasterLODIndices.push_back(modelIndex);
                }
            }
        }
    } else {
        if(auto kv = m_pModelIndices.find(renderable.getModel()); kv != m_pModelIndices.end()) {
            modelIndex = kv->second;
            if (m_instanceLists[modelIndex].empty()) {
                if (renderable.getModel()->getSkeletonDescription()) {
                    m_skinnedModelIndices.push_back(modelIndex);
                    if (renderable.getModel()->getMaterial()->isShadowCastingEnabled()) {
                        m_skinnedShadowCasterModelIndices.push_back(modelIndex);
                    }
                } else {
                    m_nonSkinnedModelIndices.push_back(modelIndex);
                    if (renderable.getModel()->getMaterial()->isShadowCastingEnabled()) {
                        m_nonSkinnedShadowCasterModelIndices.push_back(modelIndex);
                    }
                }
            }
            m_instanceLists[modelIndex].push_back(id);
        } else {
            modelIndex = m_pModels.size();
            m_pModels.push_back(renderable.getModel());
            m_instanceLists.push_back(std::list<uint32_t>({id}));
            m_pModelIndices.insert(std::make_pair(renderable.getModel(), modelIndex));
            if (renderable.getModel()->getSkeletonDescription()) {
                m_skinnedModelIndices.push_back(modelIndex);
                if (renderable.getModel()->getMaterial()->isShadowCastingEnabled()) {
                    m_skinnedShadowCasterModelIndices.push_back(modelIndex);
                }
            } else {
                m_nonSkinnedModelIndices.push_back(modelIndex);
                if (renderable.getModel()->getMaterial()->isShadowCastingEnabled()) {
                    m_nonSkinnedShadowCasterModelIndices.push_back(modelIndex);
                }
            }
        }
    }

    return id;
}

void Scene::removeRenderable(uint32_t renderableID) {
    if(renderableID >= m_renderables.size()) return;
    m_isRenderableIDFree[renderableID] = true;
    m_freeRenderableIDs.push_back(renderableID);
    if (m_renderables[renderableID].getLODComponent() != nullptr) {
        if(auto kv = m_pLODIndices.find(m_renderables[renderableID].getLODComponent()); kv != m_pLODIndices.end()) {
            m_lodInstanceLists[kv->second].remove(renderableID);
            if (m_lodInstanceLists[kv->second].empty()) {

            }
        }
    } else {
        if(auto kv = m_pModelIndices.find(m_renderables[renderableID].getModel()); kv != m_pModelIndices.end()) {
            m_instanceLists[kv->second].remove(renderableID);
            if (m_instanceLists[kv->second].empty()) {

            }
        }
    }
}

uint32_t Scene::addPointLight(const PointLight& pointLight) {
    uint32_t id = m_pointLights.size();
    m_pointLights.push_back(pointLight);
    return id;
}

uint32_t Scene::addCamera(const Camera& camera) {
    uint32_t id = m_cameras.size();
    m_cameras.push_back(camera);
    return id;
}

void Scene::setActiveCamera(uint32_t cameraID) {
    if(cameraID >= m_cameras.size()) throw std::runtime_error("Out of bounds active camera index");
    m_activeCameraID = cameraID;
}

void Scene::setAmbientLightIntensity(const glm::vec3& intensity) {
    m_ambientLightIntensity = intensity;
}

Renderable* Scene::getRenderable(uint32_t renderableID) {
    if(renderableID >= m_renderables.size() || m_isRenderableIDFree[renderableID]) return nullptr;
    return &m_renderables[renderableID];
}

PointLight* Scene::getPointLight(uint32_t pointLightID) {
    if(pointLightID >= m_pointLights.size()) return nullptr;
    return &m_pointLights[pointLightID];
}

Camera* Scene::getCamera(uint32_t cameraID) {
    if(cameraID >= m_cameras.size()) return nullptr;
    return &m_cameras[cameraID];
}

const Camera* Scene::getCamera(uint32_t cameraID) const {
    if(cameraID >= m_cameras.size()) return nullptr;
    return &m_cameras[cameraID];
}
