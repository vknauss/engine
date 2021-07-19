#include "scene.h"

#include <algorithm>
#include <exception>
#include <stdexcept>

bool Scene::ModelHasher::operator<(const Scene::ModelHasher& mh) {
    return meshID < mh.meshID || (meshID == mh.meshID &&
        (materialID < mh.materialID || (materialID == mh.materialID &&
         skeletonDescID < mh.skeletonDescID)));
}

Scene::Scene() : m_directionalLight(DirectionalLight()), m_ambientLightIntensity(0.0f) {
}

Scene::~Scene() {
    for (Skeleton* pSkeleton : m_pSkeletons) if (pSkeleton) delete pSkeleton;
}

void Scene::computeAABB(glm::vec3& minBounds, glm::vec3& maxBounds) const {
    minBounds = glm::vec3(std::numeric_limits<float>::max());
    maxBounds = glm::vec3(std::numeric_limits<float>::min());

    for (uint32_t rid = 0; rid < m_renderables.size(); ++rid) {
        if (m_isRenderableIDFree[rid]) continue;
        const BoundingSphere& b = m_renderableBoundingSpheres[rid];
        minBounds = glm::min(minBounds, b.position - glm::vec3(b.radius));
        maxBounds = glm::max(maxBounds, b.position + glm::vec3(b.radius));
    }
}

void Scene::copyLastTransforms() {
    std::for_each(m_renderables.begin(), m_renderables.end(),
        [] (Renderable& r) {
            r.copyLastTransform();
            if (r.getSkeleton()) {
                r.getSkeleton()->copyLastSkinningMatrices();
            }
        });
}

void Scene::computeBoundingSpheres() {
    m_renderableBoundingSpheres.resize(m_renderables.size());
    std::transform(m_renderables.begin(), m_renderables.end(),
                   m_renderableBoundingSpheres.begin(),
                   [] (Renderable& r) {
                        return r.computeWorldBoundingSphere();
                   });
}

void Scene::updateRenderables(GameWorld* pGameWorld) {

}

uint32_t Scene::addRenderable(const Renderable& renderable) {
    uint32_t id;
    if (m_freeRenderableIDs.empty()) {
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
        if (auto kv = m_pLODIndices.find(renderable.getLODComponent()); kv != m_pLODIndices.end()) {
            modelIndex = kv->second;
            m_lodInstanceLists[modelIndex].push_back(id);
        } else {
            modelIndex = m_pLODComponents.size();
            m_pLODComponents.push_back(renderable.getLODComponent());
            m_lodInstanceLists.push_back(std::list<uint32_t>({id}));
            m_pLODIndices.insert(std::make_pair(renderable.getLODComponent(), modelIndex));
        }
    } else {
        if (auto kv = m_pModelIndices.find(renderable.getModel()); kv != m_pModelIndices.end()) {
            modelIndex = kv->second;
            m_instanceLists[modelIndex].push_back(id);
        } else {
            modelIndex = m_pModels.size();
            m_pModels.push_back(renderable.getModel());
            m_instanceLists.push_back(std::list<uint32_t>({id}));
            m_pModelIndices.insert(std::make_pair(renderable.getModel(), modelIndex));
        }
    }

    return id;
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

/*uint32_t Scene::addMesh(const Mesh& mesh) {
    uint32_t id = static_cast<uint32_t>(m_meshes.size());
    m_meshes.push_back(mesh);
    return id;
}

uint32_t Scene::addMaterial(const Material& material) {
    uint32_t id = static_cast<uint32_t>(m_materials.size());
    m_materials.push_back(material);
    return id;
}

uint32_t Scene::addModel(const Model& model) {
    uint32_t id = static_cast<uint32_t>(m_models.size());
    ModelHasher mh = {};
    m_models.push_back(model);
    return id;
}

uint32_t Scene::addSkeletonDescription(const SkeletonDescription& sd) {
    uint32_t id = static_cast<uint32_t>(m_skeletonDescriptions.size());
    m_skeletonDescriptions.push_back(sd);
    return id;
}

uint32_t Scene::addModel(uint32_t meshID, uint32_t materialID, uint32_t skeletonDescID) {
    uint32_t id = static_cast<uint32_t>(m_models.size());

    return id;
}*/

void Scene::removeRenderable(uint32_t renderableID) {
    if (renderableID >= m_renderables.size()) return;
    m_isRenderableIDFree[renderableID] = true;
    m_freeRenderableIDs.push_back(renderableID);
    if (m_renderables[renderableID].getLODComponent() != nullptr) {
        if (auto kv = m_pLODIndices.find(m_renderables[renderableID].getLODComponent()); kv != m_pLODIndices.end()) {
            m_lodInstanceLists[kv->second].remove(renderableID);
        }
    } else {
        if (auto kv = m_pModelIndices.find(m_renderables[renderableID].getModel()); kv != m_pModelIndices.end()) {
            m_instanceLists[kv->second].remove(renderableID);
        }
    }
}

void Scene::removePointLight(uint32_t pointLightID) {
    if (pointLightID >= m_pointLights.size()) return;

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
