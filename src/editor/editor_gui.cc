#include "editor_gui.h"

#include <iostream>
#include <filesystem>
#include <unordered_map>

#if defined(__linux__)
#include <gtkmm-3.0/gtkmm.h>
#include <gdk/gdkx.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#define GLFW_EXPOSE_NATIVE_X11
#endif // defined
#include <GLFW/glfw3native.h>

#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtx/string_cast.hpp>

#include "imgui/imgui_impl_glfw.h"
#include "imgui/imgui_impl_opengl3.h"
#include "imgui/imgui_internal.h"

#include "core/resources/resource_load.h"
#include "core/ecs/components.h"

#include "transform_gizmos.h"

class FileDialog {

public:

    static bool open(std::string& fileName, AppWindow* pWindow) {
        bool hasFile = false;

        #if defined(__linux__)
        static Gtk::Main gtkMain;
        Gtk::FileChooserDialog fileDialog("Import");

        fileDialog.add_button("Cancel", GTK_RESPONSE_CLOSE);
        fileDialog.add_button("Open", GTK_RESPONSE_OK);

        struct FileDialogAction {
            Gtk::FileChooserDialog& fileDialog;

            bool& hasFile;
            std::string& fileName;

            Display* pX11Display;
            Window topLevelX11Window;
            Window fileDialogX11Window = 0;

            void onResponse(int response) {
                if (response == GTK_RESPONSE_OK) {
                    fileName = fileDialog.get_filename();
                    if (!fileName.empty()) hasFile = true;
                }
                fileDialog.close();
            }

            void onMap() {
                // We have to wait until the dialog is mapped for GTK to assign it a GDK window,
                // so we can't do this on creation.

                // Get the X11 handle for the file dialog's GDK window
                Gdk::Window* pGdkWindow = fileDialog.get_window().get();
                assert(pGdkWindow);
                fileDialogX11Window = gdk_x11_window_get_xid(pGdkWindow->gobj());

                Atom aNET_WM_STATE_MODAL = XInternAtom(pX11Display, "_NET_WM_STATE_MODAL", False);
                XChangeProperty(pX11Display, fileDialogX11Window, XInternAtom(pX11Display, "_NET_WM_STATE", False),
                                XA_ATOM, 32, PropModeAppend, (const unsigned char*) &aNET_WM_STATE_MODAL, 1);


            }

            bool onMapEvent(GdkEventAny*) {
                // Make the dialog modal
                // Seems like this needs to happen AFTER the window has been mapped,
                // which is when this event is processed

                // Make the dialog transient for the top-level window
                XSetTransientForHint(pX11Display, fileDialogX11Window, topLevelX11Window);

                // https://developer.gnome.org/wm-spec/#idm45384480082848
                // https://tronche.com/gui/x/xlib/events/client-communication/client-message.html
                // https://tronche.com/gui/x/xlib/event-handling/XSendEvent.html
                XEvent event;
                event.xclient.type = ClientMessage;
                event.xclient.display = pX11Display;
                event.xclient.window = fileDialogX11Window;
                event.xclient.message_type = XInternAtom(pX11Display, "_NET_WM_STATE", False);
                event.xclient.format = 32;
                event.xclient.data.l[0] = 1;  // _NET_WM_STATE_ADD
                event.xclient.data.l[1] = XInternAtom(pX11Display, "_NET_WM_STATE_MODAL", False);
                event.xclient.data.l[2] = 0;
                event.xclient.data.l[3] = 0;
                event.xclient.data.l[4] = 0;

                XSendEvent(pX11Display, DefaultRootWindow(pX11Display), False,
                           SubstructureNotifyMask|SubstructureRedirectMask, &event);

                // This bit is because the window properties seem not to want to update
                // unless we query the window for something--it doesn't seem to matter what.
                // It's bizarre and frustrating. A dirty hack
                char* iconName = nullptr; XGetIconName(pX11Display, fileDialogX11Window, &iconName); if (iconName) XFree(iconName);

                /*
                // Testing purposes
                // Leaving this in as a comment for future reference, since Xlib is a pain :)
                Atom* pGetProps = nullptr;
                Atom actualType;
                int actualFormat;
                unsigned long nItems, bytesAfter;

                XGetWindowProperty(pX11Display, fileDialogX11Window, XInternAtom(pX11Display, "_NET_WM_STATE", true),
                                   0, LONG_MAX, false, XA_ATOM, &actualType, &actualFormat, &nItems, &bytesAfter, (unsigned char**) (&pGetProps));

                for (auto i = 0u; i < nItems; ++i) std::cout << "Window state: " << XGetAtomName(pX11Display, pGetProps[i]) << std::endl;
                if (pGetProps) XFree(pGetProps);*/

                return false;
            }
        };

        Display *pX11Display = glfwGetX11Display();
        Window topLevelX11Window = glfwGetX11Window(pWindow->getHandle());

        FileDialogAction action = {fileDialog, hasFile, fileName, pX11Display, topLevelX11Window};

        fileDialog.signal_map().connect(sigc::mem_fun(action, &FileDialogAction::onMap));
        fileDialog.signal_map_event().connect(sigc::mem_fun(action, &FileDialogAction::onMapEvent));
        fileDialog.signal_response().connect(sigc::mem_fun(action, &FileDialogAction::onResponse));

        Gtk::Main::run(fileDialog);
        #endif

        return hasFile;
    }

};

EditorGUI::EditorGUI() :
    m_pRenderer(nullptr),
    m_pScene(nullptr),
    m_pWindow(nullptr) {
}

EditorGUI::~EditorGUI() {
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext(ImGui::GetCurrentContext());
}

void EditorGUI::beginFrame() {
    m_pWindow->acquireContext();
    if (!m_initialized) initialize();
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
    m_pWindow->releaseContext();
}

static bool selectTexture(ResourceManager* pResManager, const Texture* pCurrentTexture, const Texture*& pSelectedTexture) {
    std::string label = pCurrentTexture ? pCurrentTexture->getName() : "Select";
    if (label.empty()) label = "Untitled texture";
    if (ImGui::Button(label.c_str())) ImGui::OpenPopup("Select Texture");
    if (ImGui::BeginPopup("Select Texture")) {
        for (auto& pTex : pResManager->pTextures) {
            std::string name = pTex->getName();
            if (name.empty()) name = "Untitled texture";
            if (ImGui::MenuItem(name.c_str(), nullptr, pCurrentTexture == pTex.get())) {
                pSelectedTexture = pTex.get();
                ImGui::EndPopup();
                return true;
            }
        }
        if (ImGui::MenuItem("None", nullptr, !pCurrentTexture)) {
            pSelectedTexture = nullptr;
            ImGui::EndPopup();
            return true;
        }
        ImGui::EndPopup();
    }
    return false;
}

static void entityNode(Entity entity, Entity& selectedEntity, bool& openMenu) {
    std::string name = entity.hasComponent<Component::Name>() ?
        entity.getComponent<Component::Name>().str :
        std::string("Entity");

    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick | ImGuiTreeNodeFlags_NoTreePushOnOpen;

    if (!entity.hasComponent<Component::Children>()) flags |= ImGuiTreeNodeFlags_Leaf;
    if (selectedEntity == entity) flags |= ImGuiTreeNodeFlags_Selected;

    ImGui::PushID((const void*) entity.id);
    bool open = ImGui::TreeNodeEx(name.c_str(), flags);

    if (ImGui::IsItemClicked()) {
        selectedEntity = entity;
    }
    if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) {
        selectedEntity = entity;
        openMenu = true;
    }

    if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) {
        ImGui::SetDragDropPayload("Entity Hierarchy Node", (const void*) &entity.id, sizeof(entt::entity));
        ImGui::Text("%s", name.c_str());

        ImGui::EndDragDropSource();
    }
    if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* pPayload = ImGui::AcceptDragDropPayload("Entity Hierarchy Node")) {
            entt::entity id = *(entt::entity*)(intptr_t)(pPayload->Data);
            Entity dragged = {id, entity.pGameWorld};
            bool isParent = false;
            if (entity.hasComponent<Component::Parent>()) {
                Entity parent = entity.getComponent<Component::Parent>().entity;
                do {
                    if (parent == dragged) {
                        isParent = true;
                        break;
                    }
                    if (parent.hasComponent<Component::Parent>()) {
                        parent = parent.getComponent<Component::Parent>().entity;
                        continue;
                    }
                    break;
                } while (true);
            }
            if (!isParent) entity.pGameWorld->setParent(dragged, entity);
        }

        ImGui::EndDragDropTarget();
    }

    if (open && entity.hasComponent<Component::Children>()) {
        ImGui::TreePush(name.c_str());
        for (Entity& c : entity.getComponent<Component::Children>().entities)
            entityNode(c, selectedEntity, openMenu);
        ImGui::TreePop();
    }
    ImGui::PopID();
}

static void assetNode(Asset::Node* pNode, Asset::Node*& pSelectedNode) {
    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick | ImGuiTreeNodeFlags_NoTreePushOnOpen;

    if (pNode->pChildren.empty()) flags |= ImGuiTreeNodeFlags_Leaf;
    if (pSelectedNode == pNode) flags |= ImGuiTreeNodeFlags_Selected;

    ImGui::PushID(pNode);
    bool open = ImGui::TreeNodeEx(pNode->name.c_str(), flags);

    if (ImGui::IsItemClicked()) {
        pSelectedNode = pNode;
    }

    if (open && !pNode->pChildren.empty()) {
        ImGui::TreePush(pNode->name.c_str());
        for (Asset::Node* pChildNode : pNode->pChildren) {
            assetNode(pChildNode, pSelectedNode);
        }
        ImGui::TreePop();
    }

    ImGui::PopID();
}

void EditorGUI::drawTransformGizmo() {

    static int mode = 0;
    if (ImGui::Button("Gizmo")) ImGui::OpenPopup("Gizmo Menu");
    if (ImGui::BeginPopup("Gizmo Menu")) {
        if (ImGui::BeginMenu("Type")) {
            if (ImGui::MenuItem("Translate", nullptr, m_transformInputGizmo==0)) m_transformInputGizmo = 0;
            if (ImGui::MenuItem("Rotate", nullptr, m_transformInputGizmo==1)) m_transformInputGizmo = 1;
            if (ImGui::MenuItem("Scale", nullptr, m_transformInputGizmo==2)) m_transformInputGizmo = 2;
            if (ImGui::MenuItem("None", nullptr, m_transformInputGizmo==-1)) m_transformInputGizmo = -1;
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Mode", m_transformInputGizmo != 2)) {
            if (ImGui::MenuItem("Global", nullptr, mode == 0)) mode = 0;
            if (ImGui::MenuItem("Local", nullptr, mode == 1)) mode = 1;
            ImGui::EndMenu();
        }
        ImGui::EndPopup();
    }

    if (m_transformInputGizmo != -1) {
        bool recalc = false;

        Camera* pCamera = m_pScene->getActiveCamera();

        glm::mat4 viewMatrix = pCamera->calculateViewMatrix();
        glm::mat4 clipMatrix = pCamera->calculateProjectionMatrix() * viewMatrix;
        glm::mat3 viewAxes = glm::transpose(glm::mat3(viewMatrix));
        glm::vec2 windowSize = glm::vec2(ImGui::GetWindowWidth(), ImGui::GetWindowHeight()); //glm::vec2(m_pWindow->getWidth(), m_pWindow->getHeight());
        glm::mat4 worldMatrix = glm::mat4(1.0f);

        if (m_selectedEntity.hasComponent<Component::Parent>()) {
            Entity parent = m_selectedEntity.getComponent<Component::Parent>().entity;
            if (parent.hasComponent<Component::Transform>()) {
                worldMatrix = parent.getComponent<Component::Transform>().world;
            }
        }

        if (m_transformInputGizmo == 0) {
            glm::vec3 translation;

            glm::mat3 axes(1.0f);
            if (mode == 1) {
                axes = glm::toMat3(m_entityTransform.rotation);
            }

            bool cancel = false;
            if (Gizmo3D::translation("Translation Gizmo",
                                   m_entityTransform.position,
                                   axes,
                                   pCamera->getPosition(),
                                   viewAxes,
                                   worldMatrix,
                                   clipMatrix,
                                   windowSize,
                                   pCamera->getFOV(),
                                   translation,
                                   cancel)) {
                m_entityTransform.position = m_entityTransformCache.position + translation;
                recalc = true;
            } else {
                if (cancel) {
                    m_entityTransform.position = m_entityTransformCache.position;
                    recalc = true;
                }
            }
        } else if (m_transformInputGizmo == 1) {
            glm::quat rotation;

            glm::mat3 axes(1.0f);
            if (mode == 1) {
                axes = glm::toMat3(m_entityTransform.rotation);
            }

            bool cancel = false;
            if (Gizmo3D::rotation("Rotation Gizmo",
                                  m_entityTransform.position,
                                  axes,
                                  pCamera->getPosition(),
                                  viewAxes,
                                  worldMatrix,
                                  clipMatrix,
                                  windowSize,
                                  pCamera->getFOV(),
                                  rotation,
                                  cancel)) {
                m_entityTransform.rotation = glm::normalize(rotation) * m_entityTransformCache.rotation;
                m_entityTransform.rotationEuler = glm::degrees(glm::eulerAngles(m_entityTransform.rotation));
                recalc = true;
            } else {
                if (cancel) {
                    m_entityTransform.rotation = m_entityTransformCache.rotation;
                    m_entityTransform.rotationEuler = glm::degrees(glm::eulerAngles(m_entityTransform.rotation));
                    recalc = true;
                }
            }
        } else if (m_transformInputGizmo == 2) {
            glm::vec3 scale;

            // some weirdness because scaling is applied before rotation, so we have to rotate the axes
            glm::mat4 toLocal = glm::translate(glm::mat4(1.0f), m_entityTransform.position) *
                                glm::toMat4(m_entityTransform.rotation) *
                                glm::translate(glm::mat4(1.0f), -m_entityTransform.position);
            glm::mat3 axes(1.0f);

            bool cancel = false;
            if (Gizmo3D::scale("Scale Gizmo",
                               m_entityTransform.position,
                               axes,
                               pCamera->getPosition(),
                               viewAxes,
                               worldMatrix * toLocal,
                               clipMatrix,
                               windowSize,
                               pCamera->getFOV(),
                               scale,
                               cancel)) {
                m_entityTransform.scale = m_entityTransformCache.scale * scale;
                recalc = true;
            } else {
                if (cancel) {
                    m_entityTransform.scale = m_entityTransformCache.scale;
                    recalc = true;
                }
            }
        }

        if (recalc) {
            applyEntityTransform();
        } else {
            m_entityTransformCache = m_entityTransform;
        }
    }
}

void EditorGUI::updateEntityTransform() {
    if (!m_selectedEntity.hasComponent<Component::Transform>()) {
        m_entityTransform.entity.id = entt::null;
        m_entityTransformCache.entity.id = entt::null;
        return;
    }

    glm::mat4& localTfm = m_selectedEntity.getComponent<Component::Transform>().local;

    m_entityTransform.position = localTfm[3];

    glm::vec3& scale = m_entityTransform.scale;

    glm::mat3 rotationOnly = glm::mat3(localTfm);
    scale.x = glm::length(rotationOnly[0]);
    rotationOnly[0] = rotationOnly[0] / scale.x;
    rotationOnly[1] = rotationOnly[1] - glm::dot(rotationOnly[0], rotationOnly[1]) * rotationOnly[0];
    scale.y = glm::length(rotationOnly[1]);
    rotationOnly[1] = rotationOnly[1] / scale.y;
    rotationOnly[2] = rotationOnly[2] - glm::dot(rotationOnly[0], rotationOnly[2]) * rotationOnly[0];
    rotationOnly[2] = rotationOnly[2] - glm::dot(rotationOnly[1], rotationOnly[2]) * rotationOnly[1];
    scale.z = glm::length(rotationOnly[2]);
    rotationOnly[2] = rotationOnly[2] / scale.z;

    m_entityTransform.rotation = glm::toQuat(rotationOnly);
    m_entityTransform.rotationEuler = glm::degrees(glm::eulerAngles(m_entityTransform.rotation));

    m_entityTransform.entity = m_selectedEntity;

    m_entityTransformCache = m_entityTransform;
}

void EditorGUI::applyEntityTransform() {
    if (m_entityTransform.entity != m_selectedEntity) return;

    m_selectedEntity.getComponent<Component::Transform>().local =
        glm::translate(glm::mat4(1.0f), m_entityTransform.position) *
        glm::toMat4(m_entityTransform.rotation) *
        glm::scale(glm::mat4(1.0f), m_entityTransform.scale);
    m_selectedEntity.addComponent<Component::Transform::DirtyFlag>();
}

void EditorGUI::drawEntityInfo() {
    Entity entity = m_selectedEntity;
    if (entity.hasComponent<Component::Name>()) {
        std::string& name = entity.getComponent<Component::Name>().str;
        std::vector<char> buf(name.begin(), name.end());
        buf.resize(128); buf.back() = '\0';
        if (ImGui::InputText("Name", buf.data(), buf.size()-1)) {
            name = std::string(buf.data());
        }
    }
    if (entity.hasComponent<Component::Transform>()) {
        if (ImGui::TreeNode("Transform")) {

            static bool rotEuler = true;

            bool recalc = ImGui::InputFloat3("Position", glm::value_ptr(m_entityTransform.position));

            if (rotEuler) {
                if (ImGui::InputFloat3("Rotation", glm::value_ptr(m_entityTransform.rotationEuler))) {
                    m_entityTransform.rotation = glm::quat(glm::radians(m_entityTransform.rotationEuler));
                    recalc = true;
                }
            } else {
                if (ImGui::InputFloat4("Rotation", glm::value_ptr(m_entityTransform.rotation))) {
                    m_entityTransform.rotationEuler = glm::degrees(glm::eulerAngles(m_entityTransform.rotation));
                    recalc = true;
                }
            }
            recalc = ImGui::InputFloat3("Scale", glm::value_ptr(m_entityTransform.scale)) || recalc;
            ImGui::Checkbox("Show rotation as Euler angles", &rotEuler);

            if (recalc) {
                applyEntityTransform();
                m_entityTransformCache = m_entityTransform;
            }

            ImGui::TreePop();
        }
    } else {
        m_entityTransform.entity.id = entt::null;
    }
    //if (entity.hasComponent<Component::RenderableID>()) {
    if (entity.hasComponent<Component::Renderable>()) {
        if (ImGui::TreeNode("Renderable")) {
            //Renderable* pRenderable = m_pScene->getRenderable(entity.getComponent<Component::RenderableID>().id);
            Component::Renderable& rc = entity.getComponent<Component::Renderable>();
            if (rc.pModel) {
                std::string modelName = rc.pModel->getMesh()->getName();
                if (modelName.empty()) modelName = "Untitled model";
                ImGui::Text("Model: ");
                ImGui::SameLine();
                if (ImGui::Button(modelName.c_str())) ImGui::OpenPopup("Select Model");
            } else {
                if (ImGui::Button("Select Model")) ImGui::OpenPopup("Select Model");
                if (ImGui::Button("New MeshBuilder")) {
                    auto& pModel = m_pResManager->pModels.emplace_back(std::make_unique<Model>());
                    auto& pMesh = m_pResManager->pMeshes.emplace_back(std::make_unique<Mesh>());
                    pModel->setMesh(pMesh.get());
                    m_modelMeshBuilders.emplace(pModel.get(), m_pResManager->pMeshData.emplace_back(std::make_unique<MeshData>()).get());
                    m_pWindow->acquireContext();
                    pMesh->createFromMeshData(m_modelMeshBuilders[pModel.get()].getExternMeshData());
                    m_pWindow->releaseContext();
                    //entity.getComponent<Component::RenderableID>().id = m_pScene->addRenderable(Renderable().setModel(pModel));
                    rc.pModel = pModel.get();
                    m_pSelectedModel = pModel.get();
                }
            }
            if (ImGui::BeginPopup("Select Model")) {
                for (auto& pModel : m_pResManager->pModels) {
                    std::string modelName = pModel->getMesh()->getName();
                    if (modelName.empty()) modelName = "Untitled model";
                    if (ImGui::MenuItem(modelName.c_str(), nullptr, (rc.pModel == pModel.get()))) {
                        rc.pModel = pModel.get();
                        m_pSelectedModel = pModel.get();
                    }
                }
                if (ImGui::MenuItem("None", nullptr, !rc.pModel)) {
                    rc.pModel = nullptr;
                    m_pSelectedModel = nullptr;
                }
                ImGui::EndPopup();
            }
            ImGui::TreePop();
        }
    }
    if (entity.hasComponent<PointLight>()) {
        if (ImGui::TreeNode("Point Light")) {
            //PointLight* pLight = m_pScene->getPointLight(entity.getComponent<Component::PointLightID>().id);
            PointLight& light = entity.getComponent<PointLight>();

            glm::vec3 color = light.getIntensity();
            float intensity = std::max({color.r, color.g, color.b, 1.0f});
            color /= intensity;
            bool iedit = ImGui::ColorEdit3("Color", glm::value_ptr(color));
            iedit = ImGui::DragFloat("Intensity", &intensity) || iedit;
            if (iedit) {
                light.setIntensity(intensity * color);
            }
            bool shadows = light.isShadowMapEnabled();
            if (ImGui::Checkbox("Shadows Enabled", &shadows)) {
                light.setShadowMapEnabled(shadows);
            }

            ImGui::TreePop();
        }
    }
    if (entity.hasComponent<Component::AnimationStateID>()) {
        if (ImGui::TreeNode("Animation State")) {
            uint32_t& id = entity.getComponent<Component::AnimationStateID>().id;
            if (id == UINT_MAX) {
                if (ImGui::Button("Create Instance")) {
                    Skeleton* pSkeleton = nullptr;
                    /*if (entity.hasComponent<Component::RenderableID>()) {
                        if (Renderable* pRenderable = m_pScene->getRenderable(entity.getComponent<Component::RenderableID>().id)) {
                            pSkeleton = pRenderable->getSkeleton();
                        }
                    }*/
                    if (entity.hasComponent<Component::Renderable>()) {
                        pSkeleton = entity.getComponent<Component::Renderable>().pSkeleton;
                    }
                    if (pSkeleton) {
                        ImGui::OpenPopup("Select State Graph");
                    } else {
                        ImGui::OpenPopup("No Skeleton");
                    }
                }
                if (ImGui::BeginPopup("Select State Graph")) {
                    Skeleton* pSkeleton = entity.getComponent<Component::Renderable>().pSkeleton;

                    for (const auto& pStateGraph : m_pResManager->pAnimationStateGraphs) {
                        if (ImGui::MenuItem(pStateGraph->getName().c_str())) {
                            id = m_pAnimation->createAnimationStateInstance(pStateGraph.get(), pSkeleton);
                            entity.getComponent<Component::AnimationStateID>().pStateGraph = pStateGraph.get();
                        }
                    }
                    ImGui::MenuItem("Cancel");
                    ImGui::EndPopup();
                }
                if (ImGui::BeginPopupModal("No Skeleton", nullptr, ImGuiWindowFlags_AlwaysAutoResize |
                                                                   ImGuiWindowFlags_NoDecoration |
                                                                   ImGuiWindowFlags_NoMove |
                                                                   ImGuiWindowFlags_NoResize)) {
                    ImGui::Text("Can't add an animation instance because the selected entity has no skeleton.");
                    if (ImGui::Button("Damn, ok.")) ImGui::CloseCurrentPopup();
                    ImGui::EndPopup();
                }
            } else {
                const AnimationStateGraph* pStateGraph = entity.getComponent<Component::AnimationStateID>().pStateGraph;
                if (ImGui::CollapsingHeader("Blend Parameters")) {
                    for (auto kv : pStateGraph->getParameterIndexMap()) {
                        float value = m_pAnimation->getBlendParameter(id, kv.first);
                        if (ImGui::InputFloat(kv.first.c_str(), &value)) {
                            m_pAnimation->setBlendParameter(id, kv.first, value);
                        }
                    }
                }
                if (ImGui::CollapsingHeader("Trigger Flags")) {
                    for (auto kv : pStateGraph->getFlagIndexMap()) {
                        if (ImGui::Button(kv.first.c_str())) {
                            m_pAnimation->setFlag(id, kv.first);
                        }
                    }
                }
            }
            ImGui::TreePop();
        }
    }
}

static void drawMaterialInfo(Material* pMaterial, ResourceManager* pResManager) {
    ImGui::PushID(pMaterial);
    {
        std::string name = pMaterial->getName();
        std::vector<char> buf(name.begin(), name.end());
        buf.resize(32);
        if (ImGui::InputText("Name", buf.data(), buf.size())) {
            name = std::string(buf.data());
            pMaterial->setName(name);
        }
    }

    glm::vec3 diffuse = pMaterial->getTintColor();
    if (ImGui::ColorEdit3("Diffuse", glm::value_ptr(diffuse))) {
        pMaterial->setTintColor(diffuse);
    }

    ImGui::Text("Diffuse Texture");
    ImGui::PushID("diff");
    ImGui::SameLine();
    const Texture* pDiffuseTexture = nullptr;
    if (selectTexture(pResManager, pMaterial->getDiffuseTexture(), pDiffuseTexture))
        pMaterial->setDiffuseTexture(pDiffuseTexture);
    if ((pDiffuseTexture = pMaterial->getDiffuseTexture())) {
        ImGui::Image((void*)(intptr_t)pDiffuseTexture->getHandle(), ImVec2(100, 100));
    }
    ImGui::PopID();

    glm::vec3 emissive = pMaterial->getEmissionIntensity();
    float intensity = std::max({emissive.r, emissive.g, emissive.b, 1.0f});
    emissive /= intensity;
    bool changed = ImGui::ColorEdit3("Emissive Color", glm::value_ptr(emissive));
    changed = ImGui::DragFloat("Emissive Intensity", &intensity) || changed;
    if (changed) pMaterial->setEmissionIntensity(emissive * intensity);

    ImGui::Text("Emissive Texture");
    ImGui::PushID("em");
    ImGui::SameLine();
    const Texture* pEmissiveTexture = nullptr;
    if (selectTexture(pResManager, pMaterial->getEmissionTexture(), pEmissiveTexture))
        pMaterial->setEmissionTexture(pEmissiveTexture);
    if ((pEmissiveTexture = pMaterial->getEmissionTexture())) {
        ImGui::Image((void*)(intptr_t)pEmissiveTexture->getHandle(), ImVec2(100, 100));
    }
    ImGui::PopID();

    float metallic = pMaterial->getMetallic();
    if (ImGui::SliderFloat("Metallic", &metallic, 0.0, 1.0)) pMaterial->setMetallic(metallic);

    float roughness = pMaterial->getRoughness();
    if (ImGui::SliderFloat("Roughness", &roughness, 0.0, 1.0)) pMaterial->setRoughness(roughness);

    ImGui::Text("Metallic/Roughness Texture");
    ImGui::PushID("mr");
    ImGui::SameLine();
    const Texture* pMRTexture = nullptr;
    if (selectTexture(pResManager, pMaterial->getMetallicRoughnessTexture(), pMRTexture))
        pMaterial->setMetallicRoughnessTexture(pMRTexture);
    if ((pMRTexture = pMaterial->getMetallicRoughnessTexture())) {
        ImGui::Image((void*)(intptr_t)pMRTexture->getHandle(), ImVec2(100, 100));
    }
    ImGui::PopID();

    float alpha = pMaterial->getAlpha();
    if (ImGui::SliderFloat("Alpha", &alpha, 0.0, 1.0)) {
        if (alpha < 1.0) {
            pMaterial->setTransparencyEnabled(true);
            pMaterial->setAlphaMaskThreshold(1.0);
        }
        pMaterial->setAlpha(alpha);
    }

    bool transparent = pMaterial->isTransparencyEnabled();
    if (ImGui::Checkbox("Enable Transparency", &transparent))
        pMaterial->setTransparencyEnabled(transparent);

    float alphaThreshold = pMaterial->getAlphaMaskThreshold();
    if (ImGui::SliderFloat("Max Transparent Alpha", &alphaThreshold, 0.0, 1.0))
        pMaterial->setAlphaMaskThreshold(alphaThreshold);

    ImGui::Text("Normal Map");
    ImGui::PushID("nrm");
    ImGui::SameLine();
    const Texture* pNormalsTexture = nullptr;
    if (selectTexture(pResManager, pMaterial->getNormalsTexture(), pNormalsTexture))
        pMaterial->setNormalsTexture(pNormalsTexture);
    if ((pNormalsTexture = pMaterial->getNormalsTexture())) {
        ImGui::Image((void*)(intptr_t)pNormalsTexture->getHandle(), ImVec2(100, 100));
    }
    ImGui::PopID();

    bool castShadows = pMaterial->isShadowCastingEnabled();
    if (ImGui::Checkbox("Cast Shadows", &castShadows))
        pMaterial->setShadowCastingEnabled(castShadows);

    bool objectMotionVectors = pMaterial->useObjectMotionVectors();
    if (ImGui::Checkbox("Use Per-Object Motion Vectors", &objectMotionVectors))
        pMaterial->setObjectMotionVectors(objectMotionVectors);

    ImGui::PopID();
}

static MeshBuilderNodes::Node* addMeshBuilderNode(MeshBuilderNodes::Node* pNext) {
    if (ImGui::Button("Add Node")) ImGui::OpenPopup("Add Node");
    MeshBuilderNodes::Node* pNode = nullptr;
    if (ImGui::BeginPopup("Add Node")) {
        if (ImGui::MenuItem("Add")) pNode = new MeshBuilderNodes::Add(pNext);
        if (ImGui::MenuItem("Calculate Normals")) pNode = new MeshBuilderNodes::CalcNormals(pNext);
        if (ImGui::MenuItem("Calculate Tangent Space")) pNode = new MeshBuilderNodes::CalcTangent(pNext);
        if (ImGui::MenuItem("Capsule Primitive")) pNode = new MeshBuilderNodes::PrimCapsule(pNext);
        if (ImGui::MenuItem("Cube Primitive")) pNode = new MeshBuilderNodes::PrimCube(pNext);
        if (ImGui::MenuItem("Cylinder Primitive")) pNode = new MeshBuilderNodes::PrimCylinder(pNext);
        if (ImGui::MenuItem("Hemisphere Primitive")) pNode = new MeshBuilderNodes::PrimHemisphere(pNext);
        if (ImGui::MenuItem("Plane Primitive")) pNode = new MeshBuilderNodes::PrimPlane(pNext);
        if (ImGui::MenuItem("Sphere Primitive")) pNode = new MeshBuilderNodes::PrimSphere(pNext);
        if (ImGui::MenuItem("Rotate")) pNode = new MeshBuilderNodes::Rotate(pNext);
        if (ImGui::MenuItem("Scale")) pNode = new MeshBuilderNodes::Scale(pNext);
        if (ImGui::MenuItem("Translate")) pNode = new MeshBuilderNodes::Translate(pNext);
        if (ImGui::MenuItem("UV Map Planar")) pNode = new MeshBuilderNodes::UVMapPlanar(pNext);
        ImGui::EndPopup();
    }
    return pNode;
}

static bool drawMeshBuilderNode(MeshBuilderNodes::Node* pNode, MeshBuilderNodes::Node*& pNewRoot) {
    bool ret = false;
    if (pNode->pNext) ret = drawMeshBuilderNode(pNode->pNext, pNewRoot);

    static const std::map<MeshBuilderNodes::Operation, const char*> operationNames = {
        { MeshBuilderNodes::ADD, "Add" },
        { MeshBuilderNodes::CALC_NORMALS, "Calculate Normals" },
        { MeshBuilderNodes::CALC_TANGENT, "Calculate Tangent Space" },
        { MeshBuilderNodes::PRIM_CAPSULE, "Capsule Primitive" },
        { MeshBuilderNodes::PRIM_CUBE, "Cube Primitive" },
        { MeshBuilderNodes::PRIM_CYLINDER, "Cylinder Primitive" },
        { MeshBuilderNodes::PRIM_HEMI, "Hemisphere Primitive" },
        { MeshBuilderNodes::PRIM_PLANE, "Plane Primitive" },
        { MeshBuilderNodes::PRIM_SPHERE, "Sphere Primitive" },
        { MeshBuilderNodes::ROTATE, "Rotate" },
        { MeshBuilderNodes::SCALE, "Scale" },
        { MeshBuilderNodes::TRANSLATE, "Translate" },
        { MeshBuilderNodes::UV_PLANAR, "UV Map Planar" }
    };

    ImGui::PushID(pNode);
    bool open = ImGui::TreeNode(operationNames.at(pNode->operation));
    bool deleteNode = false;

    if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) ImGui::OpenPopup("Node Menu");
    if (ImGui::BeginPopup("Node Menu")) {
        if (ImGui::MenuItem("Move Up", nullptr, false, pNode->pNext)) {
            if (pNode->pPrev) {
                pNode->pPrev->pNext = pNode->pNext;
            } else {
                pNewRoot = pNode->pNext;
            }
            pNode->pNext->pPrev = pNode->pPrev;
            pNode->pPrev = pNode->pNext;
            pNode->pNext = pNode->pNext->pNext;
            pNode->pPrev->pNext = pNode;
            if (pNode->pNext) pNode->pNext->pPrev = pNode;
            ret = true;
        }
        if (ImGui::MenuItem("Move Down", nullptr, false, pNode->pPrev)) {
            if (pNode->pPrev->pPrev) {
                pNode->pPrev->pPrev->pNext = pNode;
            } else {
                pNewRoot = pNode;
            }
            pNode->pPrev->pNext = pNode->pNext;
            if (pNode->pNext) pNode->pNext->pPrev = pNode->pPrev;
            pNode->pNext = pNode->pPrev;
            pNode->pPrev = pNode->pPrev->pPrev;
            pNode->pNext->pPrev = pNode;
            ret = true;
        }
        if (ImGui::MenuItem("Delete")) {
            if (pNode->pPrev) pNode->pPrev->pNext = pNode->pNext;
            else pNewRoot = pNode->pNext;

            if (pNode->pNext) pNode->pNext->pPrev = pNode->pPrev;
            pNode->pNext = nullptr;

            deleteNode = true;
            ret = true;
        }
        ImGui::EndPopup();
    }

    if (open) {
        // render the settings per operation
        // operations with no settings excluded
        switch (pNode->operation) {
        case MeshBuilderNodes::ADD: {
            MeshBuilderNodes::Add* pAdd = static_cast<MeshBuilderNodes::Add*>(pNode);
            if (pAdd->pChild) {
                MeshBuilderNodes::Node* pNewChildRoot = pAdd->pChild;
                ret = ret || drawMeshBuilderNode(pAdd->pChild, pNewChildRoot);
                pAdd->pChild = pNewChildRoot;
            }
            MeshBuilderNodes::Node* pNextChild = addMeshBuilderNode(pAdd->pChild);
            if (pNextChild) {
                pAdd->pChild = pNextChild;
                ret = true;
            }
            break;
        }
        case MeshBuilderNodes::PRIM_CAPSULE: {
            MeshBuilderNodes::PrimCapsule* pPrimCapsule = static_cast<MeshBuilderNodes::PrimCapsule*>(pNode);
            ret = ret ||
                ImGui::InputFloat("Radius", &pPrimCapsule->radius) ||
                ImGui::InputFloat("Height", &pPrimCapsule->height) ||
                ImGui::InputInt("Circle Points", &pPrimCapsule->circlePoints) ||
                ImGui::InputInt("Segments", &pPrimCapsule->segments) ||
                ImGui::InputInt("Hemisphere Rings", &pPrimCapsule->hemisphereRings);
            break;
        }
        case MeshBuilderNodes::PRIM_CUBE: {
            MeshBuilderNodes::PrimCube* pPrimCube = static_cast<MeshBuilderNodes::PrimCube*>(pNode);
            ret = ret || ImGui::InputFloat("Width", &pPrimCube->width);
            break;
        }
        case MeshBuilderNodes::PRIM_CYLINDER: {
            MeshBuilderNodes::PrimCylinder* pPrimCylinder = static_cast<MeshBuilderNodes::PrimCylinder*>(pNode);
            ret = ret ||
                ImGui::InputFloat("Radius", &pPrimCylinder->radius) ||
                ImGui::InputFloat("Height", &pPrimCylinder->height) ||
                ImGui::InputInt("Circle Points", &pPrimCylinder->circlePoints) ||
                ImGui::InputInt("Segments", &pPrimCylinder->segments) ||
                ImGui::Checkbox("Make Closed", &pPrimCylinder->makeClosed);
            break;
        }
        case MeshBuilderNodes::PRIM_HEMI: {
            MeshBuilderNodes::PrimHemisphere* pPrimHemi = static_cast<MeshBuilderNodes::PrimHemisphere*>(pNode);
            ret = ret ||
                ImGui::InputFloat("Radius", &pPrimHemi->radius) ||
                ImGui::InputInt("Circle Points", &pPrimHemi->circlePoints) ||
                ImGui::InputInt("Rings", &pPrimHemi->rings) ||
                ImGui::Checkbox("Make Closed", &pPrimHemi->makeClosed);
            break;
        }
        case MeshBuilderNodes::PRIM_PLANE: {
            MeshBuilderNodes::PrimPlane* pPrimPlane = static_cast<MeshBuilderNodes::PrimPlane*>(pNode);
            ret = ret || ImGui::InputFloat("Width", &pPrimPlane->width);
            break;
        }
        case MeshBuilderNodes::PRIM_SPHERE: {
            MeshBuilderNodes::PrimSphere* pPrimSphere = static_cast<MeshBuilderNodes::PrimSphere*>(pNode);
            ret = ret ||
                ImGui::InputFloat("Radius", &pPrimSphere->radius) ||
                ImGui::InputInt("Circle Points", &pPrimSphere->circlePoints) ||
                ImGui::InputInt("Rings", &pPrimSphere->rings);
            break;
        }
        case MeshBuilderNodes::ROTATE: {
            MeshBuilderNodes::Rotate* pRotate = static_cast<MeshBuilderNodes::Rotate*>(pNode);
            glm::vec3 rotationEuler = glm::degrees(glm::eulerAngles(pRotate->rotation));
            if (ImGui::InputFloat3("Rotation", glm::value_ptr(rotationEuler))) {
                pRotate->rotation = glm::quat(glm::radians(rotationEuler));
                ret = true;
            }
            break;
        }
        case MeshBuilderNodes::SCALE: {
            MeshBuilderNodes::Scale* pScale = static_cast<MeshBuilderNodes::Scale*>(pNode);
            ret = ret || ImGui::InputFloat3("Scale", glm::value_ptr(pScale->scale));
            break;
        }
        case MeshBuilderNodes::TRANSLATE: {
            MeshBuilderNodes::Translate* pTranslate = static_cast<MeshBuilderNodes::Translate*>(pNode);
            ret = ret || ImGui::InputFloat3("Translation", glm::value_ptr(pTranslate->translation));
            break;
        }
        case MeshBuilderNodes::UV_PLANAR: {
            MeshBuilderNodes::UVMapPlanar* pUVPlanar = static_cast<MeshBuilderNodes::UVMapPlanar*>(pNode);
            if (ImGui::InputFloat3("Normal", glm::value_ptr(pUVPlanar->normal))) {
                pUVPlanar->normal = glm::normalize(pUVPlanar->normal);
                ret = true;
            }
            break;
        }
        default:
            // nothing
            break;
        }

        ImGui::TreePop();
    }
    ImGui::PopID();

    if (deleteNode) delete pNode;

    return ret;
}

static bool drawMeshBuilderNodes(MeshBuilderNodes& nodes) {
    bool ret = false;
    if (nodes.pRootNode) {
        MeshBuilderNodes::Node* pNewRoot = nodes.pRootNode;
        ret = drawMeshBuilderNode(nodes.pRootNode, pNewRoot);
        nodes.pRootNode = pNewRoot;
    }

    MeshBuilderNodes::Node* pNextRoot = addMeshBuilderNode(nodes.pRootNode);
    if (pNextRoot) {
        nodes.pRootNode = pNextRoot;
        ret = true;
    }
    return ret;
}

static Entity instantiateAssetNode(const Asset::Node* pNode, GameWorld* pGameWorld, Scene* pScene, std::unordered_map<const SkeletonDescription*, Skeleton*>& pSkeletonMap) {
    Entity entity = pGameWorld->createEntity();
    entity.addComponent<Component::Transform>().local = pNode->transform;
    entity.addComponent<Component::Name>().str = pNode->name;
    if (pNode->pModel) {
        Skeleton* pSkeleton = nullptr;
        if (const SkeletonDescription* pSD = pNode->pModel->getSkeletonDescription(); pSD) {
            if (auto it = pSkeletonMap.find(pSD); it != pSkeletonMap.end()) {
                pSkeleton = it->second;
            } else {
                pSkeleton = pScene->addSkeleton(pSD);
                pSkeletonMap[pSD] = pSkeleton;
            }
        }
        /*entity.addComponent<Component::RenderableID>(pScene).id =
            pScene->addRenderable(Renderable().setModel(pNode->pModel).setSkeleton(pSkeleton));*/
        Component::Renderable& rc = entity.addComponent<Component::Renderable>();
        rc.pModel = pNode->pModel;
        rc.pSkeleton = pSkeleton;
        if (rc.pSkeleton) entity.addComponent<Component::Renderable::SkeletalFlag>();
    }
    for (Asset::Node* pChildNode : pNode->pChildren) {
        Entity childEntity = instantiateAssetNode(pChildNode, pGameWorld, pScene, pSkeletonMap);
        pGameWorld->setParent(childEntity, entity);
    }
    return entity;
}

static Entity instantiateAsset(const Asset* pAsset, GameWorld* pGameWorld, Scene* pScene) {
    std::unordered_map<const SkeletonDescription*, Skeleton*> skeletonMap;
    return instantiateAssetNode(pAsset->pRootNode, pGameWorld, pScene, skeletonMap);
}

static Entity duplicateEntity(const Entity entity, GameWorld* pGameWorld, Scene* pScene, std::unordered_map<const Skeleton*, Skeleton*>& skeletonMap) {
    Entity e = pGameWorld->createEntity();
    if (entity.hasComponent<Component::Name>()) {
        e.addComponent<Component::Name>().str =
            entity.getComponent<Component::Name>().str + " duplicate";
    }
    if (entity.hasComponent<Component::Transform>()) {
        // Cache the transform because for some reason the line:
        // e.addComponent<Component::Transform>().local = entity.getComponent<Component::Transform>().local
        // Was causing heap-use-after-free corruption
        // I can't say for sure why but one guess is the compiler decided to grab a reference to
        // entity.getComponent() before calling e.addComponent() (which eventually calls vector::emplace and potentially reallocates the heap buffer)
        // and didn't realize the pointer would be invalidated by reallocation
        // Maybe as an optimization it was able to get the reference and hang onto it when determining if the component exists?
        // I don't know if this makes sense also I find it suspicious this part only seems to break while similar lines (see the Name section above)
        // continue to work. Also, this definitely used to work at one point so I'm double suspicious. Might actually be memory corruption elsewhere
        // Anyway the way you see here has been working and does the same thing, soo..
        glm::mat4 local = entity.getComponent<Component::Transform>().local;
        e.addComponent<Component::Transform>().local = local;
    }
    if (entity.hasComponent<Component::Renderable>()) {
        Component::Renderable& new_rc = e.addComponent<Component::Renderable>();
        const Component::Renderable& old_rc = entity.getComponent<Component::Renderable>();
        new_rc.pModel = old_rc.pModel;
        if (old_rc.pSkeleton) {
            if (auto it = skeletonMap.find(old_rc.pSkeleton); it != skeletonMap.end()) {
                new_rc.pSkeleton = it->second;
            } else {
                // TODO: Not this! Manage lifetimes somehow
                new_rc.pSkeleton = new Skeleton(old_rc.pSkeleton->getDescription());
                skeletonMap[old_rc.pSkeleton] = new_rc.pSkeleton;
            }
        }
    }
    if (entity.hasComponent<PointLight>()) {
        e.addComponent<PointLight>(entity.getComponent<PointLight>());
    }
    if (entity.hasComponent<Component::Children>()) {
        for (Entity child : entity.getComponent<Component::Children>().entities) {
            pGameWorld->setParent(duplicateEntity(child, pGameWorld, pScene, skeletonMap), e);
        }
    }
    return e;
}

static Entity duplicateEntity(const Entity entity, GameWorld* pGameWorld, Scene* pScene) {
    std::unordered_map<const Skeleton*, Skeleton*> skeletonMap;
    return duplicateEntity(entity, pGameWorld, pScene, skeletonMap);
}

void EditorGUI::update() {
    float menuHeight = 0.0f;

    bool importError = false;
    if (ImGui::BeginMainMenuBar()) {
        menuHeight = ImGui::GetWindowHeight();
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("Import")) {
                std::string fileName;
                if (FileDialog::open(fileName, m_pWindow)) {
                    auto filePath = std::filesystem::path(fileName);
                    auto extension = filePath.extension();
                    std::string extStr(extension);
                    std::transform(extStr.begin(), extStr.end(), extStr.begin(), [] (auto c) { return std::tolower(c); });
                    std::string imageFileExts[] = {".jpg", ".jpeg", ".png", ".tga", ".pnm", ".pbm", ".pgm", ".gif", ".psd", ".bmp"};
                    m_pWindow->acquireContext();
                    if (std::find(imageFileExts, imageFileExts+10, extStr) != imageFileExts+10) {
                        Texture texture;
                        if (loadTexture(fileName, &texture))
                            m_pResManager->pTextures.emplace_back(new Texture(std::move(texture)));
                    } else {
                        // treat it as an asset
                        bool err = false;
                        if (extStr == std::string(".obj")) {
                            err = (importOBJ(fileName, m_pResManager) == nullptr);
                        } else {
                            err = (importAsset(fileName, m_pResManager) == nullptr);
                        }
                        if (err) {
                            importError = true;
                        }
                    }
                    m_pWindow->releaseContext();
                }
            }
            if (ImGui::MenuItem("Import Animation JSON")) {
                std::string fileName;
                if (FileDialog::open(fileName, m_pWindow)) {
                    importError = (loadAnimationStateGraph(fileName, m_pResManager) == nullptr);
                }
            }
            ImGui::EndMenu();
        }
        ImGui::EndMainMenuBar();
    }
    if (importError) ImGui::OpenPopup("Import Error");
    if (ImGui::BeginPopupModal("Import Error", nullptr,
                               ImGuiWindowFlags_AlwaysAutoResize |
                               ImGuiWindowFlags_NoResize |
                               ImGuiWindowFlags_NoMove)) {
        ImGui::Text("Failed to import asset.");
        if (ImGui::Button("Bummer")) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    ImGuiWindowFlags metaWindowFlags =
        //ImGuiWindowFlags_NoBackground |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoDecoration |
        ImGuiWindowFlags_NoScrollWithMouse;
    ImGui::SetNextWindowPos(ImVec2(0, menuHeight));
    ImGui::SetNextWindowSize(ImVec2(m_pWindow->getWidth(), m_pWindow->getHeight() - menuHeight));
    ImGui::SetNextWindowBgAlpha(0.0);
    ImGui::Begin("Meta Window", nullptr, metaWindowFlags);


    ImGuiTableFlags layoutTableFlags =
        ImGuiTableFlags_Resizable  |
        ImGuiTableFlags_NoPadInnerX | ImGuiTableFlags_NoPadOuterX |
        ImGuiTableFlags_NoBordersInBodyUntilResize;
    ImGui::BeginTable("Layout Table", 3, layoutTableFlags);

    ImGui::TableNextColumn();

    leftPanel();

    ImGui::TableNextColumn();

    centerPanel();

    ImGui::TableNextColumn();

    rightPanel();

    ImGui::EndTable();


    ImGui::End();


    //ImGui::ShowDemoWindow();

}

void EditorGUI::render() {
    m_pWindow->acquireContext();
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    m_pWindow->releaseContext();
}

void EditorGUI::initialize() {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui_ImplGlfw_InitForOpenGL(m_pWindow->getHandle(), true);
    ImGui_ImplOpenGL3_Init("#version 330");
    ImGui::StyleColorsDark();
    m_initialized = true;
}

void EditorGUI::leftPanel() {
    ImGui::SetNextWindowBgAlpha(0.8);
    ImGui::BeginChild("Left Panel", ImVec2(0, 0), false, ImGuiWindowFlags_NoScrollbar);

    /*if(ImGui::CollapsingHeader("Clouds Render Settings")) {
        ImGui::Text("Shader Settings");

        ImGui::SliderFloat("cloudiness", &m_pRenderer->cloudiness, 0.0, 1.0);
        ImGui::DragFloat("density", &m_pRenderer->cloudDensity, 0.025, 0.0);
        ImGui::DragFloat("extinction", &m_pRenderer->cloudExtinction, 0.001, 0.0);
        ImGui::DragFloat("scattering", &m_pRenderer->cloudScattering, 0.001, 0.0);
        ImGui::DragFloat("base noise scale", &m_pRenderer->cloudNoiseBaseScale, 0.000001, 0.0, 1.0, "%.5e");
        ImGui::DragFloat("detail noise scale", &m_pRenderer->cloudNoiseDetailScale, 0.000001, 0.0, 1.0, "%.5e");
        ImGui::DragFloat("detail noise influence", &m_pRenderer->cloudDetailNoiseInfluence, 0.01, 0.0, 1.0);
        ImGui::DragFloat4("base noise contribution", &m_pRenderer->cloudBaseNoiseContribution[0], 0.01);
        ImGui::DragFloat4("base noise threshold", &m_pRenderer->cloudBaseNoiseThreshold[0], 0.01);
        ImGui::DragFloat("bottom height", &m_pRenderer->cloudBottomHeight, 50.0, 0.0);
        ImGui::DragFloat("top height", &m_pRenderer->cloudTopHeight, 50.0, 0.0);
        ImGui::DragFloat("planet radius", &m_pRenderer->planetRadius, 1000.0, 0.0);
        ImGui::DragInt("steps", &m_pRenderer->cloudSteps);
        ImGui::DragInt("shadow steps", &m_pRenderer->cloudShadowSteps);
        ImGui::DragFloat("shadow step size", &m_pRenderer->cloudShadowStepSize, 5.0, 0.0);

        ImGui::Text("Noise Generation Settings");
        ImGui::DragInt("base noise resolution", &m_pRenderer->cloudBaseNoiseWidth);
        ImGui::DragFloat("base noise fbm lowest frequency", &m_pRenderer->cloudBaseNoiseFrequency, 0.0001, 0.0, 1.0, "%.5e");
        ImGui::DragInt("base noise fbm octaves", &m_pRenderer->cloudBaseNoiseOctaves);
        ImGui::DragInt3("base noise worley layer cell dimension", &m_pRenderer->cloudBaseWorleyLayerPoints[0]);

        if (ImGui::Button("recreate noise textures")) {
            m_pRenderer->rebuildCloudTextures = true;
        }
        if (ImGui::Button("reload shader source")) {
            m_pRenderer->reloadCloudShader = true;
        }
        if (ImGui::Button("toggle TAA")) {
            m_pRenderer->toggleTAA();
        }
        if (ImGui::Button("toggle clouds")) {
            m_pRenderer->toggleBackground();
        }

        ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
    }*/

    if (ImGui::CollapsingHeader("Scene Settings")) {
        glm::vec3 cameraPosition = m_pScene->getActiveCamera()->getPosition();
        if (ImGui::DragFloat3("Camera position", &cameraPosition[0], 0.05f)) {
            m_pScene->getActiveCamera()->setPosition(cameraPosition);
        }

        glm::vec3 cameraDirection = m_pScene->getActiveCamera()->getDirection();
        if (ImGui::DragFloat3("Camera direction", &cameraDirection[0], 0.05f)) {
            m_pScene->getActiveCamera()->setDirection(cameraDirection);
        }

        glm::vec3 lightDirection = m_pScene->getDirectionalLight().getDirection();
        if (ImGui::DragFloat3("Light direction", &lightDirection[0], 0.05f)) {
            m_pScene->getDirectionalLight().setDirection(lightDirection);
        }

        glm::vec3 lightIntensity = m_pScene->getDirectionalLight().getIntensity();
        if (ImGui::DragFloat3("Light intensity", &lightIntensity[0], 0.05f, 0.0f)) {
            m_pScene->getDirectionalLight().setIntensity(lightIntensity);
        }

        glm::vec3 ambientLight = m_pScene->getAmbientLightIntensity();
        if (ImGui::DragFloat3("Ambient light intensity", &ambientLight[0], 0.05f, 0.0f)) {
            m_pScene->setAmbientLightIntensity(ambientLight);
        }
    }


    if (ImGui::CollapsingHeader("Entity Hierarchy")) {
        // recursively call entityNode for all entity trees, starting with the TopLevel entities
        bool openMenu = false;
        Entity selectedEntity = m_selectedEntity;
        for (auto e : m_pGameWorld->m_registry.view<Component::TopLevel>()) {
            entityNode({e, m_pGameWorld}, selectedEntity, openMenu);
        }
        if (selectedEntity.isValid() && selectedEntity != m_selectedEntity) {
            onDeselectEntity();
            m_selectedEntity = selectedEntity;
            onSelectEntity();
        }
        if (openMenu) ImGui::OpenPopup("Entity Menu");
        if (ImGui::BeginPopup("Entity Menu")) {
            if (ImGui::BeginMenu("Set Parent...", m_pGameWorld->m_registry.size() > 1)) {

                auto p = m_selectedEntity.hasComponent<Component::Parent>()?
                    m_selectedEntity.getComponent<Component::Parent>().entity.id : entt::null;

                m_pGameWorld->m_registry.each(
                    [&,this] (const auto e) {
                        Entity entity = {e, m_pGameWorld};

                        bool isChild = false;
                        if (entity.hasComponent<Component::Parent>()) {
                            Entity parent = entity.getComponent<Component::Parent>().entity;
                            do {
                                if (parent == m_selectedEntity) {
                                    isChild = true;
                                    break;
                                }
                                if (parent.hasComponent<Component::Parent>()) {
                                    parent = parent.getComponent<Component::Parent>().entity;
                                    continue;
                                }
                                break;
                            } while (true);
                        }
                        std::string name = entity.hasComponent<Component::Name>()?
                            entity.getComponent<Component::Name>().str : "Entity";

                        if (ImGui::MenuItem(name.c_str(), nullptr, e == p, e != m_selectedEntity.id && !isChild))
                            m_pGameWorld->setParent(m_selectedEntity, entity);
                    });
                ImGui::EndMenu();
            }
            if (ImGui::MenuItem("Clear Parent", nullptr, false, m_selectedEntity.hasComponent<Component::Parent>())) {
                m_pGameWorld->clearParent(m_selectedEntity);
            }
            if (ImGui::MenuItem("Duplicate")) {
                Entity entity = duplicateEntity(m_selectedEntity, m_pGameWorld, m_pScene);
                onDeselectEntity();
                m_selectedEntity = entity;
                onSelectEntity();
            }
            if (ImGui::MenuItem("Delete")) {
                onDeselectEntity();
                m_pGameWorld->destroyEntity(m_selectedEntity, ENTITY_DESTROY_REPARENT);
                m_selectedEntity.id = entt::null;
            }
            if (ImGui::MenuItem("Delete Hierarchy")) {
                onDeselectEntity();
                m_pGameWorld->destroyEntity(m_selectedEntity, ENTITY_DESTROY_HIERARCHY);
                m_selectedEntity.id = entt::null;
            }
            ImGui::EndPopup();
        }
        if (m_pGameWorld->m_registry.empty()) {
            ImGui::Text("Scene is empty");
        }
        if (ImGui::Button("Create Entity")) {
            if (m_selectedEntity.isValid()) onDeselectEntity();
            m_selectedEntity = m_pGameWorld->createEntity();
            onSelectEntity();
        }
    }

    if (ImGui::CollapsingHeader("Assets")) {
        bool newEntity = false;
        for (auto& pAsset : m_pResManager->pAssets) {
            ImGui::PushID(pAsset.get());
            std::string name = pAsset->name;
            if (name.empty()) name = "Untitled asset";
            bool open = ImGui::TreeNode(name.c_str());
            if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) {
                ImGui::SetDragDropPayload("Asset", pAsset.get(), sizeof(Asset));
                ImGui::Text("%s", name.c_str());
                ImGui::EndDragDropSource();
            }
            if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) ImGui::OpenPopup("Asset Menu");
            if (ImGui::BeginPopup("Asset Menu")) {
                if (ImGui::MenuItem("Instantiate")) {
                    if (m_selectedEntity.isValid()) onDeselectEntity();
                    m_selectedEntity = instantiateAsset(pAsset.get(), m_pGameWorld, m_pScene);
                    m_pSelectedModel = pAsset->pRootNode->pModel;
                    onSelectEntity();
                    newEntity = true;
                }
                ImGui::EndPopup();
            }
            if (open) {
                if (ImGui::TreeNode("Node Hierarchy")) {
                    if (pAsset->pRootNode)
                        assetNode(pAsset->pRootNode, m_pSelectedAssetNode);
                    else ImGui::Text("Asset is empty.");
                    ImGui::TreePop();
                }
                if (ImGui::TreeNode("Models")) {
                    for (Model* pModel : pAsset->pModels) {
                        std::string name = pModel->getMesh()->getName();
                        if (name.empty()) name = "Untitled model";
                        ImGuiTreeNodeFlags flags =
                            ImGuiTreeNodeFlags_Bullet |
                            ImGuiTreeNodeFlags_Leaf;
                        if (pModel == m_pSelectedModel) flags |= ImGuiTreeNodeFlags_Selected;
                        bool open = ImGui::TreeNodeEx(name.c_str(), flags);
                        if (ImGui::IsItemClicked()) {
                            m_pSelectedModel = pModel;
                            m_pSelectedAssetNode = nullptr;
                            m_selectedEntity.id = entt::null;
                        }
                        if (open) ImGui::TreePop();
                    }
                    ImGui::TreePop();
                }
                if (ImGui::TreeNode("Animation Clips")) {
                    for (AnimationClip* pClip : pAsset->pAnimationClips) {
                        std::string name = pClip->getName();
                        if (name.empty()) name = "Untitled clip";
                        ImGuiTreeNodeFlags flags =
                            ImGuiTreeNodeFlags_Bullet |
                            ImGuiTreeNodeFlags_Leaf;
                        if (ImGui::TreeNodeEx(name.c_str(), flags)) ImGui::TreePop();
                    }
                    ImGui::TreePop();
                }
                ImGui::TreePop();
            }
            ImGui::PopID();
        }
        if (m_pSelectedAssetNode && !newEntity) {
            m_selectedEntity.id = entt::null;
            m_pSelectedModel = m_pSelectedAssetNode->pModel;
        } else {
            m_pSelectedAssetNode = nullptr;
        }
        if (m_pResManager->pAssets.empty()) {
            ImGui::Text("No assets");
        }
    }

    ImGui::EndChild(); // Left Panel
}

void EditorGUI::centerPanel() {
    ImGui::BeginChild("Viewport Window", ImVec2(0, 0), false, ImGuiWindowFlags_NoBackground|ImGuiWindowFlags_NoDecoration);


    ImVec2 windowSize = ImGui::GetWindowSize();

    // Camera control
    {
        static int cameraInput = -1;
        if (ImGui::GetIO().MouseWheel != 0.0f)
            cameraInput = 0;
        else if (cameraInput == 0)
            cameraInput = -1;
        if (ImGui::IsMouseDragging(ImGuiMouseButton_Middle))
            cameraInput = 1;
        else if (cameraInput == 1)
            cameraInput = -1;
        if (ImGui::IsMouseDragging(ImGuiMouseButton_Right))
            cameraInput = 2;
        else if (cameraInput == 2)
            cameraInput = -1;

        static glm::vec3 cameraPosCache = m_pScene->getCamera(m_editorCameraID)->getPosition();
        static glm::vec3 cameraDirCache = m_pScene->getCamera(m_editorCameraID)->getDirection();
        static glm::vec3 cameraUpCache = m_pScene->getCamera(m_editorCameraID)->getUp();

        if (cameraInput != -1) {
            if (m_pScene->getActiveCameraID() != m_editorCameraID) {
                Camera& editorCamera = *m_pScene->getCamera(m_editorCameraID);
                Camera& activeCamera = *m_pScene->getActiveCamera();
                editorCamera.setPosition(activeCamera.getPosition());
                editorCamera.setDirection(activeCamera.getDirection());
                editorCamera.setUp(activeCamera.getUp());
                m_pScene->setActiveCamera(m_editorCameraID);

                cameraPosCache = m_pScene->getCamera(m_editorCameraID)->getPosition();
                cameraDirCache = m_pScene->getCamera(m_editorCameraID)->getDirection();
                cameraUpCache = m_pScene->getCamera(m_editorCameraID)->getUp();
            }
            Camera& camera = *m_pScene->getCamera(m_editorCameraID);

            glm::vec3 right = glm::normalize(glm::cross(cameraDirCache, cameraUpCache));
            glm::vec3 up = glm::cross(right, cameraDirCache);

            switch (cameraInput) {
            case 0: {  // Dolly
                camera.setPosition(camera.getPosition() + m_cameraDollySpeed * ImGui::GetIO().MouseWheel * cameraDirCache);
                break; }
            case 1: {  // Translate
                ImVec2 delta = ImGui::GetMouseDragDelta(ImGuiMouseButton_Middle);
                camera.setPosition(cameraPosCache - right * delta.x * m_cameraTranslateSpeed + up * delta.y * m_cameraTranslateSpeed);
                break; }
            case 2: {  // Rotate
                ImVec2 delta = ImGui::GetMouseDragDelta(ImGuiMouseButton_Right);
                glm::quat rotation = glm::angleAxis(delta.x * m_cameraRotateSpeed, up) * glm::angleAxis(delta.y * m_cameraRotateSpeed, right);
                camera.setDirection(glm::rotate(rotation, cameraDirCache));
                break; }
            default:  {
                assert(0);
                break; }
            }
        } else {
            cameraPosCache = m_pScene->getCamera(m_editorCameraID)->getPosition();
            cameraDirCache = m_pScene->getCamera(m_editorCameraID)->getDirection();
            cameraUpCache = m_pScene->getCamera(m_editorCameraID)->getUp();
        }
    }

    m_pWindow->acquireContext();
    m_pRenderer->setViewport(windowSize.x, windowSize.y);
    m_pWindow->releaseContext();
    m_pScene->getActiveCamera()->setAspectRatio(windowSize.x / windowSize.y);

    ImGui::Image((void*)(uintptr_t)m_pRenderer->getRenderTexture()->getHandle(), windowSize, ImVec2(0, 1), ImVec2(1, 0));

    ImGui::SetCursorPos(ImVec2(0, 0));

    if (ImGui::IsKeyPressed(GLFW_KEY_T)) m_transformInputGizmo = 0;
    if (ImGui::IsKeyPressed(GLFW_KEY_R)) m_transformInputGizmo = 1;
    if (ImGui::IsKeyPressed(GLFW_KEY_S)) m_transformInputGizmo = 2;
    if (m_selectedEntity.isValid() && m_selectedEntity.hasComponent<Component::Transform>()) {
        drawTransformGizmo();
        ImGui::SameLine();
    }

    ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);

    ImGui::EndChild();

    if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* pPayload = ImGui::AcceptDragDropPayload("Asset")) {
            if (m_selectedEntity.isValid()) onDeselectEntity();
            m_selectedEntity = instantiateAsset((Asset*) pPayload->Data, m_pGameWorld, m_pScene);
            onSelectEntity();
        }
        ImGui::EndDragDropTarget();
    }
}

void EditorGUI::rightPanel() {
    ImGui::SetNextWindowBgAlpha(0.8);
    ImGui::BeginChild("Right Panel", ImVec2(0, 0), false, ImGuiWindowFlags_NoScrollbar);

    if (ImGui::CollapsingHeader("Selected Entity")) {
        if (m_selectedEntity.isValid()) {
            // draw the entity panel
            drawEntityInfo();

            // add component menu
            if (ImGui::Button("Add Component")) ImGui::OpenPopup("Add Component");
            if (ImGui::BeginPopup("Add Component")) {
                if (ImGui::MenuItem("Name", nullptr, false, !m_selectedEntity.hasComponent<Component::Name>())) {
                    m_selectedEntity.addComponent<Component::Name>().str = "Entity";
                }
                if (ImGui::MenuItem("Transform", nullptr, false, !m_selectedEntity.hasComponent<Component::Transform>())) {
                    m_selectedEntity.addComponent<Component::Transform>();
                    updateEntityTransform();
                }
                /*if (ImGui::MenuItem("Renderable", nullptr, false, !m_selectedEntity.hasComponent<Component::RenderableID>())) {
                    m_selectedEntity.addComponent<Component::RenderableID>(m_pScene).id = Scene::ID_NULL;
                }*/
                if (ImGui::MenuItem("Renderable", nullptr, false, !m_selectedEntity.hasComponent<Component::Renderable>())) {
                    m_selectedEntity.addComponent<Component::Renderable>();
                    updateEntityTransform();
                }
                /*if (ImGui::MenuItem("Point Light", nullptr, false, !m_selectedEntity.hasComponent<Component::PointLightID>())) {
                    m_selectedEntity.addComponent<Component::PointLightID>(m_pScene).id = m_pScene->addPointLight(PointLight());
                }*/
                if (ImGui::MenuItem("Point Light", nullptr, false, !m_selectedEntity.hasComponent<PointLight>())) {
                    m_selectedEntity.addComponent<PointLight>();
                }
                if (ImGui::MenuItem("Animation State", nullptr, false, !m_selectedEntity.hasComponent<Component::AnimationStateID>())) {
                    m_selectedEntity.addComponent<Component::AnimationStateID>(m_pAnimation).id = UINT_MAX;
                }
                ImGui::EndPopup();
            }
        } else {
            ImGui::Text("None");
        }
    }
    if (ImGui::CollapsingHeader("Model")) {
        Model* pModel = m_pSelectedModel;
        if (pModel) {
            ImGui::PushID(pModel);
            {
                std::string name = pModel->getMesh()->getName();
                std::vector<char> buf(name.begin(), name.end());
                buf.resize(128);
                buf.back() = '\0';
                if (ImGui::InputText("Name", buf.data(), buf.size()-1)) {
                    name = std::string(buf.data());
                    pModel->getMesh()->setName(name);
                }
            }
            if (auto it = m_modelMeshBuilders.find(pModel); it != m_modelMeshBuilders.end()) {
                if (ImGui::TreeNode("MeshBuilder")) {
                    MeshBuilder& mb = it->second;

                    if (drawMeshBuilderNodes(m_modelMeshBuilderNodes[pModel])) {
                        mb.clear();
                        mb.executeNodes(m_modelMeshBuilderNodes[pModel]);
                        m_pWindow->acquireContext();
                        pModel->getMesh()->createFromMeshData(mb.getExternMeshData());
                        pModel->setBoundingSphere(mb.getMeshData().computeBoundingSphere());
                        if (m_selectedEntity.isValid())
                            m_selectedEntity.updateComponent<Component::Renderable>().pModel = pModel;
                        m_pWindow->releaseContext();
                    }

                    ImGui::TreePop();
                }
            }
            if (ImGui::TreeNode("Material")) {
                Material* pMaterial = pModel->getMaterial();

                if (ImGui::Button("Select")) ImGui::OpenPopup("Select Material");

                if (!pMaterial) {
                    if (ImGui::Button("New")) pModel->setMaterial(m_pResManager->pMaterials.emplace_back(std::make_unique<Material>()).get());
                } else {
                    drawMaterialInfo(pMaterial, m_pResManager);
                }

                if (ImGui::BeginPopup("Select Material")) {
                    for (auto& pMat : m_pResManager->pMaterials) {
                        std::string name = pMat->getName();
                        if (name.empty()) name = "Untitled material";
                        if (ImGui::MenuItem(name.c_str(), nullptr, pMat.get() == pMaterial))
                            pModel->setMaterial(pMat.get());
                    }
                    if (ImGui::MenuItem("None", nullptr, !pMaterial)) pModel->setMaterial(nullptr);
                    ImGui::EndPopup();
                }
                ImGui::TreePop();
            }
            ImGui::PopID();
        }
    }

    ImGui::EndChild();  // Right Panel
}

void EditorGUI::onSelectEntity() {
    m_pSelectedAssetNode = nullptr;
/*    if (Renderable* pR; m_selectedEntity.hasComponent<Component::RenderableID>() &&
            (pR = m_pScene->getRenderable(m_selectedEntity.getComponent<Component::RenderableID>().id))) {
        m_pSelectedModel = const_cast<Model*>(pR->getModel());
    } else {
        m_pSelectedModel = nullptr;
    }*/
    if (m_selectedEntity.hasComponent<Component::Renderable>()) {
        m_pSelectedModel = m_selectedEntity.getComponent<Component::Renderable>().pModel;
    } else {
        m_pSelectedModel = nullptr;
    }

    updateEntityTransform();
}

void EditorGUI::onDeselectEntity() {
    m_pSelectedModel = nullptr;
    if (m_pScene->getRenderable(m_outlineRenderableID)) {
        m_pScene->removeRenderable(m_outlineRenderableID);
        m_outlineRenderableID = Scene::ID_NULL;
    }
}
