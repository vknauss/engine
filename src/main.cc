#include <iostream>
#include <exception>

#include <algorithm>

#include <glm/glm.hpp>

#include <glm/gtc/noise.hpp>




#include "app.h"
#include "job_scheduler.h"
#include "material.h"
#include "mesh.h"
#include "mesh_data.h"
#include "mesh_builder.h"
#include "model.h"
#include "renderable.h"
#include "renderer.h"
#include "resource_load.h"
#include "scene.h"


struct FrameUpdateParam {
    double c_gameTime;
    double interval;

    bool pause = false;

    Scene* pScene;

    Animation* pAnimation;
    Skeleton* pSkeleton;
};

void FrameUpdateJob(uintptr_t param) {
    FrameUpdateParam* pParam = reinterpret_cast<FrameUpdateParam*>(param);

    glm::vec3 facingDir(0, 0, 1);

    if (!pParam->pause) {
        float radius = 12.0f;
        pParam->pScene->getActiveCamera()->setPosition(glm::vec3(radius*glm::cos(0.25f * pParam->c_gameTime), 2.0, radius*glm::sin(0.25f * pParam->c_gameTime)));
        pParam->pScene->getActiveCamera()->setDirection(glm::vec3(1, 0, 1) * -pParam->pScene->getActiveCamera()->getPosition());
        //pParam->pScene->getActiveCamera()->setDirection(glm::vec3(-glm::sin(0.25f * gameStateTime), 0, glm::cos(0.25f * gameStateTime)));

        if (pParam->pAnimation && pParam->pSkeleton) {
            pParam->pSkeleton->clearPose();
            float walkCycleSpeed = 1.25f / (35.0f / 24.0f);
            glm::vec3 walkCycleForward = {0, 0, 1};
            float speed = 1.0f;
            float radius = 4.0f;
            float omega = speed * walkCycleSpeed/radius;
            pParam->pAnimation->addPoseToSkeleton(speed * pParam->c_gameTime, pParam->pSkeleton);
            float height = pParam->pSkeleton->getJoint(0)->getLocalPoseOffset().y;
            pParam->pSkeleton->getJoint(0)->setLocalPoseOffset(glm::vec3(radius * glm::sin(omega * pParam->c_gameTime), height, radius * glm::cos(omega * pParam->c_gameTime)));
            facingDir = glm::vec3(glm::cos(omega * pParam->c_gameTime), 0, -glm::sin(omega * pParam->c_gameTime));
            pParam->pSkeleton->getJoint(0)->setLocalPoseRotation(glm::rotation(walkCycleForward, facingDir));
            pParam->pSkeleton->applyCurrentPose();
            pParam->pSkeleton->computeSkinningMatrices();
        }

        //pRenderParam->pScene->getDirectionalLight().setDirection(glm::vec3(glm::cos(2*gameStateTime), -2, glm::sin(2*gameStateTime)));

    }

    //if (glfwGetKey(pParam->pApp->getWindow()->getHandle(), GLFW_KEY_F) == GLFW_PRESS) {
        if (pParam->pSkeleton) {

            //pParam->pScene->getActiveCamera()->setPosition(pParam->pSkeleton->getJoint("Head")->getWorldPosition() + facingDir * 1.25f);
            //pParam->pScene->getActiveCamera()->setDirection(-facingDir);

            glm::vec3 dir = pParam->pSkeleton->getJoint("Head")->getWorldPosition() - pParam->pScene->getActiveCamera()->getPosition();
            pParam->pScene->getActiveCamera()->setDirection(dir);

        }
    //}


}

struct WindowResizeListener {
    Renderer* pRenderer;
    Scene* pScene;
    Window* pWindow;

    void onFramebufferResize(int width, int height) {
        pWindow->acquireContext();
        pRenderer->setViewport(static_cast<uint32_t>(width), static_cast<uint32_t>(height));
        pWindow->releaseContext();
        pScene->getActiveCamera()->setFOV((float) width / (float) height);
    }
};

int main() {

    App* pApp = new App();

    try {
        App::InitParameters param;
        param.windowInititalHeight = 800;
        param.windowInititialWidth = 1280;
        pApp->init(param);
    } catch (std::exception &e) {
        std::cerr << "Failed to initialize." << std::endl;
        std::cerr << e.what() << std::endl;

        pApp->cleanup();
        delete pApp;

        return EXIT_FAILURE;
    }

    JobScheduler* pScheduler = new JobScheduler();
    pScheduler->spawnThreads();

    // Startup Renderer
    pApp->getWindow()->acquireContext();
    //pApp->getWindow()->setVSyncInterval(1);
    pApp->getWindow()->setFullscreen(true);
    //pApp->getWindow()->setCursorCaptured(true);
    Renderer* pRenderer = new Renderer(pScheduler);
    {
        RendererParameters param = pRenderer->getParameters();
        param.exposure = 0.8f;
        param.shadowMapLightBleedCorrectionBias = 0.0f;
        param.enableMSAA = false;
        param.initialWidth = pApp->getWindow()->getWidth();
        param.initialHeight = pApp->getWindow()->getHeight();


        RendererParameters minimal = param;

        minimal.enableShadowFiltering = false;
        minimal.enableSSAO = false;
        minimal.enableBloom = false;
        minimal.enablePointLights = false;
        minimal.enableShadows = false;

        RendererParameters low = param;
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

        RendererParameters medium = param;
        medium.enableShadowFiltering = true;
        medium.enableSSAO = true;
        medium.enableBloom = true;
        medium.enablePointLights = true;
        medium.enableShadows = true;
        medium.maxPointLightShadowMaps = 5;
        medium.pointLightShadowMapResolution = 512;
        medium.shadowMapNumCascades = 4;
        medium.shadowMapCascadeScale = 0.3f;
        medium.shadowMapResolution = 1024;
        medium.shadowMapFilterResolution = 512;
        medium.shadowMapMaxDistance = 40.0f;
        medium.numBloomLevels = 5;

        RendererParameters high = param;
        high.enableShadowFiltering = true;
        high.enableSSAO = true;
        high.enableBloom = true;
        high.enablePointLights = true;
        high.enableShadows = true;
        high.maxPointLightShadowMaps = 12;
        high.pointLightShadowMapResolution = 1024;
        high.shadowMapNumCascades = 5;
        high.shadowMapCascadeScale = 0.4f;
        high.shadowMapResolution = 2048;
        high.shadowMapFilterResolution = 1024;
        high.shadowMapMaxDistance = 40.0f;
        high.numBloomLevels = 5;


        std::cout << "Detecting optimal render settings..." << std::endl;

        std::array<RendererParameters, 4> testParameters = {high, medium, low, minimal};

        // Create dummy scene for test rendering
        Scene dummy;

        dummy.setActiveCamera(dummy.addCamera(Camera(M_PI/4.0, (float) param.initialWidth / (float) param.initialHeight, 0.1f, 100.0f)));
        dummy.getActiveCamera()->setPosition(dummy.getActiveCamera()->getPosition() + glm::vec3(0, 1, 0));

        dummy.addPointLight(PointLight().setIntensity({1, 1, 1}).setPosition({-2, 1, 0}).setShadowMapEnabled(true));
        dummy.addPointLight(PointLight().setIntensity({3, 3, 3}).setPosition({4, 1, 5}).setShadowMapEnabled(true));

        MeshData meshData = MeshBuilder().sphere(1.0f, 64, 32).translate(dummy.getActiveCamera()->getPosition() + 4.0f * dummy.getActiveCamera()->getDirection()).plane(50).moveMeshData();
        Material material;
        Mesh mesh;
        mesh.setVertexCount(meshData.vertices.size());
        mesh.createVertexBuffer(0, 3, meshData.vertices.data());
        mesh.createVertexBuffer(1, 3, meshData.normals.data());
        mesh.createIndexBuffer(meshData.indices.size(), meshData.indices.data());
        Model model(&mesh, &material, nullptr, meshData.computeBoundingSphere());

        dummy.addRenderable(Renderable().setModel(&model));

        int targetFPS = glfwGetVideoMode(glfwGetPrimaryMonitor())->refreshRate;

        Renderer::RendererJobParam rParam;
        //rParam.pRenderer = pRenderer;
        rParam.pScene = &dummy;
        rParam.pScheduler = pScheduler;
        rParam.pWindow = pApp->getWindow();
        rParam.signalCounterHandle = pScheduler->getCounterByID("pr");

        JobScheduler::JobDeclaration preRenderDecl;
        preRenderDecl.numSignalCounters = 1;
        preRenderDecl.signalCounters[0] = pScheduler->getCounterByID("pr");
        preRenderDecl.pFunction = Renderer::preRenderJob;
        preRenderDecl.param = reinterpret_cast<uintptr_t>(&rParam);

        JobScheduler::CounterHandle renderCounters[2] = {pScheduler->getFreeCounter(), pScheduler->getFreeCounter()};

        JobScheduler::JobDeclaration renderDecl;
        renderDecl.numSignalCounters = 1;
        renderDecl.waitCounter = pScheduler->getCounterByID("pr");
        renderDecl.pFunction = Renderer::renderJob;
        renderDecl.param = reinterpret_cast<uintptr_t>(&rParam);

        uint32_t preset = 1;
        bool force = true;
        for (; !force && preset < testParameters.size(); ++preset) {
            std::cout << "Trying settings preset " << preset << std::endl;

            Renderer testRenderer(pScheduler);
            testRenderer.init(testParameters[preset]);
            pApp->getWindow()->releaseContext();

            rParam.pRenderer = &testRenderer;

            double beginTime = glfwGetTime();
            uint32_t frames = 0;

            int fps = 0;

            bool accept = false;

            preRenderDecl.waitCounter = JobScheduler::COUNTER_NULL;
            while (true) {
                glfwPollEvents();
                double time = glfwGetTime();
                if (time - beginTime >= 1.0) {
                    fps = static_cast<int> (frames / (time - beginTime) + 0.5);
                    if (fps >= targetFPS) {
                        accept = true;
                    }
                    break;
                }

                pScheduler->enqueueJob(preRenderDecl);
                renderDecl.signalCounters[0] = renderCounters[frames%2];
                pScheduler->enqueueJob(renderDecl);
                pScheduler->waitForCounter(renderCounters[frames%2]);


                ++frames;
            }

            pScheduler->waitForCounter(renderCounters[0]);
            pScheduler->waitForCounter(renderCounters[1]);


            pApp->getWindow()->acquireContext();
            testRenderer.cleanup();

            std::cout << "Estimated framerate: " << fps << " FPS." << std::endl;

            if (preset+1 == testParameters.size() && !accept) {
                std::cout << "Optimal settings could not be found." << std::endl;
            } else if (accept) {
                std::cout << "Settings deemed optimal for " << targetFPS << " FPS." << std::endl;
            } else {
                continue;
            }
            int syncInterval = std::max(1, targetFPS / (fps));
            //pApp->getWindow()->setVSyncInterval(syncInterval);
            //std::cout << "Setting VSync interval " << syncInterval << " (" << (targetFPS / syncInterval) << " FPS)" << std::endl;

            break;
        }



        pRenderer->init(testParameters[preset]);

        //pRenderer->setViewport(pApp->getWindow()->getWidth(), pApp->getWindow()->getHeight());
        pApp->getWindow()->releaseContext();
    }

    // Load meshes
    pApp->getWindow()->acquireContext();
    std::vector<Mesh> meshes;
    std::vector<Material> materials;
    std::vector<Model> models;
    std::vector<Texture*> pTextures;
    std::vector<SkeletonDescription> skeletonDescriptions;
    std::vector<Skeleton> skeletons;
    std::vector<Animation> animations;
    {
        std::vector<MeshData> meshDatas;
        std::vector<int> meshMaterialInds;
        std::vector<int> skeletonDescIndices;
        //importAssimp("models/alienhead.glb", meshDatas, materials, meshMaterialInds, pTextures);
        //importAssimp("models/coolguy/coolguy.gltf", meshDatas, materials, meshMaterialInds, pTextures, skeletonDescriptions, skeletonDescIndices, animations);
        importAssimp("models/mocap_test.glb", meshDatas, materials, meshMaterialInds, pTextures, skeletonDescriptions, skeletonDescIndices, animations);

        meshDatas.push_back(MeshBuilder()
                            .plane(50.0f).uvMapPlanar()
                            //.translate({0, -5, 0})
                            .moveMeshData());
        meshDatas.push_back(MeshBuilder().cylinder(0.5f, 5.0f, 32, 1, true)
                            .translate({0, 2.5f, 0})
                            .moveMeshData());
        meshDatas.push_back(MeshBuilder().sphere(0.05f, 32, 16)
                            .translate({2, 1, -0.25})
                            .moveMeshData());
        meshDatas.push_back(MeshBuilder().sphere(0.05f, 32, 16)
                            .translate({-4, 3, 2})
                            .moveMeshData());

        pTextures.push_back(new Texture());
        load_texture("textures/checkerboard.png", pTextures.back());
        materials.push_back(Material().setTintColor({0.6, 0.8, 0.7}).setDiffuseTexture(pTextures.back())); // Default material in case no material was set for a mesh
        materials.push_back(Material().setTintColor({0.6, 0.5, 0.65}).setMetallic(1.0).setRoughness(0.1));
        materials.push_back(Material().setEmissionIntensity({4, 8, 2}).setShadowCastingEnabled(false));
        materials.push_back(Material().setEmissionIntensity({12, 2, 4}).setShadowCastingEnabled(false));

        meshMaterialInds.push_back(static_cast<int>(materials.size()-4));
        meshMaterialInds.push_back(static_cast<int>(materials.size()-3));
        meshMaterialInds.push_back(static_cast<int>(materials.size()-2));
        meshMaterialInds.push_back(static_cast<int>(materials.size()-1));
        skeletonDescIndices.resize(meshDatas.size(), -1);
        meshes.resize(meshDatas.size());
        models.resize(meshDatas.size());
        for (size_t i = 0; i < meshes.size(); ++i) {
            meshes[i].setVertexCount(meshDatas[i].vertices.size());
            meshes[i].createVertexBuffer(0, 3, static_cast<void*>(meshDatas[i].vertices.data()));
            meshes[i].createVertexBuffer(1, 3, static_cast<void*>(meshDatas[i].normals.data()));
            if (meshDatas[i].hasUVs()) meshes[i].createVertexBuffer(2, 2, static_cast<void*>(meshDatas[i].uvs.data()));
            if (meshDatas[i].hasBoneIndices() && meshDatas[i].hasBoneWeights()) {
                    meshes[i].createVertexBuffer(3, 4, static_cast<void*>(meshDatas[i].boneIndices.data()), false, true);
                    meshes[i].createVertexBuffer(4, 4, static_cast<void*>(meshDatas[i].boneWeights.data()));
            }
            if (meshDatas[i].hasTangents() && meshDatas[i].hasBitangents()) {
                    meshes[i].createVertexBuffer(5, 3, static_cast<void*>(meshDatas[i].tangents.data()));
                    meshes[i].createVertexBuffer(6, 3, static_cast<void*>(meshDatas[i].bitangents.data()));
            }
            meshes[i].createIndexBuffer(meshDatas[i].indices.size(), static_cast<void*>(meshDatas[i].indices.data()));

            models[i].setMesh(&meshes[i]);
            models[i].setMaterial(&materials[meshMaterialInds[i]]);
            models[i].setBoundingSphere(meshDatas[i].computeBoundingSphere());
            if (skeletonDescIndices[i] >= 0) {
                models[i].setSkeletonDescription(&skeletonDescriptions[skeletonDescIndices[i]]);
                models[i].setJointBoundingSpheres(meshDatas[i].computeJointBoundingSpheres(models[i].getSkeletonDescription()->getNumJoints()));
                //skeletons.emplace_back(&skeletonDescriptions[skeletonDescIndices[i]]);
            }
        }
    }

    for (const SkeletonDescription& sd : skeletonDescriptions) skeletons.emplace_back(&sd);

    pApp->getWindow()->releaseContext();

    // Create mesh instances
    Scene* pScene = new Scene();
    {
        //size_t skeletonIndex = 0;
        for (size_t i = 0; i < models.size(); ++i) {
            Renderable rbl;
            rbl
                .setModel(&models[i])
                .setWorldTransform(glm::mat4(1.0f));
            if (models[i].getSkeletonDescription()) {
                auto skelit = std::find_if(skeletons.begin(), skeletons.end(), [&] (const Skeleton& s) { return models[i].getSkeletonDescription() == s.getDescription(); });
                if (skelit != skeletons.end()) {
                    rbl.setSkeleton(&(*skelit));
                }
            }
            pScene->addRenderable(rbl);
        }

        pScene->setActiveCamera(pScene->addCamera(Camera(M_PI/3.0f, (float) pApp->getWindow()->getWidth()/pApp->getWindow()->getHeight(), 0.1f, 100.0f)));
        pScene->getActiveCamera()->setPosition({0, 0, 2});

        pScene->getDirectionalLight().setDirection({0.5, -2, -1}).setIntensity({2, 2, 2}).setShadowMapEnabled(true);

        pScene->addPointLight(PointLight().setIntensity({4, 16, 2}).setPosition({2, 1, -0.25}).setShadowMapEnabled(true));
        pScene->addPointLight(PointLight().setIntensity({12, 2, 4}).setPosition({-4, 3, 2}).setShadowMapEnabled(true));

        pScene->setAmbientLightIntensity({0.1, 0.1, 0.1});
    }

    JobScheduler::CounterHandle renderCounters[2] = {
        pScheduler->getCounterByID("r0"), pScheduler->getCounterByID("r1")
    };

    // Parameter to be sent to the renderer jobs
    Renderer::RendererJobParam rParam = {};
    rParam.pRenderer = pRenderer;
    rParam.pScene = pScene;
    rParam.pWindow = pApp->getWindow();
    rParam.pScheduler = pScheduler;
    rParam.signalCounterHandle = pScheduler->getCounterByID("pr");

    // Frame update job
    FrameUpdateParam uParam = {};
    uParam.pScene = pScene;
    uParam.c_gameTime = 0.0;

    if (!skeletons.empty()) {
        uParam.pSkeleton = &skeletons.back();
    } else {
        uParam.pSkeleton = nullptr;
    }
    if (!animations.empty()) {
        uParam.pAnimation = &animations.back();
    } else {
        uParam.pAnimation = nullptr;
    }

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
    //preRenderDecl.waitCounter = pParam->renderCounters[(pParam->frame+1)%2];

    renderDecl.param = reinterpret_cast<uintptr_t>(&rParam);
    renderDecl.pFunction = Renderer::renderJob;
    renderDecl.signalCounters[0] = renderCounters[0]; //pParam->renderCounter[pParam->frame];
    renderDecl.numSignalCounters = 1;
    renderDecl.waitCounter = pScheduler->getCounterByID("pr");


    int frame = 0;
    bool pause = false;
    double lastFrameTime = glfwGetTime();
    double lastFPSTime = lastFrameTime;
    uint32_t framesSinceLastFPS = 0;
    while (pApp->isRunning()) {
        bool ppause = (glfwGetKey(pApp->getWindow()->getHandle(), GLFW_KEY_P) == GLFW_RELEASE);

        pApp->pollEvents();


        if (ppause && ((glfwGetKey(pApp->getWindow()->getHandle(), GLFW_KEY_P) == GLFW_PRESS))) {
            pause = !pause;
            uParam.pause = pause;
        }

        if (glfwGetKey(pApp->getWindow()->getHandle(), GLFW_KEY_ESCAPE) == GLFW_PRESS) {
            pApp->requestQuit();
            break;
        }

        double time = glfwGetTime();
        double dt = time - lastFrameTime;
        lastFrameTime = time;
        if (!pause) {
            uParam.c_gameTime += dt;
            uParam.interval = dt;
        } else {
            uParam.interval = 0.0;
        }


        pScheduler->enqueueJob(updateDecl);
        pScheduler->waitForCounter(pScheduler->getCounterByID("u"));

        if (time - lastFPSTime >= 1.0) {
            std::cout << "FPS: " << int (framesSinceLastFPS / (time - lastFPSTime) + 0.5) << std::endl;
            lastFPSTime = time;
            framesSinceLastFPS = 0;
        }
        ++framesSinceLastFPS;

        if(!pause) {
            pScheduler->enqueueJob(preRenderDecl);
            pScheduler->enqueueJob(renderDecl);
        }

        preRenderDecl.waitCounter = renderCounters[frame];

        frame = (frame+1)%2;

        renderDecl.signalCounters[0] = renderCounters[frame];

        // The next update job will wait until this render job has finished to begin executing
        updateDecl.waitCounter = pScheduler->getCounterByID("pr");
    }

    // Signal the worker threads to stop processing their queues (regardless if there is still work in the queue) and join
    pScheduler->joinThreads();

    // Need the GL context for freeing e.g. Mesh buffers
    pApp->getWindow()->acquireContext();

    std::cout << ("GL Error: " + std::to_string((int)glGetError())) << std::endl;

    // Cleanup

    for (Texture* pTexture : pTextures) if (pTexture) delete pTexture;

    delete pScene;

    pRenderer->cleanup();
    delete pRenderer;

    delete pScheduler;

    pApp->cleanup();
    delete pApp;

    return EXIT_SUCCESS;
}
