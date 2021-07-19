#include "frustum_culler.h"

#include <algorithm>
#include <numeric>

#include "core/util/math_util.h"
#include "core/util/timer.h"

void FrustumCuller::initForScheduler(JobScheduler* pScheduler) {
    if (pScheduler != m_pScheduler) {
        m_pScheduler = pScheduler;
        m_resultsReadyCounter = pScheduler->getFreeCounter();
    }
}

size_t FrustumCuller::cullSpheres(const BoundingSphere* pBoundingSpheres, size_t count, const glm::mat4& frustumMatrix) {
    std::array<math_util::Plane, 6> frustumPlanes = math_util::frustumPlanes(frustumMatrix);

    m_numToRender = count;
    m_cullResults.resize(count);

    std::transform(pBoundingSpheres, pBoundingSpheres+count, m_cullResults.begin(),
        [&] (const BoundingSphere& b) {
            for (const auto& p : frustumPlanes)
                if (glm::dot(p.normal, b.position) + p.offset < -b.radius) {
                    --m_numToRender;
                    return false;
                }
            return true;
        });

    m_cullResultsForFrame = Timer::getCurrentFrame();

    return m_numToRender;
}

size_t FrustumCuller::cullSceneRenderables(const Scene* pScene, const glm::mat4& frustumMatrix) {

   return cullSpheres(pScene->getRenderableBoundingSpheres().data(), pScene->getNumRenderableIDs(), frustumMatrix);
}

size_t FrustumCuller::cullEntitySpheres(const GameWorld* pGameWorld, const glm::mat4& frustumMatrix) {
    std::array<math_util::Plane, 6> frustumPlanes = math_util::frustumPlanes(frustumMatrix);

    auto view = pGameWorld->getRegistry().view<const Component::Renderable>();

    m_numToRender = view.size();
    m_cullResults.resize(view.size());

    auto pack = view | pGameWorld->getRegistry().view<const BoundingSphere>();

    std::transform(pack.begin(), pack.end(), m_cullResults.begin(),
        [&] (const entt::entity e) {
            const BoundingSphere& b = pack.get<const BoundingSphere>(e);
            for (const auto& p : frustumPlanes)
                if (glm::dot(p.normal, b.position) + p.offset < -b.radius) {
                    --m_numToRender;
                    return false;
                }
            return true;
        });

    m_cullResultsForFrame = Timer::getCurrentFrame();

    return m_numToRender;
}

bool FrustumCuller::hasCullResultsForFrame() const {
    return Timer::getCurrentFrame() == m_cullResultsForFrame;
}

void FrustumCuller::cullSpheresJob(uintptr_t param) {
    CullSpheresParam* pParam = reinterpret_cast<CullSpheresParam*>(param);
    pParam->pCuller->cullSpheres(pParam->pBoundingSpheres, pParam->count, pParam->frustumMatrix);
}

void FrustumCuller::cullSceneRenderablesJob(uintptr_t param) {
    CullSceneParam* pParam = reinterpret_cast<CullSceneParam*>(param);
    pParam->pCuller->cullSceneRenderables(pParam->pScene, pParam->frustumMatrix);
}

void FrustumCuller::cullEntitySpheresJob(uintptr_t param) {
    CullEntitiesParam* pParam = reinterpret_cast<CullEntitiesParam*>(param);
    pParam->pCuller->cullEntitySpheres(pParam->pGameWorld, pParam->frustumMatrix);
}
