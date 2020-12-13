#ifndef LOD_COMPONENT_H_
#define LOD_COMPONENT_H_

#include <vector>

#include "model.h"

class LODComponent {

    struct LOD {
        const Model* pModel;

        float maxScreenHeight;
    };

public:

    LODComponent();

    ~LODComponent();

    void addLOD(float maxScreenHeight, const Model* pModel);

    const Model* getModelLOD(glm::mat4 frustumMatrix, glm::mat4 worldTransform) const;
    const Model* getModelLOD(uint32_t level) const;
    const Model* getModelMinLOD() const;
    const Model* getModelMaxLOD() const;

    uint32_t getNumLODs() const {
        return m_lODs.size();
    }

    uint32_t getLOD(glm::mat4 frustumMatrix, glm::mat4 worldTransform) const;

    const BoundingSphere& getBoundingSphere() const {
        return m_boundingSphere;
    }

private:

    std::vector<LOD> m_lODs;

    BoundingSphere m_boundingSphere;
};

#endif // LOD_COMPONENT_H_
