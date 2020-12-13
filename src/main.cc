#include <iostream>
#include <exception>

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


const int FRAMES = 3;

struct FrameUpdateParam {
    App* pApp;

    JobScheduler* pScheduler;

    uintptr_t renderParam;

    int frame = 0;

    double lastFrameTime;
    double lastGameStateTime;

    Animation* pAnimation;
    Skeleton* pSkeleton;

    //JobScheduler::CounterHandle renderCounter[FRAMES], updateCounter[FRAMES];
};

void FrameUpdateJob(uintptr_t param) {
    FrameUpdateParam* pParam = reinterpret_cast<FrameUpdateParam*>(param);


    //std::cout << "update" << std::endl;

    pParam->pApp->pollEvents();

    bool pause = glfwGetKey( pParam->pApp->getWindow()->getHandle(), GLFW_KEY_P) == GLFW_PRESS;

    Renderer::RendererJobParam* pRenderParam = reinterpret_cast<Renderer::RendererJobParam*>(pParam->renderParam);
    double time = glfwGetTime();
    double dt = time - pParam->lastFrameTime;

    if (!pause) {
        double gameStateTime = pParam->lastGameStateTime + dt;

        float radius = 10.0f;
        pRenderParam->pScene->getActiveCamera()->setPosition(glm::vec3(radius*glm::cos(0.25f * gameStateTime), 2.0, radius*glm::sin(0.25f * gameStateTime)));
        pRenderParam->pScene->getActiveCamera()->setDirection(glm::vec3(1, 0, 1) * -pRenderParam->pScene->getActiveCamera()->getPosition());

        if (pParam->pAnimation && pParam->pSkeleton) {
            pParam->pSkeleton->clearPose();
            float speed = 0.75f;
            float radius = 4.0f;
            float omega = speed/radius;
            pParam->pAnimation->addPoseToSkeleton(speed * gameStateTime, pParam->pSkeleton);
            pParam->pSkeleton->getJoint("root")->setLocalPoseOffset(glm::vec3(radius * glm::cos(omega * gameStateTime), radius * glm::sin(omega * gameStateTime), 0));
            pParam->pSkeleton->getJoint("root")->setLocalPoseRotation(glm::rotation(glm::vec3(0, -1, 0), glm::vec3(-glm::sin(omega * gameStateTime), glm::cos(omega * gameStateTime), 0)));
            pParam->pSkeleton->applyCurrentPose();
            pParam->pSkeleton->computeSkinningMatrices();
        }

        //pRenderParam->pScene->getDirectionalLight().setDirection(glm::vec3(glm::cos(2*gameStateTime), -2, glm::sin(2*gameStateTime)));

        pParam->lastGameStateTime = gameStateTime;
    }


    pParam->lastFrameTime = time;


    JobScheduler::JobDeclaration renderDecl, updateDecl;

    renderDecl.param = pParam->renderParam;
    renderDecl.pFunction = Renderer::dispatchRenderJobs;
    renderDecl.signalCounters[0] = pParam->pScheduler->getCounterByID("r"); //pParam->renderCounter[pParam->frame];
    renderDecl.numSignalCounters = 1;
    renderDecl.waitCounter = JobScheduler::COUNTER_NULL;

    if(!pause) pParam->pScheduler->enqueueJob(renderDecl);


    if (pParam->pApp->isRunning()) {
        updateDecl.param = param;
        updateDecl.pFunction = FrameUpdateJob;
        updateDecl.signalCounters[0] = pParam->pScheduler->getCounterByID("u");
        updateDecl.numSignalCounters = 1;
        // The next update job will wait until this render job has finished to begin executing
        updateDecl.waitCounter = pParam->pScheduler->getCounterByID("r"); //pParam->renderCounter[pParam->frame];
        pParam->frame = (pParam->frame+1)%FRAMES;

        pParam->pScheduler->enqueueJob(updateDecl);
    }


    //std::cout << "end update" << std::endl;
}

int main() {

    App* pApp = new App();

    try {
        App::InitParameters param;
        param.windowInititalHeight = 1024;
        param.windowInititialWidth = 1024;
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
    pApp->getWindow()->setVSyncInterval(1);
    Renderer* pRenderer = new Renderer(pScheduler);
    {
        RendererParameters param = pRenderer->getParameters();
        param.enableMSAA = false;
        param.enableShadowFiltering = true;
        param.shadowMapNumCascades = 2;
        param.shadowMapCascadeScale = 0.3f;
        param.shadowMapResolution = 2048;
        param.shadowMapFilterResolution = 1024;
        param.shadowMapMaxDistance = 20.0f;
        param.shadowMapLightBleedCorrectionBias = 0.0f;
        param.exposure = 1.0f;
        param.numBloomLevels = 4;

        pRenderer->setParameters(param);
        pRenderer->init();

        pRenderer->setViewport(pApp->getWindow()->getWidth(), pApp->getWindow()->getHeight());
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
        importAssimp("models/coolguy/coolguy.gltf", meshDatas, materials, meshMaterialInds, pTextures, skeletonDescriptions, skeletonDescIndices, animations);

        meshDatas.push_back(MeshBuilder()
                            .cube(10.0f)
                            .translate({0, -5, 0})
                            .moveMeshData());
        meshDatas.push_back(MeshBuilder().cylinder(0.1f, 5.0f, 32, 1, true)
                            .translate({0, 2.5f, 0})
                            .moveMeshData());
        meshDatas.push_back(MeshBuilder().sphere(0.05f, 32, 16)
                            .translate({2, 1, -0.25})
                            .moveMeshData());
        meshDatas.push_back(MeshBuilder().sphere(0.05f, 32, 16)
                            .translate({-4, 3, 2})
                            .moveMeshData());

        materials.push_back(Material().setTintColor({0.2, 0.8, 0.4})); // Default material in case no material was set for a mesh
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
                skeletons.emplace_back(&skeletonDescriptions[skeletonDescIndices[i]]);
            }

            BoundingSphere b = models[i].getBoundingSphere();
        }
    }
    pApp->getWindow()->releaseContext();

    // Create mesh instances
    Scene* pScene = new Scene();
    {
        size_t skeletonIndex = 0;
        for (size_t i = 0; i < models.size(); ++i) {
            Renderable rbl;
            rbl
                .setModel(&models[i])
                .setWorldTransform(glm::mat4(1.0f));
            if (models[i].getSkeletonDescription()) {
                rbl.setSkeleton(&skeletons[skeletonIndex++]);
            }
            pScene->addRenderable(rbl);
        }

        pScene->setActiveCamera(pScene->addCamera(Camera(M_PI/3.0f, (float) pApp->getWindow()->getWidth()/pApp->getWindow()->getHeight(), 0.1f, 100.0f)));
        pScene->getActiveCamera()->setPosition({0, 0, 2});

        pScene->getDirectionalLight().setDirection({0.5, -2, -1}).setIntensity({2, 2, 2}).setShadowMapEnabled(true);

        pScene->addPointLight(PointLight().setIntensity({4, 8, 2}).setPosition({2, 1, -0.25}).setShadowMapEnabled(true));
        pScene->addPointLight(PointLight().setIntensity({12, 2, 4}).setPosition({-4, 3, 2}).setShadowMapEnabled(true));

        pScene->setAmbientLightIntensity({0.1, 0.1, 0.1});
    }

    // Parameter to be sent to the renderer jobs
    Renderer::RendererJobParam rParam = {};
    rParam.pRenderer = pRenderer;
    rParam.pScene = pScene;
    rParam.pWindow = pApp->getWindow();
    rParam.pScheduler = pScheduler;

    // Frame update job
    FrameUpdateParam uParam = {};
    uParam.pApp = pApp;
    uParam.pScheduler = pScheduler;
    uParam.renderParam = reinterpret_cast<uintptr_t>(&rParam);
    uParam.lastFrameTime = glfwGetTime();
    uParam.lastGameStateTime = uParam.lastFrameTime;

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

    JobScheduler::JobDeclaration updateDecl;
    updateDecl.param = reinterpret_cast<uintptr_t>(&uParam);
    updateDecl.pFunction = FrameUpdateJob;
    updateDecl.signalCounters[0] = pScheduler->getCounterByID("u");
    updateDecl.numSignalCounters = 1;

    // This job implements an update loop by continually adding itself back to the job queue
    pScheduler->enqueueJob(updateDecl);

    // Control transferred to update thread.
    // This thread will sleep until the app sends a signal to quit
    pApp->waitToQuit();

    // waitForQueues() doesn't really work, since it uses a loop to wait on each queue's CV,
    //   if work comes in to a previous queue while we're waiting for a later one there's no guarantee when this returns that all queues are idle.
    // This only really works when all jobs are sent in a batch and no jobs can create other jobs.

    //pScheduler->waitForQueues();

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
