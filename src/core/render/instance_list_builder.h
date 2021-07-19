#ifndef INSTANCE_LIST_BUILDER_H_
#define INSTANCE_LIST_BUILDER_H_

#include "instance_list.h"

class Scene;
class GameWorld;

class InstanceListBuilder {

    //friend class Renderer;

    std::vector<InstanceList> m_nonSkinnedInstanceLists;
    std::vector<InstanceList> m_skinnedInstanceLists;

    size_t m_numNonSkinnedInstances;
    size_t m_numSkinnedInstances;

    //std::vector<InstanceList> m_nonSkinnedShadowCasterInstanceLists;
    //std::vector<InstanceList> m_skinnedShadowCasterInstanceLists;

public:

    typedef bool (*filterPredicate) (const Model*);

    InstanceListBuilder() : m_numNonSkinnedInstances(0), m_numSkinnedInstances(0) {
    }

    // Note: frustum matrix only needed for LOD calculations
    // predicate may be null, in which case it acts as if it returns true for every model, i.e., none will be filtered
    void buildInstanceLists(const Scene* pScene, const std::vector<bool>& cullResults, glm::mat4 frustumMatrix, filterPredicate predicate = nullptr);


    // Note: frustum matrix only needed for LOD calculations
    // predicate may be null, in which case it acts as if it returns true for every model, i.e., none will be filtered
    void buildInstanceLists(const GameWorld* pGameWorld, const std::vector<bool>& cullResults, glm::mat4 frustumMatrix, filterPredicate predicate = nullptr, bool useLastTransforms = false);

    // Filter out instances of Models that are not shadow-casting
    //void buildShadowMapInstanceLists(const Scene* pScene, const std::vector<bool>& cullResults, glm::mat4 frustumMatrix);

    void clearInstanceLists() {
        //m_nonSkinnedInstanceLists.clear();
        //m_skinnedInstanceLists.clear();

        m_numNonSkinnedInstances = 0;
        m_numSkinnedInstances = 0;
    }

    const std::vector<InstanceList>& getNonSkinnedInstanceLists() const {
        return m_nonSkinnedInstanceLists;
    }

    const std::vector<InstanceList>& getSkinnedInstanceLists() const {
        return m_skinnedInstanceLists;
    }

    size_t getNumNonSkinnedInstances() const {
        return m_numNonSkinnedInstances;
    }

    size_t getNumSkinnedInstances() const {
        return m_numSkinnedInstances;
    }

    bool hasNonSkinnedInstances() const {
        return m_numNonSkinnedInstances > 0;
    }

    bool hasSkinnedInstances() const {
        return m_numSkinnedInstances > 0;
    }

    bool hasInstances() const {
        return hasNonSkinnedInstances() || hasSkinnedInstances();
    }

};

#endif // INSTANCE_LIST_BUILDER_H_
