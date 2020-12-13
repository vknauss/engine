#ifndef SCENE_H_
#define SCENE_H_

#include <list>
#include <map>
#include <vector>

#include "camera.h"
#include "directional_light.h"
#include "point_light.h"
#include "renderable.h"
#include "lod_component.h"

class Scene;

class SceneNode {
    const Scene* m_pScene;

    uint32_t m_index;
    uint32_t m_parentIndex;

    std::list<uint32_t> m_childrenIndices;

public:

    SceneNode(const Scene* pScene) : m_pScene(pScene) {
    }

    ~SceneNode() {
    }

};

class Scene {

    friend class Renderer;
    friend class InstanceListBuilder;
    friend class FrustumCuller;

    /*typedef uintptr_t ResourceHandle;
    const ResourceHandle RESOURCE_NULL = std::numeric_limits<uintptr_t>::max();

    struct SceneNode {
        uint32_t id;
        uint32_t parent;

        std::list<uint32_t> children;

        glm::mat4 worldTransform;
        glm::mat4 localTransform;

        ResourceHandle hModel = RESOURCE_NULL;
        ResourceHandle hPointLight = RESOURCE_NULL;
        ResourceHandle hCamera = RESOURCE_NULL;
        ResourceHandle hLODComponent = RESOURCE_NULL;

        bool dirty;
    };*/


public:

    Scene();

    ~Scene();

    void computeAABB(glm::vec3& minBounds, glm::vec3& maxBounds) const;


    /*uint32_t createNode();

    void setNodeModel(ResourceHandle hModel);

    void setNodePointLight(ResourceHandle hPointLight);

    void setNodeCamera(ResourceHandle hCamera);

    void setNodeLODComponent(ResourceHandle hLODComponent);*/


    // Take care when using get* methods:
    // These pointers may (will likely) be invalidated when subsequent adds modify
    //   the underlying containers, since all types are held in flat vectors.
    // The intention is to keep track of the necessary ids, and use get* to modify instances

    uint32_t addRenderable(const Renderable& renderable);

    void removeRenderable(uint32_t renderableID);

    uint32_t addPointLight(const PointLight& pointLight);

    uint32_t addCamera(const Camera& camera);

    void setActiveCamera(uint32_t cameraID);

    void setAmbientLightIntensity(const glm::vec3& intensity);

    Renderable* getRenderable(uint32_t renderableID);

    PointLight* getPointLight(uint32_t pointLightID);

    Camera* getCamera(uint32_t cameraID);

    const Camera* getCamera(uint32_t cameraID) const;

    uint32_t getActiveCameraID() const {
        return m_activeCameraID;
    }

    Camera* getActiveCamera() {
        return getCamera(getActiveCameraID());
    }

    const Camera* getActiveCamera() const {
        return getCamera(getActiveCameraID());
    }

    const DirectionalLight& getDirectionalLight() const {
        return m_directionalLight;
    }

    DirectionalLight& getDirectionalLight() {
        return m_directionalLight;
    }

    glm::vec3 getAmbientLightIntensity() const {
        return m_ambientLightIntensity;
    }


private:

    std::vector<SceneNode> m_nodes;

    std::vector<Renderable> m_renderables;

    std::vector<const Model*> m_pModels;
    std::vector<std::list<uint32_t>> m_instanceLists;
    std::map<const Model*, uint32_t> m_pModelIndices;

    std::vector<const LODComponent*> m_pLODComponents;
    std::vector<std::list<uint32_t>> m_lodInstanceLists;
    std::map<const LODComponent*, uint32_t> m_pLODIndices;

    std::vector<uint32_t> m_skinnedModelIndices;
    std::vector<uint32_t> m_nonSkinnedModelIndices;
    std::vector<uint32_t> m_skinnedShadowCasterModelIndices;
    std::vector<uint32_t> m_nonSkinnedShadowCasterModelIndices;

    std::vector<uint32_t> m_skinnedLODIndices;
    std::vector<uint32_t> m_nonSkinnedLODIndices;
    std::vector<uint32_t> m_skinnedShadowCasterLODIndices;
    std::vector<uint32_t> m_nonSkinnedShadowCasterLODIndices;

    std::vector<bool> m_isRenderableIDFree;
    std::list<uint32_t> m_freeRenderableIDs;

    std::vector<PointLight> m_pointLights;
    std::vector<Camera> m_cameras;

    DirectionalLight m_directionalLight;

    glm::vec3 m_ambientLightIntensity;

    uint32_t m_activeCameraID;

};

#endif // SCENE_H_
