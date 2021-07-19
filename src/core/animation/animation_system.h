#ifndef ANIMATION_SYSTEM_H_INCLUDED
#define ANIMATION_SYSTEM_H_INCLUDED

#include <forward_list>
#include <unordered_map>
#include <vector>

#include "animation.h"
#include "animation_blend_tree.h"
#include "animation_instance.h"
#include "animation_state_graph.h"

#include "core/resources/resource_manager.h"

class AnimationSystem {

    struct AnimationClipSet {
        const SkeletonDescription* pSkeletonDescription = nullptr;
        const AnimationStateGraph* pStateGraph = nullptr;

        std::vector<const AnimationClip*> pClips;
    };

    // An Animation State instance type
    // Not intended to be used outside of this class
    struct AnimationState {
        const AnimationStateGraph* pStateGraph = nullptr;

        const AnimationStateNode* pCurrentState = nullptr;
        const AnimationStateTransition* pCurrentTransition = nullptr;
        const AnimationStateTransition* pNextTransition = nullptr;

        Skeleton* pSkeleton = nullptr;

        SkeletonPose* pCachedPose = nullptr;

        const AnimationClipSet* pClipSet = nullptr;

        std::vector<float> blendParameters;

        std::forward_list<uint32_t> flagsTriggered;
        std::forward_list<std::pair<uint32_t, float>> parametersUpdated;

        float currentStateTimer = 0.0f;
        float currentTransitionTimer = 0.0f;
        float nextStateTimer = 0.0f;

        float currentStateTimeScale = 1.0f;
        float nextStateTimeScale = 1.0f;

        bool useCachedPose = false;
    };



public:

    ~AnimationSystem();

    // Will be used for retrieving animation clips
    void setResourceManager(const ResourceManager* pResManager) {
        m_pResManager = pResManager;
    }

    uint32_t createAnimationStateInstance(const AnimationStateGraph* pStateGraph,
                                          Skeleton* pSkeleton);

    void destroyAnimationStateInstance(uint32_t instanceID);

    // setFlag() and setBlendParameter() take effect when processStateUpdates() is called

    // Trigger a flag to 'true' for the frame, if it exists
    // Does not persist after update
    void setFlag(uint32_t instanceID, const std::string& flagName);

    // Set a blend parameter to value if it exists
    // Persists until set again
    void setBlendParameter(uint32_t instanceID, const std::string& paramName, float value);

    float getBlendParameter(uint32_t instanceID, const std::string& paramName) const {
        const AnimationState& instance = m_stateInstances[instanceID];
        uint32_t paramID = 0;
        if (instance.pStateGraph->getParameterIndexOpt(paramName, paramID)) {
            return instance.blendParameters[paramID];
        } else {
            return 0.0f;
        }
    }

    // To allow for controller code to observe the internal animation state of an instance
    const AnimationStateNode* getCurrentState(uint32_t instanceID) const {
        return m_stateInstances[instanceID].pCurrentState;
    }

    // Process flag and blend parameter updates that have been enqueued
    // Advance state and transition timers
    // Logic for state machine transitions
    void processStateUpdates(float dt);

    // Evaluate blend trees and copy their poses to the skeletons
    void applyPosesToSkeletons();

    void cleanup();

private:

    std::vector<AnimationState> m_stateInstances;
    std::vector<const AnimationStateTransition*> m_updateTransition;
    std::vector<bool> m_alive;

    std::forward_list<uint32_t> m_deadInstanceInds;

    std::vector<AnimationClipSet*> m_pClipSets;
    std::unordered_map<const SkeletonDescription*,
                       std::unordered_map<const AnimationStateGraph*, uint32_t>>
        m_clipSetIndexMap;

    const ResourceManager* m_pResManager = nullptr;

    void updateParameters();

    void processFlagTriggers();

    void updateStateTimers(float dt);

    void updateTransitions();

    const AnimationClipSet* getClipSet(const SkeletonDescription* pSkeletonDescription,
                                       const AnimationStateGraph* pStateGraph);

};

#endif // ANIMATION_SYSTEM_H_INCLUDED
