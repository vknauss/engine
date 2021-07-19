#include "animation_system.h"

#include <algorithm>
#include <iostream>

static const AnimationStateTransition* getNextTransition(const AnimationStateNode* pState, float stateTime) {
    std::vector<const AnimationStateTransition*> pCStateTransitions = pState->getOutTransitions();
    const AnimationStateTransition* pNextTransition = nullptr;
    for (const AnimationStateTransition* pTransition : pCStateTransitions) {
        // size_t index = pTransition->getIndex();
        if (!pTransition->hasTriggerFlag()) {
            if (pTransition->waitForOutTime()) {
                if (pNextTransition) {
                    float outTime = pTransition->getOutLocalTimelinePosition();
                    float nextOutTime = pNextTransition->getOutLocalTimelinePosition();
                    if (outTime < nextOutTime && (stateTime < outTime || stateTime > nextOutTime)) {
                        pNextTransition = pTransition;
                    } else if (nextOutTime < outTime && stateTime < outTime && stateTime > nextOutTime) {
                        pNextTransition = pTransition;
                    }
                } else {
                    pNextTransition = pTransition;
                }
            } else {
                pNextTransition = pTransition;
                break;
            }
        }
    }
    return pNextTransition;
}

AnimationSystem::~AnimationSystem() {
    cleanup();
}

uint32_t AnimationSystem::createAnimationStateInstance(const AnimationStateGraph* pStateGraph,
                                                       Skeleton* pSkeleton) {
    bool create = true;
    uint32_t index = (uint32_t) m_stateInstances.size();
    if (!m_deadInstanceInds.empty()) {
        index = m_deadInstanceInds.front();
        m_deadInstanceInds.pop_front();
        m_alive[index] = true;
        create = false;
    } else {
        m_alive.push_back(true);
    }
    AnimationState& instance = create ? m_stateInstances.emplace_back() : m_stateInstances[index];

    instance.pStateGraph = pStateGraph;
    instance.pSkeleton = pSkeleton;

    instance.pClipSet = getClipSet(pSkeleton->getDescription(), pStateGraph);

    instance.pCurrentState = pStateGraph->getInitialState();
    instance.currentStateTimeScale = instance.pCurrentState->hasTimeScaleParameter() ?
        0.0f : instance.pCurrentState->getTimeScale();

    instance.currentStateTimer = 0.0f;

    instance.pNextTransition = getNextTransition(instance.pCurrentState, 0.0f);

    instance.blendParameters.assign(pStateGraph->getNumParameters(), 0.0f);

    return index;
}

void AnimationSystem::destroyAnimationStateInstance(uint32_t instanceID) {
    m_alive[instanceID] = false;
    m_deadInstanceInds.push_front(instanceID);
    if (m_stateInstances[instanceID].pCachedPose) {
        delete m_stateInstances[instanceID].pCachedPose;
        m_stateInstances[instanceID].pCachedPose = nullptr;
    }
    m_stateInstances[instanceID].pCurrentState = nullptr;
    m_stateInstances[instanceID].pCurrentTransition = nullptr;
    m_stateInstances[instanceID].pNextTransition = nullptr;
    m_stateInstances[instanceID].pSkeleton = nullptr;
    m_stateInstances[instanceID].pStateGraph = nullptr;

    m_stateInstances[instanceID].blendParameters.clear();
    m_stateInstances[instanceID].flagsTriggered.clear();
    m_stateInstances[instanceID].parametersUpdated.clear();
}

void AnimationSystem::setFlag(uint32_t instanceID, const std::string& flagName) {
    uint32_t index = 0;
    if (m_stateInstances[instanceID].pStateGraph->getFlagIndexOpt(flagName, index)) {
        m_stateInstances[instanceID].flagsTriggered.push_front(index);
    }
}

void AnimationSystem::setBlendParameter(uint32_t instanceID, const std::string& paramName, float value) {
    uint32_t index = 0;
    if (m_stateInstances[instanceID].pStateGraph->getParameterIndexOpt(paramName, index)) {
        m_stateInstances[instanceID].parametersUpdated.push_front({index, value});
    }
}

void AnimationSystem::processStateUpdates(float dt) {
    m_updateTransition.assign(m_stateInstances.size(), nullptr);

    updateParameters();

    processFlagTriggers();

    updateStateTimers(dt);

    updateTransitions();
}

void AnimationSystem::applyPosesToSkeletons() {
    static const auto getPose = [] (const AnimationState& instance,
                                    const AnimationStateNode* pState,
                                    float time) {
        AnimationInstanceParameterSet parameters(instance.pStateGraph,
                                                 instance.blendParameters,
                                                 pState->getIndex());
        return pState->getBlendTree()->getPose(time,
                                               &parameters,
                                               instance.pStateGraph->getStateClipIndices(pState->getIndex()),
                                               instance.pClipSet->pClips,
                                               pState->isTimeScaleAbsolute());
    };

    for (auto i = 0u; i < m_stateInstances.size(); ++i) {
        AnimationState& instance = m_stateInstances[i];

        instance.pSkeleton->copyLastSkinningMatrices();  // this should be moved

        if (instance.pCurrentTransition == nullptr) {
            instance.pSkeleton->setPose(getPose(instance, instance.pCurrentState, instance.currentStateTimer));
        } else {
            SkeletonPose pose(nullptr);

            if (!instance.useCachedPose) {
                pose = getPose(instance, instance.pCurrentState, instance.currentStateTimer);
            } else {
                pose = *instance.pCachedPose;
            }

            pose.lerp(getPose(instance, instance.pCurrentTransition->getSecondNode(), instance.nextStateTimer),
                      instance.currentTransitionTimer / instance.pCurrentTransition->getDuration());

            instance.pSkeleton->setPose(pose);
        }

        instance.pSkeleton->applyCurrentPose();
        instance.pSkeleton->computeSkinningMatrices();  // this should be moved
    }
}

void AnimationSystem::cleanup() {
    m_stateInstances.clear();
    m_alive.clear();
    m_deadInstanceInds.clear();
    for (AnimationClipSet* pClipSet : m_pClipSets) if (pClipSet) delete pClipSet;
    m_pClipSets.clear();
    m_clipSetIndexMap.clear();
    m_pResManager = nullptr;
}

// Check for each instance if any flags are set which will cause us to begin or begin waiting for a transition
void AnimationSystem::processFlagTriggers() {
    for (auto i = 0u; i < m_stateInstances.size(); ++i) {
        AnimationState& instance = m_stateInstances[i];

        const AnimationStateGraph* pStateGraph = instance.pStateGraph;

        // Only consider transitions from the current state if not in a transition
        // Additionally, if in a cancellable transition consider transitions out of the next state
        const AnimationStateNode* pTransitionFromState = nullptr;
        if (instance.pCurrentState && !instance.pCurrentTransition) {
            pTransitionFromState = instance.pCurrentState;
        } else if (instance.pCurrentTransition && instance.pCurrentTransition->isCancellable()) {
            pTransitionFromState = instance.pCurrentTransition->getSecondNode();
        }

        if (pTransitionFromState) {
            std::vector<const AnimationStateTransition*> pStateTransitions = pTransitionFromState->getOutTransitions();
            // Find the first flag that matches a possible transition
            for (uint32_t flagID : instance.flagsTriggered) {
                // Search pStateTransitions for a transition that has a trigger flag whose index matches flagID
                if (auto it = std::find_if(pStateTransitions.begin(), pStateTransitions.end(),
                        [&] (const AnimationStateTransition* pTransition) {
                            return pTransition->hasTriggerFlag() &&
                                   pStateGraph->getTransitionFlagIndex(pTransition->getIndex()) == flagID;
                        }); it != pStateTransitions.end()) {
                    // pointer to the transition, for convenience
                    const AnimationStateTransition* pTransition = *it;
                    if (pTransition->waitForOutTime()) {
                        // We may not be able to transition immediately
                        if (instance.pNextTransition) {
                            // Already waiting on a transition, check which is first
                            float outTime = pTransition->getOutLocalTimelinePosition();
                            float nextOutTime = instance.pNextTransition->getOutLocalTimelinePosition();
                            float time = (pTransitionFromState == instance.pCurrentState) ?
                                            instance.currentStateTimer :
                                            instance.nextStateTimer;
                            if ((outTime < nextOutTime && (time < outTime || time > nextOutTime)) ||
                                (nextOutTime < outTime && time < outTime && time > nextOutTime)) {
                                instance.pNextTransition = pTransition;
                            }
                        } else {
                            // We will wait for this transition
                            instance.pNextTransition = pTransition;
                        }
                    } else {
                        // Immediately set this transition and stop searching
                        m_updateTransition[i] = pTransition;
                        break;
                    }
                }
            }
        }

        instance.flagsTriggered.clear();
    }
}

void AnimationSystem::updateStateTimers(float dt) {
    for (auto i = 0u; i < m_stateInstances.size(); ++i) {
        AnimationState& instance = m_stateInstances[i];

        if (instance.pCurrentState && !instance.pCurrentTransition) {
            // If we are waiting for a transition and are currently earlier in our local timeline
            // than when we will enter into it, we save that state so we know if we crossed that threshold
            bool primed = (instance.pNextTransition &&
                           instance.currentStateTimer < instance.pNextTransition->getOutLocalTimelinePosition());

            instance.currentStateTimer += instance.currentStateTimeScale * dt;

            bool didLoop = false;
            if (instance.currentStateTimer >= 1.0f && instance.pCurrentState->doesLoop()) {
                instance.currentStateTimer -= 1.0f;
                didLoop = true;
            }

            if (primed && (instance.currentStateTimer >= instance.pNextTransition->getOutLocalTimelinePosition() ||
                           didLoop)) {
                m_updateTransition[i] = instance.pNextTransition;
            }
        } else if (instance.pCurrentTransition) {
            instance.currentTransitionTimer += dt;
            if (instance.currentTransitionTimer >= instance.pCurrentTransition->getDuration()) {
                instance.pCurrentState = instance.pCurrentTransition->getSecondNode();
                instance.pCurrentTransition = nullptr;
                if (instance.pCachedPose) {
                    delete instance.pCachedPose;
                    instance.pCachedPose = nullptr;
                }
                instance.currentStateTimeScale = instance.nextStateTimeScale;
                instance.currentStateTimer = instance.nextStateTimer + instance.currentStateTimeScale * dt;
                instance.pNextTransition = getNextTransition(instance.pCurrentState, instance.currentStateTimer);
            } else {
                if (instance.pCurrentTransition->getOutTransition() == AnimationStateTransition::TRANSITION_ANIMATED) {
                    instance.currentStateTimer += instance.currentStateTimeScale * dt;
                }
                if (instance.pCurrentTransition->getInTransition() == AnimationStateTransition::TRANSITION_ANIMATED) {
                    instance.nextStateTimer += instance.nextStateTimeScale * dt;
                }
            }
        }
    }
}

void AnimationSystem::updateTransitions() {
    for (auto i = 0u; i < m_stateInstances.size(); ++i) {
        if (m_updateTransition[i]) {
            AnimationState& instance = m_stateInstances[i];
            instance.pCurrentTransition = m_updateTransition[i];
            instance.pNextTransition = nullptr;
            instance.nextStateTimer = instance.pCurrentTransition->getInLocalTimelinePosition();
            if (instance.pCurrentTransition->getSecondNode()->hasTimeScaleParameter()) {
                instance.nextStateTimeScale =
                    instance.blendParameters[instance.pStateGraph->
                        getStateTimeScaleParameterIndex(instance.pCurrentTransition->getSecondNode()->getIndex())];
            } else {
                instance.nextStateTimeScale = instance.pCurrentTransition->getSecondNode()->getTimeScale();
            }
            instance.currentTransitionTimer = 0.0f;
            if (instance.pCurrentTransition->getFirstNode() != instance.pCurrentState) {
                // Transitioning from a canceled transition. We must cache the current pose for correct blending
                // Or could be we are transitioning from the "any" state, doesn't matter this still works.
                instance.pCachedPose = new SkeletonPose(*instance.pSkeleton);
                instance.useCachedPose = true;
            } else {
                instance.useCachedPose = false;
            }
        }
    }
}

void AnimationSystem::updateParameters() {
    for (auto i = 0u; i < m_stateInstances.size(); ++i) {
        AnimationState& instance = m_stateInstances[i];
        for (auto pr : instance.parametersUpdated) {
            instance.blendParameters[pr.first] = pr.second;

            // Two disgusting looking if statements lol I love my long variable names
            // Just checking if the updated parameter was a timescale for either the current or next state
            // and updating the appropriate field
            if (instance.pCurrentState->hasTimeScaleParameter() &&
                instance.pStateGraph->
                    getStateTimeScaleParameterIndex(
                        instance.pCurrentState->getIndex())
                    == pr.first) {
                instance.currentStateTimeScale = pr.second;
            }
            if (instance.pCurrentTransition &&
                instance.pCurrentTransition->getSecondNode()->hasTimeScaleParameter() &&
                instance.pStateGraph->
                    getStateTimeScaleParameterIndex(
                        instance.pCurrentTransition->getSecondNode()->getIndex())
                    == pr.first) {
                instance.nextStateTimeScale = pr.second;
            }
        }

        instance.parametersUpdated.clear();
    }
}

const AnimationSystem::AnimationClipSet* AnimationSystem::getClipSet(const SkeletonDescription* pSkeletonDescription,
                                                                     const AnimationStateGraph* pStateGraph) {
    uint32_t index = 0;
    if (auto it1 = m_clipSetIndexMap.find(pSkeletonDescription); it1 != m_clipSetIndexMap.end()) {
        std::unordered_map<const AnimationStateGraph*, uint32_t> submap = it1->second;
        if (auto it2 = submap.find(pStateGraph); it2 != submap.end()) {
            return m_pClipSets[it2->second];
        } else {
            index = (uint32_t) m_pClipSets.size();
            submap.insert({pStateGraph, index});
        }
    } else {
        std::unordered_map<const AnimationStateGraph*, uint32_t> submap;
        index = (uint32_t) m_pClipSets.size();
        submap.insert({pStateGraph, index});
        m_clipSetIndexMap.insert({pSkeletonDescription, submap});
    }

    AnimationClipSet* pClipSet = m_pClipSets.emplace_back(new AnimationClipSet);
    pClipSet->pSkeletonDescription = pSkeletonDescription;
    pClipSet->pStateGraph = pStateGraph;

    std::vector<const AnimationClip*> pClips;
    for (auto& pClip : m_pResManager->pAnimationClips)
        if (pClip->getSkeletonDescription() == pSkeletonDescription)
            pClips.push_back(pClip.get());

    const auto& clipIndexMap = pStateGraph->getClipIndexMap();
    pClipSet->pClips.resize(clipIndexMap.size(), nullptr);
    for (auto kv : clipIndexMap) {
        if (auto it = std::find_if(pClips.begin(), pClips.end(),
                [&name=kv.first] (const AnimationClip* pClip) {
                    return pClip->getName() == name;
                }); it != pClips.end()) {
            pClipSet->pClips[kv.second] = *it;
        }
    }

    return pClipSet;
}
