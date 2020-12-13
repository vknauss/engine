#include "lod_component.h"

LODComponent::LODComponent() {
}

LODComponent::~LODComponent() {
}

void LODComponent::addLOD(float maxScreenHeight, const Model* pModel) {
    int position = 0;
    for(const LOD& lod : m_lODs) {
        if(lod.maxScreenHeight < maxScreenHeight) break;
        position++;
    }

    LOD lod = {};
    lod.pModel = pModel;
    lod.maxScreenHeight = maxScreenHeight;

    m_lODs.insert(m_lODs.begin() + position, lod);

    if (m_lODs.size() == 1) {
        m_boundingSphere = pModel->getBoundingSphere();
    } else {
        const BoundingSphere& b = pModel->getBoundingSphere();
        float dist = glm::length(b.getPosition() - m_boundingSphere.getPosition());
        m_boundingSphere.setPosition(glm::mix(b.getPosition(), m_boundingSphere.getPosition(),
                                              0.5f * (1.0f + (b.getRadius() - m_boundingSphere.getRadius()) / dist)));
        m_boundingSphere.setRadius(0.5f * (b.getRadius() + m_boundingSphere.getRadius() + dist));
    }
}

const Model* LODComponent::getModelLOD(glm::mat4 frustumMatrix, glm::mat4 worldTransform) const {
    if(m_lODs.empty()) return nullptr;

    uint32_t level = getLOD(frustumMatrix, worldTransform);

    return m_lODs[level].pModel;
}

const Model* LODComponent::getModelLOD(uint32_t level) const {
    if(level >= m_lODs.size()) return nullptr;
    return m_lODs[level].pModel;
}

const Model* LODComponent::getModelMinLOD() const {
    if(m_lODs.empty()) return nullptr;
    return m_lODs.front().pModel;
}

const Model* LODComponent::getModelMaxLOD() const {
    if(m_lODs.empty()) return nullptr;
    return m_lODs.back().pModel;
}

float computeScreenHeight(float radius, glm::mat4 frustumMatrix, glm::mat4 worldMatrix) {
    glm::mat4 mvp = frustumMatrix * worldMatrix;
    glm::vec4 ypos[2];
    ypos[0] = mvp * glm::vec4(0, radius, 0, 1);
    ypos[1] = mvp * glm::vec4(0, -radius, 0, 1);
    return ypos[0].y/ypos[0].w - ypos[1].y/ypos[1].w;
}

uint32_t LODComponent::getLOD(glm::mat4 frustumMatrix, glm::mat4 worldTransform) const {
    uint32_t level = 0;
    for (; level < m_lODs.size()-1; ++level) {
        float height = computeScreenHeight(m_boundingSphere.getRadius(), frustumMatrix, worldTransform);
        if (height > m_lODs[level+1].maxScreenHeight) return level;
    }
    return m_lODs.size()-1;
}
