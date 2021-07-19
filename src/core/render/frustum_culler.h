#ifndef FRUSTUM_CULLER_H_
#define FRUSTUM_CULLER_H_

#include <vector>

#include <glm/glm.hpp>

#include "core/scene/bounding_sphere.h"
#include "core/job_scheduler.h"
#include "core/scene/scene.h"
#include "core/ecs/game_world.h"

class FrustumCuller {

    friend class InstanceListBuilder;

public:

    void initForScheduler(JobScheduler* pScheduler);

    size_t cullSpheres(const BoundingSphere* pBoundingSpheres, size_t count, const glm::mat4& frustumMatrix);

    size_t cullSceneRenderables(const Scene* pScene, const glm::mat4& frustumMatrix);

    size_t cullEntitySpheres(const GameWorld* pGameWorld, const glm::mat4& frustumMatrix);

    size_t getNumToRender() const {
        return m_numToRender;
    }

    const std::vector<bool>& getCullResults() const {
        return m_cullResults;
    }

    bool hasCullResultsForFrame() const;

    JobScheduler::CounterHandle getResultsReadyCounter() const {
        return m_resultsReadyCounter;
    }

    struct CullSpheresParam {
        FrustumCuller* pCuller;
        const BoundingSphere* pBoundingSpheres;
        size_t count;
        glm::mat4 frustumMatrix;
    };

    struct CullSceneParam {
        FrustumCuller* pCuller;
        const Scene* pScene;
        glm::mat4 frustumMatrix;
    };

    struct CullEntitiesParam {
        FrustumCuller* pCuller;
        const GameWorld* pGameWorld;
        glm::mat4 frustumMatrix;
    };

    static void cullSpheresJob(uintptr_t param);
    static void cullSceneRenderablesJob(uintptr_t param);
    static void cullEntitySpheresJob(uintptr_t param);

private:

    std::vector<bool> m_cullResults;

    size_t m_numToRender = 0;

    uint64_t m_cullResultsForFrame = std::numeric_limits<uint64_t>::max();

    JobScheduler* m_pScheduler = nullptr;
    JobScheduler::CounterHandle m_resultsReadyCounter = JobScheduler::COUNTER_NULL;

};

#endif // FRUSTUM_CULLER_H_
