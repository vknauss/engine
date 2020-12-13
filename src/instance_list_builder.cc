#include "instance_list_builder.h"

#include <list>
#include <map>

#include <iostream>

#include <glm/glm.hpp>

void InstanceListBuilder::buildInstanceLists(const Scene* pScene, const std::vector<bool>& cullResults, glm::mat4 frustumMatrix) {

    //m_nonSkinnedInstanceLists.clear();
    //m_skinnedInstanceLists.clear();

    m_numNonSkinnedInstances = 0;
    m_numSkinnedInstances = 0;

    std::vector<InstanceList> instanceLists;

    std::map<const Model*, size_t> modelIndices;


    size_t numNonSkinnedModels = 0;
    size_t numSkinnedModels = 0;

    // Model instances
    instanceLists.resize(pScene->m_pModels.size());
    size_t c_index = 0;

    for (uint32_t i = 0; i < pScene->m_pModels.size(); ++i) {
        if (pScene->m_instanceLists[i].empty()) continue;

        InstanceList& instanceList = instanceLists[c_index];
        instanceList.m_pModel = pScene->m_pModels[i];
        modelIndices[instanceList.m_pModel] = c_index;
        ++c_index;

        instanceList.m_instanceTransforms.resize(pScene->m_instanceLists[i].size());
        size_t j = 0;
        for (uint32_t rid : pScene->m_instanceLists[i]) {
            if (!cullResults[rid]) continue;
            instanceList.m_instanceTransforms[j] = pScene->m_renderables[rid].getWorldTransform();
            ++j;
        }
        if (j == 0) { // fully culled
            --c_index;
            modelIndices.erase(instanceList.m_pModel);
            continue;
        }
        instanceList.m_numInstances = j;

        if (instanceList.m_pModel->getSkeletonDescription() != nullptr) {
            instanceList.m_instanceSkeletons.resize(instanceList.m_numInstances);
            j = 0;
            for (uint32_t rid : pScene->m_instanceLists[i]) {
                if (!cullResults[rid]) continue;
                instanceList.m_instanceSkeletons[j] = pScene->m_renderables[rid].getSkeleton();
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

            if (lodInstances[level].empty() || pModel == nullptr) continue;

            size_t modelIndex;
            bool newInsert = false;
            if (auto kv = modelIndices.find(pModel); kv != modelIndices.end()) {
                modelIndex = kv->second;
            } else {
                modelIndex = c_index;
                if (c_index == instanceLists.size()) instanceLists.resize(c_index + lodInstances.size() - level);

                //std::cout << (" -_ " + std::to_string(c_index) + " -- " + std::to_string(instanceLists.size()) + " _- ") << std::endl;

                ++c_index;

                instanceLists[modelIndex].m_pModel = pModel;
                modelIndices[pModel] = modelIndex;
                newInsert = true;
            }
            InstanceList& instanceList = instanceLists[modelIndex];

            size_t j = instanceList.m_numInstances;
            instanceList.m_instanceTransforms.resize(j + lodInstances[level].size());
            for (uint32_t rid : lodInstances[level]) {
                instanceList.m_instanceTransforms[j] = pScene->m_renderables[rid].getWorldTransform();
                ++j;
            }
            instanceList.m_numInstances = j;

            if (instanceList.m_pModel->getSkeletonDescription() != nullptr) {
                j = instanceList.m_instanceSkeletons.size();
                instanceList.m_instanceSkeletons.resize(instanceList.m_numInstances);
                for (uint32_t rid : lodInstances[level]) {
                    instanceList.m_instanceSkeletons[j] = pScene->m_renderables[rid].getSkeleton();
                    ++j;
                }

                if (newInsert) {
                    ++numSkinnedModels;
                }
            } else if (newInsert) {
                ++numNonSkinnedModels;
            }
        }
    }

    m_nonSkinnedInstanceLists.resize(numNonSkinnedModels);
    m_skinnedInstanceLists.resize(numSkinnedModels);

    size_t iNonSkinned = 0;
    size_t iSkinned = 0;

    for (uint32_t i = 0; i < c_index; ++i) {
        const Model* pModel = instanceLists[i].m_pModel;
        if (pModel->getSkeletonDescription() != nullptr) {
            m_skinnedInstanceLists[iSkinned] = std::move(instanceLists[i]);
            m_skinnedInstanceLists[iSkinned].m_instanceTransforms.resize(m_skinnedInstanceLists[iSkinned].m_numInstances);
            m_skinnedInstanceLists[iSkinned].m_instanceSkeletons.resize(m_skinnedInstanceLists[iSkinned].m_numInstances);
            m_numSkinnedInstances += m_skinnedInstanceLists[iSkinned].m_numInstances;
            ++iSkinned;
        } else {
            m_nonSkinnedInstanceLists[iNonSkinned] = std::move(instanceLists[i]);
            m_nonSkinnedInstanceLists[iNonSkinned].m_instanceTransforms.resize(m_nonSkinnedInstanceLists[iNonSkinned].m_numInstances);
            m_numNonSkinnedInstances += m_nonSkinnedInstanceLists[iNonSkinned].m_numInstances;
            ++iNonSkinned;
        }
    }
}

// Kind of silly to duplicate the entire function and change 2 LOCs but this was way easier than trying to insert some sort of custom filtering option
// The renderer knows which version it needs to use. Fuck D.R.Y.!! (I say until I need to change something..)
void InstanceListBuilder::buildShadowMapInstanceLists(const Scene* pScene, const std::vector<bool>& cullResults, glm::mat4 frustumMatrix) {
    //m_nonSkinnedInstanceLists.clear();
    //m_skinnedInstanceLists.clear();

    m_numNonSkinnedInstances = 0;
    m_numSkinnedInstances = 0;

    std::vector<InstanceList> instanceLists;

    std::map<const Model*, size_t> modelIndices;

    size_t numNonSkinnedModels = 0;
    size_t numSkinnedModels = 0;

    // Model instances
    instanceLists.resize(pScene->m_pModels.size());
    size_t c_index = 0;

    for (uint32_t i = 0; i < pScene->m_pModels.size(); ++i) {
        if (pScene->m_instanceLists[i].empty() || !pScene->m_pModels[i]->getMaterial()->isShadowCastingEnabled()) continue;

        InstanceList& instanceList = instanceLists[c_index];
        instanceList.m_pModel = pScene->m_pModels[i];
        modelIndices[instanceList.m_pModel] = c_index;
        ++c_index;

        instanceList.m_instanceTransforms.resize(pScene->m_instanceLists[i].size());
        size_t j = 0;
        for (uint32_t rid : pScene->m_instanceLists[i]) {
            if (!cullResults[rid]) continue;
            instanceList.m_instanceTransforms[j] = pScene->m_renderables[rid].getWorldTransform();
            ++j;
        }
        if (j == 0) { // fully culled
            --c_index;
            modelIndices.erase(instanceList.m_pModel);
            continue;
        }
        instanceList.m_numInstances = j;

        if (instanceList.m_pModel->getSkeletonDescription() != nullptr) {
            instanceList.m_instanceSkeletons.resize(instanceList.m_numInstances);
            j = 0;
            for (uint32_t rid : pScene->m_instanceLists[i]) {
                if (!cullResults[rid]) continue;
                instanceList.m_instanceSkeletons[j] = pScene->m_renderables[rid].getSkeleton();
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

            if (lodInstances[level].empty() || pModel == nullptr || !pModel->getMaterial()->isShadowCastingEnabled()) continue;

            size_t modelIndex;
            bool newInsert = false;
            if (auto kv = modelIndices.find(pModel); kv != modelIndices.end()) {
                modelIndex = kv->second;
            } else {
                modelIndex = c_index;
                if (c_index == instanceLists.size()) instanceLists.resize(c_index + lodInstances.size() - level);
                ++c_index;
                instanceLists[modelIndex].m_pModel = pModel;
                modelIndices[pModel] = modelIndex;
                newInsert = true;
            }
            InstanceList& instanceList = instanceLists[modelIndex];

            size_t j = instanceList.m_numInstances;
            instanceList.m_instanceTransforms.resize(j + lodInstances[level].size());
            for (uint32_t rid : lodInstances[level]) {
                instanceList.m_instanceTransforms[j] = pScene->m_renderables[rid].getWorldTransform();
                ++j;
            }
            instanceList.m_numInstances = j;

            if (instanceList.m_pModel->getSkeletonDescription() != nullptr) {
                j = instanceList.m_instanceSkeletons.size();
                instanceList.m_instanceSkeletons.resize(instanceList.m_numInstances);
                for (uint32_t rid : lodInstances[level]) {
                    instanceList.m_instanceSkeletons[j] = pScene->m_renderables[rid].getSkeleton();
                    ++j;
                }

                if (newInsert) {
                    ++numSkinnedModels;
                }
            } else if (newInsert) {
                ++numNonSkinnedModels;
            }
        }
    }

    m_nonSkinnedInstanceLists.resize(numNonSkinnedModels);
    m_skinnedInstanceLists.resize(numSkinnedModels);

    size_t iNonSkinned = 0;
    size_t iSkinned = 0;

    for (uint32_t i = 0; i < c_index; ++i) {
        const Model* pModel = instanceLists[i].m_pModel;
        if (pModel->getSkeletonDescription() != nullptr) {
            m_skinnedInstanceLists[iSkinned] = std::move(instanceLists[i]);
            m_skinnedInstanceLists[iSkinned].m_instanceTransforms.resize(m_skinnedInstanceLists[iSkinned].m_numInstances);
            m_skinnedInstanceLists[iSkinned].m_instanceSkeletons.resize(m_skinnedInstanceLists[iSkinned].m_numInstances);
            m_numSkinnedInstances += m_skinnedInstanceLists[iSkinned].m_numInstances;
            ++iSkinned;
        } else {
            m_nonSkinnedInstanceLists[iNonSkinned] = std::move(instanceLists[i]);
            m_nonSkinnedInstanceLists[iNonSkinned].m_instanceTransforms.resize(m_nonSkinnedInstanceLists[iNonSkinned].m_numInstances);
            m_numNonSkinnedInstances += m_nonSkinnedInstanceLists[iNonSkinned].m_numInstances;
            ++iNonSkinned;
        }
    }
}
