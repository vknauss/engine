#ifndef SCENE_H_
#define SCENE_H_

#include <forward_list>
#include <list>
#include <map>
#include <vector>

#include "camera.h"
#include "directional_light.h"
#include "point_light.h"
#include "renderable.h"

/*class Scene;

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

};*/

class GameWorld;

class Scene {

    friend class Renderer;
    friend class InstanceListBuilder;

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


    template<typename T> struct SceneList {
        std::vector<T> items;
        std::vector<bool> isFree;
        std::forward_list<uint32_t> freeIDs;

        uint32_t add(const T& item) {
            uint32_t index;
            if (freeIDs.empty()) {
                index = (uint32_t) items.size();
                items.push_back(item);
                isFree.push_back(false);
            } else {
                index = freeIDs.front();
                freeIDs.pop_front();
                items[index] = item;
                isFree[index] = false;
            }
            return index;
        }

        void remove(uint32_t id) {
            if (id < items.size() && !isFree[id]) {
                isFree[id] = true;
                freeIDs.push_front(id);
            }
        }
    };




    struct ModelHasher {
        uint32_t meshID;
        uint32_t materialID;
        uint32_t skeletonDescID;

        bool operator<(const ModelHasher& mh);
    };

public:

    static constexpr uint32_t ID_NULL = std::numeric_limits<uint32_t>::max();

    Scene();

    ~Scene();

    void computeAABB(glm::vec3& minBounds, glm::vec3& maxBounds) const;

    void copyLastTransforms();

    void computeBoundingSpheres();

    void updateRenderables(GameWorld* pGameWorld);


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

    uint32_t addPointLight(const PointLight& pointLight);

    uint32_t addCamera(const Camera& camera);

    /*uint32_t addMesh(const Mesh& mesh);

    uint32_t addMaterial(const Material& material);

    uint32_t addModel(const Model& model);

    uint32_t addSkeletonDescription(const SkeletonDescription& sd);

    uint32_t addModel(uint32_t meshID, uint32_t materialID, uint32_t skeletonDescID = ID_NULL);*/

    Skeleton* addSkeleton(const SkeletonDescription* pSkeletonDesc) {
        m_pSkeletons.push_back(new Skeleton(pSkeletonDesc));
        return m_pSkeletons.back();
    }

    //uint32_t addRenderable(uint32_t modelID);


    void removeRenderable(uint32_t renderableID);

    void removePointLight(uint32_t pointLightID);

    void removeCamera(uint32_t cameraID);

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

    // This is a bit awkward, but the Scene actually doesn't keep track of how many 'alive' Renderables it contains.
    // For iterating over all renderables, you should actually iterate over IDs from 0 to this number.
    // getRenderable() will return null if the ID is for a 'dead' Renderable
    size_t getNumRenderableIDs() const {
        return m_renderables.size();
    }

    const std::vector<Renderable>& getRenderables() const {
        return m_renderables;
    }

    const std::vector<PointLight>& getPointLights() const {
        return m_pointLights;
    }

    const std::vector<BoundingSphere>& getRenderableBoundingSpheres() const {
        return m_renderableBoundingSpheres;
    }


private:

//    std::vector<SceneNode> m_nodes;

    std::vector<Renderable> m_renderables;
    std::vector<Skeleton*> m_pSkeletons;

    /*std::vector<Model> m_models;
    std::vector<Mesh> m_meshes;
    std::vector<Material> m_materials;
    std::vector<SkeletonDescription> m_skeletonDescriptions;
    std::vector<Skeleton> m_skeletons;*/

    std::vector<const Model*> m_pModels;
    std::vector<std::list<uint32_t>> m_instanceLists;
    std::map<const Model*, uint32_t> m_pModelIndices;

    //std::map<ModelHasher, uint32_t> m_modelIndices;

    std::vector<const LODComponent*> m_pLODComponents;
    std::vector<std::list<uint32_t>> m_lodInstanceLists;
    std::map<const LODComponent*, uint32_t> m_pLODIndices;

    /*std::vector<uint32_t> m_skinnedModelIndices;
    std::vector<uint32_t> m_nonSkinnedModelIndices;
    std::vector<uint32_t> m_skinnedShadowCasterModelIndices;
    std::vector<uint32_t> m_nonSkinnedShadowCasterModelIndices;

    std::vector<uint32_t> m_skinnedLODIndices;
    std::vector<uint32_t> m_nonSkinnedLODIndices;
    std::vector<uint32_t> m_skinnedShadowCasterLODIndices;
    std::vector<uint32_t> m_nonSkinnedShadowCasterLODIndices;*/

    std::vector<bool> m_isRenderableIDFree;
    std::list<uint32_t> m_freeRenderableIDs;

    std::vector<BoundingSphere> m_renderableBoundingSpheres;

    std::vector<PointLight> m_pointLights;
    std::vector<Camera> m_cameras;

    DirectionalLight m_directionalLight;

    glm::vec3 m_ambientLightIntensity;

    uint32_t m_activeCameraID;

};

#endif // SCENE_H_
