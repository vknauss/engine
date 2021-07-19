#include <iostream>

#include <exception>
#include <algorithm>
#include <memory>
#include <sstream>

#include <glm/glm.hpp>
#include <glm/gtc/noise.hpp>
#include <glm/gtc/random.hpp>
#include <glm/gtx/string_cast.hpp>

#include "core/app/app.h"

#include "core/animation/animation_system.h"
#include "core/audio/audio.h"
#include "core/ecs/entity.h"
#include "core/job_scheduler.h"
#include "core/physics/physics.h"
#include "core/render/renderer.h"
#include "core/resources/resource_manager.h"
#include "core/resources/resource_load.h"
#include "core/scene/scene.h"
#include "core/util/mesh_builder.h"
#include "core/util/timer.h"

#include "editor/editor_gui.h"


/*
struct CharacterInputState {
    glm::vec2 walkInput = glm::vec2(0.0f);
    glm::vec2 cameraInput = glm::vec2(0.0f);
    float cameraZoom = 0.0f;
    bool gamepadInput = false;
    bool jump = false;
    bool running = false;
    float runInterp = 0.0f;

    glm::vec3 velocity = glm::vec3(0.0f);
    float rotVelocity = 0;
};*/


struct FrameUpdateParam {
    double c_gameTime;
    double interval;

    bool pause = false;

    //uint32_t playerRigidBodyID;

    //CharacterInputState* pInputState;
    //const AnimationStateNode* pStateCache;

    //glm::vec2 cameraAngle = glm::vec2(0.0f);
    //float cameraDistance = 4.0f;

    Scene* pScene;

    PhysicsSystem* pPhysics;

    //AnimationClip* pAnimation;
    //AnimationInstance* pAnimInstance;
    //Skeleton* pSkeleton;
};

/*
struct DynamicObject {
    size_t rigidBodyID;
    size_t renderableID;
};*/

/*
class CharacterBehavior {

public:

    float walkCycleSpeed = 0.7f;
    float runCycleSpeed = 1.92f;
    float runSpeed = 3.0f;
    float walkSpeed = 0.5f;
    float accel = 5.0f;
    float stopAccel = 15.0f;
    float rotSpeed = 15.0f;
    float rotAccel = 5.0f;

    RigidBody* pRigidBody;
    AnimationInstance* pAnimInstance;
    Skeleton* pSkeleton;
    PhysicsSystem* pPhysics;
    const AnimationStateNode* pStateCache;
    CharacterInputState* pInputState;
    Camera* pCamera;

    void update(float dt) {
        glm::quat rootRot = pSkeleton->getJoint(0)->getLocalPoseRotation();
        glm::vec3 playerForward = glm::rotate(rootRot, glm::vec3(0, 0, 1));
        glm::vec3 playerRight = glm::cross(playerForward, glm::vec3(0, 1, 0));

        glm::vec3 cameraDirection = pCamera->getDirection();
        glm::vec3 cameraForward = glm::normalize(glm::vec3(cameraDirection.x, 0, cameraDirection.z));
        glm::vec3 cameraRight = glm::cross(cameraForward, {0, 1, 0});

        // Character jump/ground state

        std::string cStateName = pAnimInstance->getCurrentState()->getName();

        bool onGround = pPhysics->castRay(glm::vec3(pRigidBody->getWorldTransform()[3]) + glm::vec3(0, 0.9, 0), glm::vec3(0, -1, 0));  // 10cm tolerance
        if (pStateCache->getName() == std::string("jumpStart") && cStateName == std::string("jumpInAir")) {
            // jump anticipation animation ended, apply jump impulse
            pRigidBody->applyImpulse(glm::vec3(0, 5, 0) * pRigidBody->getParameters().mass);
        } else if (onGround && (cStateName == std::string("fall") || (cStateName == std::string("jumpInAir") && pRigidBody->getVelocity().y <= 0.01f))) {
            // fallen, important that we check velocity to avoid triggering this right when the jump starts
            pAnimInstance->triggerFlag("landJump");
        } //else if (pAnimInstance->getCurrentState()->getName() == std::string("jumpInAir") && pRigidBody->getVelocity().y <= 0.01f) {
        else if (!onGround && pRigidBody->getVelocity().y <= 0.01f) {
            pAnimInstance->triggerFlag("startFalling");
        }
        if (pInputState->jump) {
            pAnimInstance->triggerFlag("jump");
        }

        if (true) {
            glm::vec3 velocity = pRigidBody->getVelocity();
            glm::vec3 horizontalV = glm::vec3(velocity.x, 0, velocity.z);

            bool motionLocked = false;
            if (cStateName == std::string("jumpLand") || cStateName == std::string("jumpStart")) motionLocked = true;

            float targetSpeed = motionLocked ? 0.0 : (pInputState->running ? runSpeed : walkSpeed);

            glm::vec3 targetVelocity = targetSpeed * (cameraRight * pInputState->walkInput.x + cameraForward * pInputState->walkInput.y);

            glm::vec3 targetDeltaV = targetVelocity - horizontalV;

            float targetAccel = (glm::dot(targetDeltaV, horizontalV) < 0.0f) ? stopAccel : accel;

            // Move the character rigidbody using impulses
            glm::vec3 impulse(0.0);
            if (glm::length(targetDeltaV) > targetAccel * dt) {
                impulse = glm::normalize(targetDeltaV) * targetAccel * dt * pRigidBody->getParameters().mass;
            } else {
                impulse = targetDeltaV * pRigidBody->getParameters().mass;
            }
            if (glm::length(impulse) > 0) {
                pRigidBody->applyImpulse(impulse);

            }

            //pInputState->velocity = horizontalV;

            float cSpeed = glm::length(horizontalV);

            if (cSpeed > 0.01) {
                // Compute blend factors for walking animation based on angle and speed
                float angle = glm::acos(glm::clamp(glm::dot(horizontalV, playerForward) / cSpeed, -1.0f, 1.0f));
                if (glm::dot(horizontalV, playerRight) > 0) {
                    angle = -angle;
                }

                // Rotation is not computed for the rigid body. For that we use the world transform
                float s = rotAccel * cSpeed / targetSpeed;
                if (glm::abs(s*angle*dt) <= glm::abs(angle))
                    pInputState->rotVelocity = glm::clamp(s*angle, -rotSpeed, rotSpeed);
                else
                    pInputState->rotVelocity = 0.0f;

                if (!pAnimInstance->getFlag("isWalking")) pAnimInstance->triggerFlag("isWalking");

                if (cSpeed <= walkSpeed) {
                    pAnimInstance->setParameter("walkSpeed", cSpeed/walkCycleSpeed);
                    pAnimInstance->setParameter("walkRun", 0.0f);
                } else if (cSpeed >= runSpeed) {
                    pAnimInstance->setParameter("walkSpeed", cSpeed/runCycleSpeed);
                    pAnimInstance->setParameter("walkRun", 1.0f);
                } else {
                    float blend = (cSpeed - walkSpeed) / (runSpeed - walkSpeed);
                    float timescale = cSpeed / (blend * runCycleSpeed + (1.0f-blend) * walkCycleSpeed);
                    pAnimInstance->setParameter("walkSpeed", timescale);
                    pAnimInstance->setParameter("walkRun", blend);
                }

                pAnimInstance->setParameter("turnLR", -pInputState->rotVelocity / rotSpeed);
            } else {
                if (!pAnimInstance->getFlag("isIdle")) pAnimInstance->triggerFlag("isIdle");
                pInputState->rotVelocity = 0.0f;  // so it can't drift
            }
        }

        //pInputState->velocity = pRigidBody->getVelocity();
        pStateCache = pAnimInstance->getCurrentState();

        // Update animation and set skeleton pose
        // This will be moved into an update method to be performed for each animation instance each frame
        pAnimInstance->update(dt);

        pSkeleton->clearPose();
        pSkeleton->setPose(pAnimInstance->getPose());

        // Rotate root joint to match the calculated angular velocity
        // Has to be done after computing the animation pose (of course)
        pSkeleton->getJoint(0)->setLocalPoseRotation(glm::rotate(rootRot, dt * pInputState->rotVelocity, glm::vec3(0, 1, 0)));

        // Finalize animation and build skinning palette to prepare for render
        pSkeleton->applyCurrentPose();
        pSkeleton->computeSkinningMatrices();
    }
};*/

/*
class CameraBehavior {
    float cameraSpeed = 10.0f;
    float cameraHeight = 2.0f;
    float cameraDistance = 4.0f;
    glm::vec2 cameraAngle = glm::vec2(0);

    Camera* pCamera;
    CharacterInputState* pInputState;
    RigidBody* pPlayerBody;

    void update(float dt) {
        cameraDistance = glm::max(cameraDistance + dt * pInputState->cameraZoom * cameraSpeed, 1.0f);

        cameraAngle.x += dt * pInputState->cameraInput.x * cameraSpeed;
        cameraAngle.y += dt * pInputState->cameraInput.y * cameraSpeed;
        cameraAngle.y = glm::clamp(cameraAngle.y, -0.99f * (float) M_PI / 2.0f, 0.99f * (float) M_PI / 2.0f);

        glm::vec3 cameraDirection = glm::normalize(
            glm::rotate(
                glm::angleAxis(cameraAngle.x, glm::vec3(0, 1, 0)) *
                    glm::angleAxis(cameraAngle.y, glm::vec3(1, 0, 0)),
                glm::vec3(0, 0, 1)));

        pCamera->setDirection(cameraDirection);

        // Set camera position to track the player
        pCamera->setPosition(
            glm::vec3(pPlayerBody->getWorldTransform()[3]) +
            glm::vec3(0, cameraHeight, 0) -
            cameraDirection * cameraDistance);
    }
};*/


void FrameUpdateJob(uintptr_t param) {
    FrameUpdateParam* pParam = reinterpret_cast<FrameUpdateParam*>(param);

    pParam->pScene->copyLastTransforms();

    if (!pParam->pause) {

        // Update logic goes here

    }
}

/*
constexpr size_t N_RENDER_PRESETS = 4;
std::array<RendererParameters, N_RENDER_PRESETS> getRenderPresets(const RendererParameters& initial) {
    RendererParameters minimal = initial;

    minimal.enableShadowFiltering = false;
    minimal.enableSSAO = false;
    minimal.enableBloom = false;
    minimal.enablePointLights = false;
    minimal.enableShadows = false;

    RendererParameters low = initial;
    low.enableShadowFiltering = false;
    low.enableSSAO = false;
    low.enableBloom = true;
    low.enablePointLights = true;
    low.enableShadows = true;
    low.maxPointLightShadowMaps = 1;
    low.pointLightShadowMapResolution = 128;
    low.shadowMapNumCascades = 2;
    low.shadowMapCascadeScale = 0.3f;
    low.shadowMapResolution = 512;
    low.shadowMapMaxDistance = 15.0f;
    low.numBloomLevels = 1;

    RendererParameters medium = initial;
    medium.enableShadowFiltering = true;
    medium.enableSSAO = true;
    medium.enableBloom = true;
    medium.enablePointLights = true;
    medium.enableShadows = true;
    medium.maxPointLightShadowMaps = 5;
    medium.pointLightShadowMapResolution = 512;
    medium.shadowMapNumCascades = 4;
    medium.shadowMapCascadeScale = 0.4f;
    medium.shadowMapResolution = 1024;
    medium.shadowMapFilterResolution = 512;
    medium.shadowMapMaxDistance = 40.0f;
    medium.numBloomLevels = 5;

    RendererParameters high = initial;
    high.enableShadowFiltering = true;
    high.enableSSAO = true;
    high.enableBloom = true;
    high.enablePointLights = true;
    high.enableShadows = true;
    high.maxPointLightShadowMaps = 12;
    high.pointLightShadowMapResolution = 1024;
    high.shadowMapNumCascades = 4;
    high.shadowMapCascadeScale = 0.4f;
    high.shadowMapResolution = 2048;
    high.shadowMapFilterResolution = 1024;
    high.shadowMapMaxDistance = 40.0f;
    high.numBloomLevels = 5;

    return std::array<RendererParameters, N_RENDER_PRESETS> {high, medium, low, minimal};
}*/

/*class CharacterInputContext : public InputContext {

public:

    CharacterInputContext(CharacterInputState* pState) :
        m_pState(pState),
        m_gamepadID(-1) {
    }

    bool consumeKeyEvent(int key, int scancode, int action, int mods) override {
        if (m_gamepadID >= 0) return false;

        glm::vec2 delta(0.0f);

        switch (key) {
        case GLFW_KEY_W:
            if (action == GLFW_PRESS) {
                delta.y = 1.0f;
            } else if (action == GLFW_RELEASE) {
                delta.y = -1.0f;
            }
            break;
        case GLFW_KEY_S:
            if (action == GLFW_PRESS) {
                delta.y = -1.0f;
            } else if (action == GLFW_RELEASE) {
                delta.y = 1.0f;
            }
            break;
        case GLFW_KEY_A:
            if (action == GLFW_PRESS) {
                delta.x = -1.0f;
            } else if (action == GLFW_RELEASE) {
                delta.x = 1.0f;
            }
            break;
        case GLFW_KEY_D:
            if (action == GLFW_PRESS) {
                delta.x = 1.0f;
            } else if (action == GLFW_RELEASE) {
                delta.x = -1.0f;
            }
            break;
        case GLFW_KEY_SPACE:
            if (action == GLFW_PRESS) {
                m_pState->jump = true;
                return true;
            }
            break;
        case GLFW_KEY_LEFT_SHIFT:
            if (action == GLFW_PRESS) {
                m_pState->running = true;
            } else if (action == GLFW_RELEASE) {
                m_pState->running = false;
            }
        }

        if (glm::dot(delta, delta) > 0.0f) {
            if (delta.x != 0.0f) {
                if (m_pState->walkInput.y != 0.0f) {
                    m_pState->walkInput.x += delta.x * glm::abs(m_pState->walkInput.y);
                    m_pState->walkInput = glm::normalize(m_pState->walkInput);
                } else {
                    m_pState->walkInput.x += delta.x;
                }
            } else {
                if (m_pState->walkInput.x != 0.0f) {
                    m_pState->walkInput.y += delta.y * glm::abs(m_pState->walkInput.x);
                    m_pState->walkInput = glm::normalize(m_pState->walkInput);
                } else {
                    m_pState->walkInput.y += delta.y;
                }
            }
            return true;
        }
        return false;
    }

    bool consumeCursorMoveEvent(double positionX, double positionY, double deltaX, double deltaY) override {
        if (m_gamepadID >= 0) return false;

        m_pState->cameraInput.x = -0.05f * deltaX;
        m_pState->cameraInput.y = 0.05f * deltaY;

        return true;
    }

    bool consumeMouseScrollEvent(double offsetX, double offsetY) {
        if (m_gamepadID >= 0) return false;

        m_pState->cameraZoom = -offsetY;
        return true;
    }

    bool consumeGamepadStickEvent(int gamepad, int stick, float valueX, float valueY) override {
        if (m_gamepadID < 0) return false;

        glm::vec2 value(valueX, -valueY);

        // Create a "dead zone" near between[(-0.1, -0.1), (0.1, 0.1)] where values are clamped
        //   to 0 to minimize drifting inputs, re-normalize result
        //value = glm::sign(value) * (glm::max(glm::vec2(0.0f), glm::abs(value) - 0.1f) / 0.9f);
        if (float len = glm::length(value); len > 0.1) {
            value /= len;
            value *= glm::min((len - 0.1f) / 0.9f, 1.0f);
        } else {
            value = {0.0f, 0.0f};
        }

        // Remap values from a circle to a unit square
        //float inputAngle = glm::atan(value.y / value.x);
        //float inputScaleLength = glm::abs((glm::abs(value.x) > glm::abs(value.y)) ? glm::cos(inputAngle) : glm::sin(inputAngle));
        //if (inputScaleLength > 0.0f) value /= inputScaleLength;

        switch (stick) {
        case GAMEPAD_STICK_LEFT:
            if (glm::length(value) > 0.5f) {
                m_pState->running = true;
                m_pState->walkInput = value;
            } else {
                m_pState->running = false;
                m_pState->walkInput = 2.0f * value;
            }
            return true;

        case GAMEPAD_STICK_RIGHT:
            m_pState->cameraInput = -value;
            return true;

        default:
            return false;
        }
    }

    bool consumeGamepadAxisEvent(int gamepad, int axis, float value) {
        if (m_gamepadID < 0) return false;

        if (axis == GLFW_GAMEPAD_AXIS_LEFT_TRIGGER) {
            m_pState->cameraZoom = 0.5f * (1.0f + value);
            return true;
        } else if (axis == GLFW_GAMEPAD_AXIS_RIGHT_TRIGGER) {
            m_pState->cameraZoom = -0.5f * (1.0f + value);
            return true;
        }

        return false;
    }

    bool consumeGamepadButtonEvent(int gamepad, int button, int action) override {
        if (m_gamepadID < 0) {
            if (action == GLFW_PRESS) {
                m_gamepadID = gamepad;
                m_pState->gamepadInput = true;
                return true;
            }
        }

        switch (button) {
        case GLFW_GAMEPAD_BUTTON_A:
            if (action == GLFW_PRESS) {
                m_pState->jump = true;
                return true;
            }
            break;
        }

        return false;
    }

    bool consumeGamepadConnectEvent(int gamepad) override {
        if (m_gamepadID < 0) {
            m_gamepadID = gamepad;
            m_pState->gamepadInput = true;
            return true;
        }
        return false;
    }

    bool consumeGamepadDisconnectEvent(int gamepad) override {
        if (gamepad == m_gamepadID) {
            m_gamepadID = -1;
            m_pState->gamepadInput = false;
            return true;
        }
        return false;
    }

private:

    CharacterInputState* m_pState;

    int m_gamepadID;
};*/

int main() {

    std::cout << "start" << std::endl;

    auto pApp = std::make_unique<App>();

    try {
        App::InitParameters param;
        param.windowInititalHeight = 800;
        param.windowInititialWidth = 1280;
        param.windowIsResizable = true;
        pApp->init(param);
        pApp->getWindow()->setVSyncInterval(0);
    } catch (std::exception &e) {
        std::cerr << "Failed to initialize." << std::endl;
        std::cerr << e.what() << std::endl;

        pApp->cleanup();

        return EXIT_FAILURE;
    }

    {
        std::string gamepadMappingsFile = "data/gamecontrollerdb.txt";
        if (!glfwUpdateGamepadMappings(file_as_string(gamepadMappingsFile).c_str())) {
            std::cerr << "Failed to update gamepad mappings from file: '" << gamepadMappingsFile << "'" << std::endl;
        } else {
            std::cout << "Loaded gamepad mappings." << std::endl;
        }
    }

    //CharacterInputState inputState;
    //pApp->getInputManager()->addInputContext(new CharacterInputContext(&inputState));

    auto pScheduler = std::make_unique<JobScheduler>();
    pScheduler->spawnThreads();

    pApp->getWindow()->acquireContext();
    //pApp->getWindow()->setVSyncInterval(1);

    auto pRenderer = std::make_unique<Renderer>(pScheduler.get());
    pRenderer->init(pApp->getWindow()->getWidth(), pApp->getWindow()->getHeight());
    pRenderer->setRenderToTexture(true);

    pApp->getWindow()->releaseContext();

    // Init scene

    auto pResManager = std::make_unique<ResourceManager>();
    auto pScene = std::make_unique<Scene>();

    // Physics
    auto pPhysics = std::make_unique<PhysicsSystem>();
    pPhysics->init();

    // Game World
    auto pGameWorld = std::make_unique<GameWorld>();
//    pGameWorld->setPhysics(pPhysics);
//    pGameWorld->setScene(pScene);

    // Animation
    auto pAnimation = std::make_unique<AnimationSystem>();
    pAnimation->setResourceManager(pResManager.get());

    /*pApp->getWindow()->setFramebufferSizeCallback([&] (int width, int height) {
        pApp->getWindow()->acquireContext();
        pRenderer->setViewport(static_cast<uint32_t>(width), static_cast<uint32_t>(height));
        pApp->getWindow()->releaseContext();
        pScene->getActiveCamera()->setAspectRatio(static_cast<float>(width) / static_cast<float>(height));
    });*/

    auto pEditorGUI = std::make_unique<EditorGUI>();
    pEditorGUI->setRenderer(pRenderer.get());
    pEditorGUI->setScene(pScene.get());
    pEditorGUI->setWindow(pApp->getWindow());
    pEditorGUI->setResourceManager(pResManager.get());
    pEditorGUI->setGameWorld(pGameWorld.get());
    pEditorGUI->setAnimationSystem(pAnimation.get());



    // Audio
    Audio::init();
    //Audio::SoundID loopSoundID = Audio::addSound("audio/soundtrack.ogg", true, true);
    //Audio::SoundID plinkSoundID = Audio::addSound("audio/plink.ogg", false, false);
    //Audio::SoundID annoyingSound = Audio::addSound("audio/sound1.ogg", true, false);
    //Audio::playSound(loopSoundID, 0.5f);
    //Audio::playSound(annoyingSound, 0.15f, glm::vec3(2, 1, -0.25), 0.1f, 50.0f);

    //Audio::setMasterVolume(0.01f);

    //pPhysics->addCollisionCallback(0, 0, [&] (const glm::vec3& contactPoint) { Audio::playSound(plinkSoundID, 0.85f, contactPoint, 0.5f, 15.0f); });



    Entity test = pGameWorld->createEntity();
    Component::Renderable& rc = test.addComponent<Component::Renderable>();

    auto& pMD = pResManager->pMeshData.emplace_back(std::make_unique<MeshData>(MeshBuilder().cube(1.0f).moveMeshData()));

    auto& pMesh = pResManager->pMeshes.emplace_back(std::make_unique<Mesh>());
    pApp->getWindow()->acquireContext();
    pMesh->createFromMeshData(pMD.get());
    pApp->getWindow()->releaseContext();

    auto& pModel = pResManager->pModels.emplace_back(std::make_unique<Model>());
    pModel->setMesh(pMesh.get());
    pModel->setBoundingSphere(pMD->computeBoundingSphere());

    rc.pModel = pModel.get();

    JobScheduler::CounterHandle renderCounters[2] = {
        pScheduler->getCounterByID("r0"), pScheduler->getCounterByID("r1")
    };

    // Parameter to be sent to the renderer jobs
    Renderer::RendererJobParam rParam = {};
    rParam.pRenderer = pRenderer.get();
    //rParam.pScene = pScene;
    rParam.pGameWorld = pGameWorld.get();
    rParam.pWindow = pApp->getWindow();
    rParam.pScheduler = pScheduler.get();
    rParam.signalCounterHandle = pScheduler->getCounterByID("pr");

    // Frame update job
    FrameUpdateParam uParam = {};
    uParam.pScene = pScene.get();
    uParam.c_gameTime = 0.0;

    // Create job declarations for game state update and render jobs
    JobScheduler::JobDeclaration
        updateDecl,
        preRenderDecl,
        renderDecl;

    updateDecl.param = reinterpret_cast<uintptr_t>(&uParam);
    updateDecl.pFunction = FrameUpdateJob;
    updateDecl.signalCounters[0] = pScheduler->getCounterByID("u");
    updateDecl.numSignalCounters = 1;

    preRenderDecl.param = reinterpret_cast<uintptr_t>(&rParam);
    preRenderDecl.pFunction = Renderer::preRenderJob;
    preRenderDecl.signalCounters[0] = pScheduler->getCounterByID("pr"); //pParam->renderCounter[pParam->frame];
    preRenderDecl.numSignalCounters = 1;
    preRenderDecl.waitCounter = pScheduler->getCounterByID("clear_buffer");

    renderDecl.param = reinterpret_cast<uintptr_t>(&rParam);
    renderDecl.pFunction = Renderer::renderJob;
    renderDecl.signalCounters[0] = renderCounters[0]; //pParam->renderCounter[pParam->frame];
    renderDecl.numSignalCounters = 1;
    renderDecl.waitCounter = pScheduler->getCounterByID("pr");

    JobScheduler::JobDeclaration clearBufferDecl;
    clearBufferDecl.numSignalCounters = 2;
    clearBufferDecl.signalCounters[0] = pScheduler->getCounterByID("clear_buffer"); //pScheduler->getFreeCounter();
    clearBufferDecl.signalCounters[1] = renderCounters[0];
    clearBufferDecl.param = reinterpret_cast<uintptr_t>(pApp->getWindow());
    clearBufferDecl.pFunction = [] (uintptr_t param) {
        AppWindow* pWindow = reinterpret_cast<AppWindow*>(param);
        pWindow->acquireContext();
        glClear(GL_COLOR_BUFFER_BIT);
        pWindow->releaseContext();
    };

    JobScheduler::JobDeclaration swapBuffersDecl;
    swapBuffersDecl.numSignalCounters = 1;
    swapBuffersDecl.signalCounters[0] = pScheduler->getCounterByID("swap_buffers"); //pScheduler->getFreeCounter();
    swapBuffersDecl.waitCounter = renderCounters[0];
    swapBuffersDecl.param = reinterpret_cast<uintptr_t>(pApp->getWindow());
    swapBuffersDecl.pFunction = [] (uintptr_t param) {
        AppWindow* pWindow = reinterpret_cast<AppWindow*>(param);
        pWindow->acquireContext();
        pWindow->swapBuffers();
        pWindow->releaseContext();
    };

    JobScheduler::JobDeclaration editorGuiBeginDecl;
    editorGuiBeginDecl.numSignalCounters = 1;
    editorGuiBeginDecl.signalCounters[0] = pScheduler->getCounterByID("begin_gui"); //pScheduler->getFreeCounter();
    editorGuiBeginDecl.waitCounter = swapBuffersDecl.signalCounters[0];
    editorGuiBeginDecl.param = reinterpret_cast<uintptr_t>(pEditorGUI.get());
    editorGuiBeginDecl.pFunction = [] (uintptr_t param) {
        EditorGUI* pEditorGUI = reinterpret_cast<EditorGUI*>(param);
        pEditorGUI->beginFrame();
    };

    JobScheduler::JobDeclaration editorGuiUpdateDecl;
    editorGuiUpdateDecl.numSignalCounters = 2;
    editorGuiUpdateDecl.signalCounters[0] = pScheduler->getCounterByID("update_gui"); //pScheduler->getFreeCounter();
    editorGuiUpdateDecl.signalCounters[1] = pScheduler->getCounterByID("render_gui"); //pScheduler->getFreeCounter();
    editorGuiUpdateDecl.waitCounter = editorGuiBeginDecl.signalCounters[0];
    editorGuiUpdateDecl.param = reinterpret_cast<uintptr_t>(pEditorGUI.get());
    editorGuiUpdateDecl.pFunction = [] (uintptr_t param) {
        EditorGUI* pEditorGUI = reinterpret_cast<EditorGUI*>(param);
        pEditorGUI->update();
    };

    JobScheduler::JobDeclaration editorGuiRenderDecl;
    editorGuiRenderDecl.numSignalCounters = 1;
    editorGuiRenderDecl.signalCounters[0] = renderCounters[0];
    editorGuiRenderDecl.waitCounter = editorGuiUpdateDecl.signalCounters[1]; //pScheduler->getFreeCounter();
    editorGuiRenderDecl.param = reinterpret_cast<uintptr_t>(pEditorGUI.get());
    editorGuiRenderDecl.pFunction = [] (uintptr_t param) {
        EditorGUI* pEditorGUI = reinterpret_cast<EditorGUI*>(param);
        pEditorGUI->render();
    };

    updateDecl.waitCounter = editorGuiUpdateDecl.signalCounters[0];
    preRenderDecl.waitCounter = updateDecl.signalCounters[0];
    renderDecl.signalCounters[renderDecl.numSignalCounters++] = editorGuiRenderDecl.waitCounter;
    clearBufferDecl.signalCounters[clearBufferDecl.numSignalCounters++] = editorGuiRenderDecl.waitCounter;


    int frame = 0;
    bool pause = false;
    double lastFrameTime = glfwGetTime();

    double lastFPSTime = lastFrameTime;
    int fpsFrames = 0;

    double longestFrame = 0.0;

    // Main Loop
    while (pApp->isRunning()) {
        // Input handling

        /*
        bool ppause = (glfwGetKey(pApp->getWindow()->getHandle(), GLFW_KEY_P) == GLFW_RELEASE);
        bool pfs = (glfwGetKey(pApp->getWindow()->getHandle(), GLFW_KEY_F) == GLFW_RELEASE);
        */

        pApp->pollEvents();

        pScheduler->enqueueJob(editorGuiBeginDecl);
        pScheduler->enqueueJob(editorGuiUpdateDecl);

        /*
        if (pfs && glfwGetKey(pApp->getWindow()->getHandle(), GLFW_KEY_F)) {
            AppWindow* pWindow = pApp->getWindow();

            pWindow->setFullscreen(!pWindow->isFullscreen());
            //pWindow->setCursorCaptured(!pWindow->isCursorCaptured());
        }

        if (ppause && ((glfwGetKey(pApp->getWindow()->getHandle(), GLFW_KEY_P) == GLFW_PRESS))) {
            pause = !pause;
            uParam.pause = pause;
        }
        */

        if (glfwGetKey(pApp->getWindow()->getHandle(), GLFW_KEY_ESCAPE) == GLFW_PRESS) {
            pApp->requestQuit();
            break;
        }

        // Game timers update
        double time = glfwGetTime();
        double dt = time - lastFrameTime;
        lastFrameTime = time;
        if (!pause) {
            uParam.c_gameTime += dt;
            uParam.interval = dt;
        } else {
            uParam.interval = 0.0;
        }
        if (dt > longestFrame) longestFrame = dt;

        ++fpsFrames;
        if (time - lastFPSTime >= 1.0) {
            std::cout << "FPS: " << fpsFrames
                      << " Longest frame: " << longestFrame
                      << " (" << ((int) (1.0 / longestFrame)) << " FPS)"
                      << std::endl;
            fpsFrames = 0;
            lastFPSTime = time;
            longestFrame = 0.00;
        }

        // Send game state update job to scheduler
        pScheduler->enqueueJob(updateDecl);

        pScheduler->waitForCounter(updateDecl.signalCounters[0]);

        // Animation update
        pAnimation->processStateUpdates(dt);
        pAnimation->applyPosesToSkeletons();

        // TODO: Jobify physics update
        pPhysics->update(dt);
        pGameWorld->postPhysicsUpdate();

        // Late update callbacks go here

        pGameWorld->preRenderUpdate();
        pGameWorld->updateBoundingSpheres();
        //pScene->computeBoundingSpheres();

        rParam.pDirectionalLight = &pScene->getDirectionalLight();
        rParam.ambientLightIntensity = pScene->getAmbientLightIntensity();
        rParam.pCamera = pScene->getActiveCamera();

        // TODO: Jobify audio update
        Audio::setListenerInverseTransform(pScene->getActiveCamera()->calculateViewMatrix());
        Audio::update(false);

        pScheduler->enqueueJob(clearBufferDecl);

        // Render
        if (!pause) {
            // Send render jobs to scheduler
            pScheduler->enqueueJob(preRenderDecl);
            pScheduler->enqueueJob(renderDecl);
        }

        pScheduler->enqueueJob(editorGuiRenderDecl);
        pScheduler->enqueueJob(swapBuffersDecl);


        // Synchronize
        pScheduler->waitForCounter(swapBuffersDecl.signalCounters[0]);

        Timer::incrementFrame();

        //preRenderDecl.waitCounter = renderCounters[frame];

        frame = (frame+1)%2;

        clearBufferDecl.signalCounters[1] = renderCounters[frame];
        renderDecl.signalCounters[0] = renderCounters[frame];
        editorGuiRenderDecl.signalCounters[0] = renderCounters[frame];
        swapBuffersDecl.waitCounter = renderCounters[frame];
    }

    // Signal the worker threads to stop processing their queues (regardless if there is still work in the queue) and join
    pScheduler->joinThreads();

    // Need the GL context for freeing e.g. Mesh buffers
    pApp->getWindow()->acquireContext();

    // Cleanup

    Audio::cleanup();

    pAnimation->cleanup();
    pPhysics->cleanup();
    pRenderer->cleanup();
    pApp->cleanup();

    return EXIT_SUCCESS;
}
