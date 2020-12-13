#ifndef RENDER_SCENE_STATE_H_
#define RENDER_SCENE_STATE_H_

#include <vector>

#include <glm/glm.hpp>

struct RenderInstanceList {
    uintptr_t m_meshHandle;
    uintptr_t m_materialHandle;

    size_t m_numInstances;
    size_t m_numSkinningMatricesPerInstance;

    std::vector<glm::mat4> m_instanceTransforms;
    std::vector<glm::mat4> m_skinningMatrices;
};

class RenderSceneState {

    friend class Renderer;
    friend class Renderer2;

    glm::mat4 m_viewMatrix;
    glm::mat4 m_projectionMatrix;

    std::vector<RenderInstanceList> m_instanceLists;

public:

    RenderSceneState();

};

#endif // RENDER_SCENE_STATE_H_
