#ifndef EDITOR_GUI_H_INCLUDED
#define EDITOR_GUI_H_INCLUDED

#include <map>
#include <string>

#include "core/ecs/entity.h"
#include "core/ecs/game_world.h"
#include "core/render/renderer.h"
#include "core/scene/scene.h"
#include "core/app/window.h"
#include "core/resources/resource_manager.h"
#include "core/util/mesh_builder.h"
#include "core/animation/animation_system.h"

class EditorGUI {

public:

    EditorGUI();
    ~EditorGUI();

    void setGameWorld(GameWorld* pGameWorld) { m_pGameWorld = pGameWorld; }

    void setRenderer(Renderer* pRenderer) { m_pRenderer = pRenderer; }

    void setScene(Scene* pScene) {
        m_pScene = pScene;
        m_editorCameraID = m_pScene->addCamera(Camera()
                                               .setFOV(M_PI/4.0f)
                                               .setNearPlane(0.1f)
                                               .setFarPlane(100.0f)
                                               .setDirection(glm::vec3(0, 0, -1))
                                               .setPosition(glm::vec3(0, 1, 1)));
        m_pScene->setActiveCamera(m_editorCameraID);
    }

    void setWindow(AppWindow* pWindow) { m_pWindow = pWindow; }

    void setResourceManager(ResourceManager* pResManager) { m_pResManager = pResManager; }

    void setAnimationSystem(AnimationSystem* pAnimation) { m_pAnimation = pAnimation; }

    void beginFrame();

    void update();

    void render();


private:

    GameWorld* m_pGameWorld;
    Renderer* m_pRenderer;
    Scene* m_pScene;
    AppWindow* m_pWindow;
    ResourceManager* m_pResManager;
    AnimationSystem* m_pAnimation;

    Entity m_selectedEntity = {entt::null, nullptr};
    Asset::Node* m_pSelectedAssetNode = nullptr;
    Model* m_pSelectedModel = nullptr;

    int m_transformInputGizmo = -1;

    // Meters per pixel
    float m_cameraTranslateSpeed = 0.01f;

    // meters per scroll unit
    float m_cameraDollySpeed = 0.1f;

    // radians per pixel
    float m_cameraRotateSpeed = 0.002f;

    uint32_t m_editorCameraID = Scene::ID_NULL;

    struct EntityTransform {
        glm::vec3 position;
        glm::quat rotation;
        glm::vec3 rotationEuler;
        glm::vec3 scale;
        glm::vec3 localScale;
        Entity entity;
    };
    EntityTransform m_entityTransform;
    EntityTransform m_entityTransformCache;

    std::map<Model*, MeshBuilder> m_modelMeshBuilders;
    std::map<Model*, MeshBuilderNodes> m_modelMeshBuilderNodes;

    bool m_initialized = false;

    void initialize();

    void updateEntityTransform();
    void applyEntityTransform();
    void drawTransformGizmo();
    void drawEntityInfo();

    void leftPanel();
    void centerPanel();
    void rightPanel();

    void onSelectEntity();
    void onDeselectEntity();

    uint32_t m_outlineRenderableID = Scene::ID_NULL;

};

#endif // EDITOR_GUI_H_INCLUDED
