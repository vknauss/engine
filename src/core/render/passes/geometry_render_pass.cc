#include "geometry_render_pass.h"

#include <cstring>

#include <iostream>
#include <algorithm>
#include <numeric>

#include <glm/gtc/matrix_inverse.hpp>

#include "core/render/render_debug.h"

#include "core/util/timer.h"

void GeometryRenderPass::init() {
    m_ownsShaders = loadShaders();
    initRenderTargets();

    m_buildListsParam.pListBuilder = &m_listBuilder;
    m_buildListsParam.predicate    = getFilterPredicate();
    m_buildListsParam.pUser = this;
    m_buildListsParam.useLastTransforms = useLastFrameMatrix();

    m_fillDefaultBucketParam.pBucket             = &m_defaultCallBucket;
    m_fillDefaultBucketParam.useLastFrameMatrix  = useLastFrameMatrix();
    m_fillDefaultBucketParam.useNormalsMatrix    = useNormalsMatrix();
    m_fillDefaultBucketParam.useSkinningMatrices = false;

    m_fillSkinnedBucketParam.pBucket             = &m_skinnedCallBucket;
    m_fillSkinnedBucketParam.useLastFrameMatrix  = useLastFrameMatrix();
    m_fillSkinnedBucketParam.useNormalsMatrix    = useNormalsMatrix();
    m_fillSkinnedBucketParam.useSkinningMatrices = true;
}

void GeometryRenderPass::render() {
    for (int i = 0; i < 2; i++) {
        const Shader* pShader    = (i == 0) ? m_pDefaultShader    : m_pSkinnedShader;
        const CallBucket& bucket = (i == 0) ? m_defaultCallBucket : m_skinnedCallBucket;

        if (bucket.numInstances == 0) continue;

        VKR_DEBUG_CALL(
        pShader->bind();
        onBindShader(pShader);)

        VKR_DEBUG_CALL(
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, bucket.instanceTransformBuffer);)

        const Mesh* pBoundMesh = nullptr;
        for (uint32_t j = 0; j < bucket.callInfos.size(); ++j) {
            if(!bucket.usageFlags[j]) break;

            const CallInfo& callInfo = bucket.callInfos[j];
            const CallHeader& header = callInfo.header;

            if (i == 1) {
                 pShader->setUniform("numJoints", (uint32_t) header.pSkeletonDesc->getNumJoints());
            }

            pShader->setUniform("transformBufferOffset", callInfo.transformBufferOffset);

            VKR_DEBUG_CALL(
            bindMaterial(header.pMaterial, pShader);)

            if (header.pMesh != pBoundMesh) {
                pBoundMesh = header.pMesh;
                VKR_DEBUG_CALL(pBoundMesh->bind();)
            }

            VKR_DEBUG_CALL(
            glDrawElementsInstanced(pBoundMesh->getDrawType(), pBoundMesh->getIndexCount(), GL_UNSIGNED_INT, nullptr, callInfo.numInstances);)
        }
    }
}

void GeometryRenderPass::cleanup() {
    cleanupRenderTargets();
    if (m_defaultCallBucket.buffersInitialized) {
        glDeleteBuffers(1, &m_defaultCallBucket.instanceTransformBuffer);
        m_defaultCallBucket.buffersInitialized = false;
    }
    if (m_skinnedCallBucket.buffersInitialized) {
        glDeleteBuffers(1, &m_skinnedCallBucket.instanceTransformBuffer);
        m_skinnedCallBucket.buffersInitialized = false;
    }
    if (m_ownsShaders) {
        if (m_pDefaultShader) delete m_pDefaultShader;
        if (m_pSkinnedShader) delete m_pSkinnedShader;
    }
    m_pDefaultShader = nullptr;
    m_pSkinnedShader = nullptr;
}


void GeometryRenderPass::initForScheduler(JobScheduler* pScheduler) {
    if (m_pScheduler != pScheduler) {
        m_pScheduler = pScheduler;
        static int i = 0;
        std::string counterID = "GRP" + std::to_string(i++);
        //std::cout << this << " " << counterID << std::endl;
        m_buildListsJobCounter = pScheduler->getCounterByID(counterID); //pScheduler->getFreeCounter();
    }
}

void GeometryRenderPass::updateInstanceBuffers() {
    for (CallBucket* pBucket : {&m_defaultCallBucket, &m_skinnedCallBucket}) {
        if (pBucket->numInstances == 0) continue;

        if(!pBucket->buffersInitialized) {
            glGenBuffers(1, &(pBucket->instanceTransformBuffer));
            pBucket->buffersInitialized = true;
        }

        glBindBuffer(GL_SHADER_STORAGE_BUFFER, pBucket->instanceTransformBuffer);
        glBufferData(GL_SHADER_STORAGE_BUFFER,
                     sizeof(float)*pBucket->instanceTransformFloats.size(),
                     pBucket->instanceTransformFloats.data(), GL_STREAM_DRAW);
    }
}

void GeometryRenderPass::updateInstanceListsJob(uintptr_t param) {
    UpdateParam* pParam = reinterpret_cast<UpdateParam*>(param);

    GeometryRenderPass* pPass = pParam->pPass;

    if (pParam->pCuller->getNumToRender() == 0) {
        pPass->m_listBuilder.clearInstanceLists();
        pPass->m_defaultCallBucket.numInstances = 0;
        pPass->m_skinnedCallBucket.numInstances = 0;
        return;
    }

    BuildInstanceListsParam& buildListsParam = pPass->m_buildListsParam;
    buildListsParam.frustumMatrix = pParam->globalMatrix;
    buildListsParam.pCuller       = pParam->pCuller;
    //buildListsParam.pScene        = pParam->pScene;
    buildListsParam.pGameWorld    = pParam->pGameWorld;

    JobScheduler::JobDeclaration buildListsDecl;
    buildListsDecl.param             = reinterpret_cast<uintptr_t>(&pPass->m_buildListsParam);
    buildListsDecl.numSignalCounters = 1;
    buildListsDecl.signalCounters[0] = pPass->m_buildListsJobCounter;
    buildListsDecl.pFunction         = buildInstanceListsJob;

    pPass->m_pScheduler->enqueueJob(buildListsDecl);

    JobScheduler::JobDeclaration dispatchDecl;
    dispatchDecl.numSignalCounters = 1;
    dispatchDecl.signalCounters[0] = pParam->signalCounter;
    dispatchDecl.waitCounter = pPass->m_buildListsJobCounter;
    dispatchDecl.param = param;
    dispatchDecl.pFunction = GeometryRenderPass::dispatchCallBucketsJob;

    pPass->m_pScheduler->enqueueJob(dispatchDecl);
}

void GeometryRenderPass::fillCallBucketJob(uintptr_t param) {
    FillCallBucketParam* pParam = reinterpret_cast<FillCallBucketParam*>(param);

    CallBucket& bucket = *pParam->pBucket;
    const std::vector<InstanceList>& instanceLists = *pParam->pInstanceLists;
    size_t numInstances = pParam->numInstances;
    glm::mat4 globalMatrix = pParam->globalMatrix;
    bool useNormalsMatrix = pParam->useNormalsMatrix;
    bool useSkinningMatrices = pParam->useSkinningMatrices;

    if (bucket.callInfos.size() < instanceLists.size()) {
        bucket.callInfos.resize(instanceLists.size());
        bucket.usageFlags.resize(instanceLists.size());
    }
    bucket.usageFlags.assign(bucket.usageFlags.size(), false);

    size_t transformSize = (useNormalsMatrix ? 32 : 16) + (pParam->useLastFrameMatrix ? 16 : 0);
    size_t numInstanceTransforms;

    if (!useSkinningMatrices) {
        numInstanceTransforms = numInstances;
    } else {
        numInstanceTransforms = std::accumulate(instanceLists.begin(), instanceLists.end(), size_t(0),
            [] (size_t acc, const InstanceList& il) {
                return acc + il.getModel()->getSkeletonDescription()->getNumJoints() * il.getNumInstances(); });
    }

    bucket.instanceTransformFloats.resize(numInstanceTransforms * transformSize);
    bucket.numInstances = numInstances;

    size_t transformBufferOffset = 0;
    size_t floatBufferOffset = 0;
    for (size_t i = 0; i < instanceLists.size(); ++i) {
        const Model* pModel = instanceLists[i].getModel();

        CallHeader header;
        header.pMesh = pModel->getMesh();
        header.pMaterial = pModel->getMaterial();
        header.pSkeletonDesc = pModel->getSkeletonDescription();

        CallInfo callInfo;
        callInfo.header = header;
        callInfo.transformBufferOffset = transformBufferOffset;
        callInfo.numInstances = instanceLists[i].getNumInstances();

        bucket.callInfos[i] = callInfo;
        bucket.usageFlags[i] = true;

        for (size_t j = 0; j < instanceLists[i].getNumInstances(); ++j) {
            glm::mat4 worldMatrix = instanceLists[i].getInstanceTransforms()[j];
            glm::mat4 worldGlobalMatrix = globalMatrix * worldMatrix;
            glm::mat4 worldGlobalNormalsMatrix;
            glm::mat4 lastWorldGlobalMatrix;
            if (pParam->useNormalsMatrix) worldGlobalNormalsMatrix = pParam->normalsMatrix * glm::inverseTranspose(worldMatrix);
            if (pParam->useLastFrameMatrix) lastWorldGlobalMatrix = pParam->lastGlobalMatrix * instanceLists[i].getLastInstanceTransforms()[j];

            if (!useSkinningMatrices) {
                memcpy(&bucket.instanceTransformFloats[floatBufferOffset], &worldGlobalMatrix[0][0], 16*sizeof(float));
                floatBufferOffset += 16;
                if (useNormalsMatrix) {
                    memcpy(&bucket.instanceTransformFloats[floatBufferOffset], &worldGlobalNormalsMatrix[0][0], 16*sizeof(float));
                    floatBufferOffset += 16;
                }
                if (pParam->useLastFrameMatrix) {
                    memcpy(&bucket.instanceTransformFloats[floatBufferOffset], &lastWorldGlobalMatrix[0][0], 16*sizeof(float));
                    floatBufferOffset += 16;
                }
            } else {
                const Skeleton* pSkeleton = instanceLists[i].getInstanceSkeletons()[j];
                //assert(pSkeleton);

                size_t numJoints = header.pSkeletonDesc->getNumJoints();
                assert(pSkeleton->getSkinningMatrices().size() == numJoints);
                std::vector<glm::mat4> skinningMatrices((useNormalsMatrix ? 2 * numJoints : numJoints) + (pParam->useLastFrameMatrix ? numJoints : 0));
                if (!useNormalsMatrix && !pParam->useLastFrameMatrix) {
                    std::transform(pSkeleton->getSkinningMatrices().begin(), pSkeleton->getSkinningMatrices().end(),
                               skinningMatrices.begin(),
                               [&] (const glm::mat4& m) { return worldGlobalMatrix * m; });
                } else {
                    if (pParam->useLastFrameMatrix) {
                        if (useNormalsMatrix) {
                            for (size_t k = 0; k < numJoints; ++k) {
                                glm::mat4 skinningMatrix = pSkeleton->getSkinningMatrices()[k];
                                skinningMatrices[3*k] = worldGlobalMatrix * skinningMatrix;
                                skinningMatrices[3*k+1] = worldGlobalNormalsMatrix * glm::inverseTranspose(skinningMatrix);
                                skinningMatrices[3*k+2] = lastWorldGlobalMatrix * pSkeleton->getLastSkinningMatrices()[k];
                            }
                        } else {
                            for (size_t k = 0; k < numJoints; ++k) {
                                skinningMatrices[2*k] = worldGlobalMatrix * pSkeleton->getSkinningMatrices()[k];
                                skinningMatrices[2*k+1] = lastWorldGlobalMatrix * pSkeleton->getLastSkinningMatrices()[k];
                            }
                        }
                    } else {
                        for (size_t k = 0; k < numJoints; ++k) {
                            glm::mat4 skinningMatrix = pSkeleton->getSkinningMatrices()[k];
                            skinningMatrices[2*k] = worldGlobalMatrix * skinningMatrix;
                            skinningMatrices[2*k+1] = worldGlobalNormalsMatrix * glm::inverseTranspose(skinningMatrix);
                        }
                    }
                }

                size_t numFloats = 16 * skinningMatrices.size();
                memcpy(&bucket.instanceTransformFloats[floatBufferOffset], skinningMatrices.data(), numFloats * sizeof(float));
                floatBufferOffset += numFloats;
            }
        }
        if (!useSkinningMatrices) {
            transformBufferOffset += instanceLists[i].getNumInstances();
        } else {
            transformBufferOffset += instanceLists[i].getNumInstances() * header.pSkeletonDesc->getNumJoints();
        }
    }
}

void GeometryRenderPass::dispatchCallBucketsJob(uintptr_t param) {
    UpdateParam* pParam = reinterpret_cast<UpdateParam*>(param);

    GeometryRenderPass* pPass = pParam->pPass;

    FillCallBucketParam* fillBucketParams[] {
        &pPass->m_fillDefaultBucketParam,
        &pPass->m_fillSkinnedBucketParam };

    int pindex[2];
    int ndecl = 0;

    if (pPass->m_listBuilder.hasNonSkinnedInstances()) {
        fillBucketParams[0]->pInstanceLists   = &pPass->m_listBuilder.getNonSkinnedInstanceLists();
        fillBucketParams[0]->numInstances     = pPass->m_listBuilder.getNumNonSkinnedInstances();
        fillBucketParams[0]->globalMatrix     = pParam->globalMatrix;
        fillBucketParams[0]->lastGlobalMatrix = pParam->lastGlobalMatrix;
        fillBucketParams[0]->normalsMatrix    = pParam->normalsMatrix;

        pindex[ndecl++] = 0;
    } else {
        pPass->m_defaultCallBucket.numInstances = 0;
    }

    if (pPass->m_listBuilder.hasSkinnedInstances()) {
        fillBucketParams[1]->pInstanceLists   = &pPass->m_listBuilder.getSkinnedInstanceLists();
        fillBucketParams[1]->numInstances     = pPass->m_listBuilder.getNumSkinnedInstances();
        fillBucketParams[1]->globalMatrix     = pParam->globalMatrix;
        fillBucketParams[1]->lastGlobalMatrix = pParam->lastGlobalMatrix;
        fillBucketParams[1]->normalsMatrix    = pParam->normalsMatrix;

        pindex[ndecl++] = 1;
    } else {
        pPass->m_skinnedCallBucket.numInstances = 0;
    }

    if (ndecl == 0) return;

    JobScheduler::JobDeclaration callBucketDecls[2];
    for (int i = 0; i < ndecl; ++i) {
        callBucketDecls[i].param = reinterpret_cast<uintptr_t>(fillBucketParams[pindex[i]]);
        callBucketDecls[i].numSignalCounters = 1;
        callBucketDecls[i].signalCounters[0] = pParam->signalCounter;
        callBucketDecls[i].pFunction = fillCallBucketJob;
    }

    pPass->m_pScheduler->enqueueJobs(ndecl, callBucketDecls);
}

void GeometryRenderPass::buildInstanceListsJob(uintptr_t param) {
    BuildInstanceListsParam* pParam = reinterpret_cast<BuildInstanceListsParam*>(param);

    if (pParam->pCuller->getNumToRender() > 0) {
//        pParam->pListBuilder->buildInstanceLists(pParam->pScene, pParam->pCuller->getCullResults(), pParam->frustumMatrix, pParam->predicate);
        pParam->pListBuilder->buildInstanceLists(pParam->pGameWorld, pParam->pCuller->getCullResults(), pParam->frustumMatrix, pParam->predicate, pParam->useLastTransforms);
    } else {
        pParam->pListBuilder->clearInstanceLists();
    }
}
