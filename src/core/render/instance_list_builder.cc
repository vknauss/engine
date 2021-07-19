#include "instance_list_builder.h"

#include <list>
#include <map>

#include <iostream>
#include <sstream>

#include <glm/glm.hpp>

#include "core/ecs/components.h"
#include "core/ecs/game_world.h"
#include "core/scene/scene.h"

void InstanceListBuilder::buildInstanceLists(const Scene* pScene, const std::vector<bool>& cullResults, glm::mat4 frustumMatrix, bool (*filterPredicate) (const Model*)) {
    //m_nonSkinnedInstanceLists.clear();
    //m_skinnedInstanceLists.clear();

    m_numNonSkinnedInstances = 0;
    m_numSkinnedInstances = 0;

    std::vector<InstanceList> instanceLists;

    //std::map<const Model*, size_t> modelIndices;

    size_t numNonSkinnedModels = 0;
    size_t numSkinnedModels = 0;

    // Model instances
    instanceLists.resize(pScene->m_pModels.size());
    size_t c_index = 0;

    std::vector<size_t> numMissingSkeletons(pScene->m_pModels.size(), 0);
    size_t numModelsMissingSkeletons = 0;

    for (uint32_t i = 0; i < pScene->m_pModels.size(); ++i) {
        if (pScene->m_instanceLists[i].empty() ||
            !pScene->m_pModels[i]->getMesh() ||
            pScene->m_pModels[i]->getMesh()->getVertexCount() == 0 ||
            (filterPredicate && !filterPredicate(pScene->m_pModels[i])))
                continue;

        InstanceList& instanceList = instanceLists[c_index];
        instanceList.m_pModel = pScene->m_pModels[i];
        //modelIndices[instanceList.m_pModel] = c_index;
        ++c_index;

        instanceList.m_instanceTransforms.resize(pScene->m_instanceLists[i].size());
        instanceList.m_lastInstanceTransforms.resize(pScene->m_instanceLists[i].size());

        size_t j = 0;
        for (uint32_t rid : pScene->m_instanceLists[i]) {
            if (!cullResults[rid]) continue;
            instanceList.m_instanceTransforms[j] = pScene->m_renderables[rid].getWorldTransform();
            instanceList.m_lastInstanceTransforms[j] = pScene->m_renderables[rid].getLastWorldTransform();
            ++j;
        }
        if (j == 0) { // fully culled
            --c_index;
            //modelIndices.erase(instanceList.m_pModel);
            continue;
        }
        instanceList.m_numInstances = j;

        if (instanceList.m_pModel->getSkeletonDescription() != nullptr) {
            instanceList.m_instanceSkeletons.resize(instanceList.m_numInstances);
            j = 0;
            for (uint32_t rid : pScene->m_instanceLists[i]) {
                if (!cullResults[rid]) continue;
                if (!(instanceList.m_instanceSkeletons[j] = pScene->m_renderables[rid].getSkeleton()))
                    ++numMissingSkeletons[c_index-1];
                if (numMissingSkeletons[c_index-1] > 0) ++numModelsMissingSkeletons;
                ++j;
            }

            ++numSkinnedModels;
        } else {
            ++numNonSkinnedModels;
        }
    }

    // LOD instances

    for (uint32_t i = 0; i < pScene->m_pLODComponents.size(); ++i) {
        if (pScene->m_lodInstanceLists[i].empty()) continue;

        std::vector<std::list<uint32_t>> lodInstances(pScene->m_pLODComponents[i]->getNumLODs());

        for (uint32_t rid : pScene->m_lodInstanceLists[i]) {
            if (!cullResults[rid]) continue;
            uint32_t level = pScene->m_pLODComponents[i]->getLOD(frustumMatrix, pScene->m_renderables[rid].getWorldTransform());
            lodInstances[level].push_back(rid);
        }

        for (uint32_t level = 0; level < lodInstances.size(); ++level) {
            const Model* pModel = pScene->m_pLODComponents[i]->getModelLOD(level);

            if (lodInstances[level].empty() ||
                !pModel ||
                !pModel->getMesh() ||
                pModel->getMesh()->getVertexCount() == 0 ||
                (filterPredicate && !filterPredicate(pModel)))
                    continue;

            size_t modelIndex;
            //bool newInsert = false;
            //if (auto kv = modelIndices.find(pModel); kv != modelIndices.end()) {
            //    modelIndex = kv->second;
            //} else {
                modelIndex = c_index;
                if (c_index == instanceLists.size()) instanceLists.resize(c_index + lodInstances.size() - level);
                numMissingSkeletons.resize(instanceLists.size(), 0);
                ++c_index;
                instanceLists[modelIndex].m_pModel = pModel;
            //    modelIndices[pModel] = modelIndex;
            //    newInsert = true;
            //}
            InstanceList& instanceList = instanceLists[modelIndex];

            size_t j = instanceList.m_numInstances;
            instanceList.m_instanceTransforms.resize(j + lodInstances[level].size());
            instanceList.m_lastInstanceTransforms.resize(j + lodInstances[level].size());
            for (uint32_t rid : lodInstances[level]) {
                instanceList.m_instanceTransforms[j] = pScene->m_renderables[rid].getWorldTransform();
                instanceList.m_lastInstanceTransforms[j] = pScene->m_renderables[rid].getLastWorldTransform();
                ++j;
            }
            instanceList.m_numInstances = j;

            if (instanceList.m_pModel->getSkeletonDescription() != nullptr) {
                j = instanceList.m_instanceSkeletons.size();
                instanceList.m_instanceSkeletons.resize(instanceList.m_numInstances);
                for (uint32_t rid : lodInstances[level]) {
                    if (!(instanceList.m_instanceSkeletons[j] = pScene->m_renderables[rid].getSkeleton()))
                        ++numMissingSkeletons[modelIndex];
                    if (numMissingSkeletons[modelIndex] > 0) ++numModelsMissingSkeletons;
                    ++j;
                }

                //if (newInsert) {
                    ++numSkinnedModels;
                //}
            } else /*if (newInsert)*/ {
                ++numNonSkinnedModels;
            }
        }
    }

    m_nonSkinnedInstanceLists.resize(numNonSkinnedModels+numModelsMissingSkeletons);
    m_skinnedInstanceLists.resize(numSkinnedModels);

    size_t iNonSkinned = 0;
    size_t iSkinned = 0;

    for (uint32_t i = 0; i < c_index; ++i) {
        const Model* pModel = instanceLists[i].m_pModel;
        if (pModel->getSkeletonDescription() != nullptr) {
            if (numMissingSkeletons[i] == 0) {
                m_skinnedInstanceLists[iSkinned] = std::move(instanceLists[i]);
                m_skinnedInstanceLists[iSkinned].m_instanceTransforms.resize(m_skinnedInstanceLists[iSkinned].m_numInstances);
                m_skinnedInstanceLists[iSkinned].m_lastInstanceTransforms.resize(m_skinnedInstanceLists[iSkinned].m_numInstances);
                m_skinnedInstanceLists[iSkinned].m_instanceSkeletons.resize(m_skinnedInstanceLists[iSkinned].m_numInstances);
                m_numSkinnedInstances += m_skinnedInstanceLists[iSkinned].m_numInstances;
                ++iSkinned;
            } else {
                size_t iiSkinned = 0, iiNonSkinned = 0;

                m_skinnedInstanceLists[iSkinned].m_pModel = pModel;
                m_skinnedInstanceLists[iSkinned].m_instanceTransforms.resize(instanceLists[i].m_numInstances - numMissingSkeletons[i]);
                m_skinnedInstanceLists[iSkinned].m_lastInstanceTransforms.resize(instanceLists[i].m_numInstances - numMissingSkeletons[i]);
                m_skinnedInstanceLists[iSkinned].m_instanceSkeletons.resize(instanceLists[i].m_numInstances - numMissingSkeletons[i]);

                m_nonSkinnedInstanceLists[iNonSkinned].m_pModel = pModel;
                m_nonSkinnedInstanceLists[iNonSkinned].m_instanceTransforms.resize(numMissingSkeletons[i]);
                m_nonSkinnedInstanceLists[iNonSkinned].m_lastInstanceTransforms.resize(numMissingSkeletons[i]);

                for (auto j = 0u; j < instanceLists[i].m_numInstances; ++j) {
                    if (instanceLists[i].m_instanceSkeletons[j]) {
                        m_skinnedInstanceLists[iSkinned].m_instanceTransforms[iiSkinned] = instanceLists[i].m_instanceTransforms[j];
                        m_skinnedInstanceLists[iSkinned].m_lastInstanceTransforms[iiSkinned] = instanceLists[i].m_lastInstanceTransforms[j];
                        m_skinnedInstanceLists[iSkinned].m_instanceSkeletons[iiSkinned] = instanceLists[i].m_instanceSkeletons[j];
                        ++iiSkinned;
                    } else {
                        m_nonSkinnedInstanceLists[iNonSkinned].m_instanceTransforms[iiNonSkinned] = instanceLists[i].m_instanceTransforms[j];
                        m_nonSkinnedInstanceLists[iNonSkinned].m_lastInstanceTransforms[iiNonSkinned] = instanceLists[i].m_lastInstanceTransforms[j];
                        ++iiNonSkinned;
                    }
                }

                m_skinnedInstanceLists[iSkinned].m_numInstances = iiSkinned;
                m_nonSkinnedInstanceLists[iNonSkinned].m_numInstances = iiNonSkinned;

                m_numNonSkinnedInstances += iiNonSkinned;
                m_numSkinnedInstances += iiSkinned;

                if (iiSkinned > 0) ++iSkinned;
                ++iNonSkinned;
            }
        } else {
            m_nonSkinnedInstanceLists[iNonSkinned] = std::move(instanceLists[i]);
            m_nonSkinnedInstanceLists[iNonSkinned].m_instanceTransforms.resize(m_nonSkinnedInstanceLists[iNonSkinned].m_numInstances);
            m_nonSkinnedInstanceLists[iNonSkinned].m_lastInstanceTransforms.resize(m_nonSkinnedInstanceLists[iNonSkinned].m_numInstances);

            /*m_nonSkinnedInstanceLists[iNonSkinned].m_pModel = pModel;
            m_nonSkinnedInstanceLists[iNonSkinned].m_instanceTransforms = std::move(instanceLists[i].m_instanceTransforms);
            m_nonSkinnedInstanceLists[iNonSkinned].m_lastInstanceTransforms = std::move(instanceLists[i].m_lastInstanceTransforms);
            m_nonSkinnedInstanceLists[iNonSkinned].m_numInstances = instanceLists[i].m_numInstances;*/

            m_numNonSkinnedInstances += m_nonSkinnedInstanceLists[iNonSkinned].m_numInstances;
            ++iNonSkinned;
        }
    }
    //instanceLists.clear();
}

void InstanceListBuilder::buildInstanceLists(const GameWorld* pGameWorld,
                                             const std::vector<bool>& cullResults,
                                             glm::mat4 frustumMatrix,
                                             InstanceListBuilder::filterPredicate predicate,
                                             bool useLastTransforms) {
    m_numNonSkinnedInstances = 0;
    m_numSkinnedInstances = 0;

    std::vector<size_t> modelNumInstances;
    std::vector<size_t> skinnedModelNumInstances;

    std::vector<size_t> modelFirstIndex;
    std::vector<size_t> skinnedModelFirstIndex;

    size_t numNonSkinnedModels = 0;
    size_t numSkinnedModels = 0;

    const Model* pCModel = nullptr;
    const Model* pCSkinnedModel = nullptr;

    auto view = pGameWorld->getRegistry().view<const Component::Renderable>();

    auto c_index = 0u;
    for (const auto &&[e, r] : view.each()) {
        if (r.pModel && cullResults[c_index] && (!predicate || predicate(r.pModel))) {
            if (r.pSkeleton) {
                if (pCSkinnedModel == r.pModel) {
                    ++skinnedModelNumInstances.back();
                } else {
                    pCSkinnedModel = r.pModel;
                    ++numSkinnedModels;
                    skinnedModelNumInstances.push_back(1);
                    skinnedModelFirstIndex.push_back(c_index);
                }
                ++m_numSkinnedInstances;
            } else {
                if (pCModel == r.pModel) {
                    ++modelNumInstances.back();
                } else {
                    pCModel = r.pModel;
                    ++numNonSkinnedModels;
                    modelNumInstances.push_back(1);
                    modelFirstIndex.push_back(c_index);
                }
                ++m_numNonSkinnedInstances;
            }
        }
        ++c_index;
    }

    m_nonSkinnedInstanceLists.resize(numNonSkinnedModels);
    m_skinnedInstanceLists.resize(numSkinnedModels);

    if (numNonSkinnedModels == 0 && numSkinnedModels == 0) {
        clearInstanceLists();
        return;
    }

    auto tview = pGameWorld->getRegistry().view<const Component::Transform>();
    auto pack = view | tview;

    pCModel = pCSkinnedModel = nullptr;

    c_index = 0u;

    auto c_modelIndex = 0u;
    auto c_skinnedModelIndex = 0u;

    auto c_nonSkinnedListIndex = 0u;
    auto c_skinnedListIndex = 0u;

    InstanceList* pCInstanceList = nullptr;
    unsigned int* pCListIndex = nullptr;

    for (const auto &&[e, r, t] : pack.each()) {
        if (r.pModel && cullResults[c_index] && (!predicate || predicate(r.pModel))) {
            if (r.pSkeleton) {
                if (pCSkinnedModel != r.pModel) {
                    pCSkinnedModel = r.pModel;

                    ++c_skinnedModelIndex;

                    pCInstanceList = &m_skinnedInstanceLists[c_skinnedModelIndex - 1];

                    pCInstanceList->m_pModel = pCSkinnedModel;
                    pCInstanceList->m_numInstances = skinnedModelNumInstances[c_skinnedModelIndex - 1];
                    pCInstanceList->m_instanceSkeletons.resize(pCInstanceList->m_numInstances);
                    pCInstanceList->m_instanceTransforms.resize(pCInstanceList->m_numInstances);
                    if (useLastTransforms)
                        pCInstanceList->m_lastInstanceTransforms.resize(pCInstanceList->m_numInstances);

                    pCListIndex = &(c_skinnedListIndex = 0);
                }
            } else {
                if (pCModel != r.pModel) {
                    pCModel = r.pModel;

                    ++c_modelIndex;

                    pCInstanceList = &m_nonSkinnedInstanceLists[c_modelIndex - 1];

                    pCInstanceList->m_pModel = pCModel;
                    pCInstanceList->m_numInstances = modelNumInstances[c_modelIndex - 1];
                    pCInstanceList->m_instanceTransforms.resize(pCInstanceList->m_numInstances);
                    if (useLastTransforms)
                        pCInstanceList->m_lastInstanceTransforms.resize(pCInstanceList->m_numInstances);

                    pCListIndex = &(c_nonSkinnedListIndex = 0);
                }
            }
            pCInstanceList->m_instanceTransforms[*pCListIndex] = t.world;
            if (useLastTransforms)
                pCInstanceList->m_lastInstanceTransforms[*pCListIndex] = t.lastWorld;
            if (r.pSkeleton)
                pCInstanceList->m_instanceSkeletons[*pCListIndex] = r.pSkeleton;

            ++(*pCListIndex);
        }
        ++c_index;
    }
}
