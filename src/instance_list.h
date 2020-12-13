#ifndef INSTANCE_LIST_H_
#define INSTANCE_LIST_H_

#include <vector>

#include <glm/glm.hpp>

#include "model.h"

class InstanceList {

    friend class InstanceListBuilder;

    const Model* m_pModel;

    std::vector<glm::mat4> m_instanceTransforms;
    std::vector<const Skeleton*> m_instanceSkeletons;

    size_t m_numInstances;

public:

    explicit InstanceList(Model* pModel) : m_pModel(pModel), m_numInstances(0) {
    }

    InstanceList() : InstanceList(nullptr) {
    }

    InstanceList(InstanceList&& i) :
        m_pModel(std::move(i.m_pModel)),
        m_instanceTransforms(std::move(i.m_instanceTransforms)),
        m_instanceSkeletons(std::move(i.m_instanceSkeletons)),
        m_numInstances(std::move(i.m_numInstances)) {
    }

    InstanceList& operator=(InstanceList&& i) {
        m_pModel = std::move(i.m_pModel);
        m_instanceTransforms = std::move(i.m_instanceTransforms);
        m_instanceSkeletons = std::move(i.m_instanceSkeletons);
        m_numInstances = std::move(i.m_numInstances);
        return *this;
    }

    const Model* const getModel() const {
        return m_pModel;
    }

    const std::vector<glm::mat4>& getInstanceTransforms() const {
        return m_instanceTransforms;
    }

    const std::vector<const Skeleton*>& getInstanceSkeletons() const {
        return m_instanceSkeletons;
    }

    size_t getNumInstances() const {
        return m_numInstances;
    }

};

#endif // INSTANCE_LIST_H_
