#include "animation_instance.h"

AnimationInstanceParameterSet::
AnimationInstanceParameterSet(const AnimationStateGraph* pStateGraph,
                              const std::vector<float>& parameters,
                              size_t stateIndex) :
    m_pStateGraph(pStateGraph),
    m_parameters(parameters),
    m_stateIndex(stateIndex) {
}

float AnimationInstanceParameterSet::getParameter(size_t index) const {
    return m_parameters[m_pStateGraph->getStateParameterIndex(m_stateIndex, index)];
}

/*
AnimationInstance::AnimationInstance() :
    m_cachedPose(nullptr),
    m_pose(nullptr) {
}

void AnimationInstance::setStateGraph(const AnimationStateGraph* pStateGraph) {
    m_pStateGraph = pStateGraph;

    const auto& pTransitions = pStateGraph->getTransitions();
    m_transitionHasFlag.resize(pTransitions.size());
    m_transitionFlagIndices.resize(pTransitions.size());
    for (size_t i = 0; i < pTransitions.size(); ++i) {
        m_transitionHasFlag[i] = pTransitions[i]->hasTriggerFlag();
        m_transitionFlagIndices[i] = getFlagIndex(pTransitions[i]->getTriggerFlagName());
    }

    const auto& pStates = pStateGraph->getStates();
    m_stateParameterIndices.resize(pStates.size());
    m_stateHasTimeScaleParameter.resize(pStates.size());
    m_stateTimeScaleParameterIndices.resize(pStates.size());
    m_stateTimeScales.resize(pStates.size());
    m_stateTimers.assign(pStates.size(), 0.0f);
    for (size_t i = 0; i < pStates.size(); ++i) {
        const auto& indexMap = pStates[i]->getBlendTree()->getParameterIndexMap();
        m_stateParameterIndices[i].resize(indexMap.size());
        for (auto kv : indexMap) {
            m_stateParameterIndices[i][kv.second] = getParameterIndex(kv.first);
        }

        if (pStates[i]->hasTimeScaleParameter()) {
            m_stateHasTimeScaleParameter[i] = true;
            m_stateTimeScaleParameterIndices[i] = getParameterIndex(pStates[i]->getTimeScaleParameterName());
        } else {
            m_stateTimeScales[i] = pStates[i]->getTimeScale();
        }
    }
}

void AnimationInstance::triggerFlag(const std::string& name) {
    //bool eventHandled = false;
    size_t flagIndex = getFlagIndex(name);

    const AnimationStateNode* pState = nullptr;
    if (m_pCurrentState && !m_pCurrentTransition)
        pState = m_pCurrentState;
    else if (m_pCurrentTransition && m_pCurrentTransition->isCancellable())
        pState = m_pCurrentTransition->getSecondNode();

    if (pState) {
        std::vector<const AnimationStateTransition*> pStateTransitions = pState->getOutTransitions();
        for (const AnimationStateTransition* pTransition : pStateTransitions) {
            size_t index = pTransition->getIndex();
            if (m_transitionHasFlag[index] && m_transitionFlagIndices[index] == flagIndex) {
                if (pTransition->waitForOutTime()) {
                    if (m_pNextTransition) {
                        float outTime = pTransition->getOutLocalTimelinePosition();
                        float nextOutTime = m_pNextTransition->getOutLocalTimelinePosition();
                        float time = m_stateTimers[pState->getIndex()];
                        if (outTime < nextOutTime && (time < outTime || time > nextOutTime)) {
                            m_pNextTransition = pTransition;
                        } else if (nextOutTime < outTime && time < outTime && time > nextOutTime) {
                            m_pNextTransition = pTransition;
                        }
                    } else {
                        m_pNextTransition = pTransition;
                    }
                } else {
                    setCurrentTransition(pTransition);
                    //eventHandled = true;
                    break;
                }
            }
        }
    }
}

bool AnimationInstance::getFlag(const std::string& name) {
    if (auto it = m_flagIndexMap.find(name); it != m_flagIndexMap.end()) {
        return m_flags[it->second];
    } else {
        m_flagIndexMap[name] = m_flags.size();
        m_flags.push_back(false);
        return false;
    }
}

bool AnimationInstance::getFlag(size_t index) const {
    assert(index < m_flags.size());
    return m_flags[index];
}

size_t AnimationInstance::getFlagIndex(const std::string& name) {
    if (auto it = m_flagIndexMap.find(name); it != m_flagIndexMap.end()) {
        return it->second;
    } else {
        m_flagIndexMap[name] = m_flags.size();
        m_flags.push_back(false);
        return m_flags.size() - 1;
    }
}

void AnimationInstance::setParameter(const std::string& name, float value) {
    if (auto it = m_parameterIndexMap.find(name); it != m_parameterIndexMap.end()) {
        m_parameters[it->second] = value;
    } else {
        m_parameterIndexMap[name] = m_parameters.size();
        m_parameters.push_back(value);
    }
}

void AnimationInstance::setParameter(size_t index, float value) {
    assert(index < m_parameters.size());
    m_parameters[index] = value;
}

float AnimationInstance::getParameter(const std::string& name) {
    if (auto it = m_parameterIndexMap.find(name); it != m_parameterIndexMap.end()) {
        return m_parameters[it->second];
    } else {
        m_parameterIndexMap[name] = m_parameters.size();
        m_parameters.push_back(0.0f);
        return 0.0f;
    }
}

float AnimationInstance::getParameter(size_t index) const {
    assert(index < m_parameters.size());
    return m_parameters[index];
}

float AnimationInstance::getStateParameter(size_t state, size_t index) const {
    assert(state < m_stateParameterIndices.size() && index < m_stateParameterIndices[state].size());
    index = m_stateParameterIndices[state][index];
    assert(index < m_parameters.size());
    return m_parameters[index];
}

size_t AnimationInstance::getParameterIndex(const std::string& name) {
    if (auto it = m_parameterIndexMap.find(name); it != m_parameterIndexMap.end()) {
        return it->second;
    } else {
        m_parameterIndexMap[name] = m_parameters.size();
        m_parameters.push_back(0.0f);
        return m_parameters.size() - 1;
    }
}

void AnimationInstance::setCurrentState(const AnimationStateNode* pNode) {
    m_pCurrentState = pNode;
    m_pCurrentTransition = nullptr;
    std::vector<const AnimationStateTransition*> pCStateTransitions = m_pCurrentState->getOutTransitions();
    for (const AnimationStateTransition* pTransition : pCStateTransitions) {
        size_t index = pTransition->getIndex();
        // bool setNext = false;
        if ((m_transitionHasFlag[index] && getFlag(m_transitionFlagIndices[index])) || !m_transitionHasFlag[index]) {
            if (pTransition->waitForOutTime()) {
                if (m_pNextTransition) {
                    float outTime = pTransition->getOutLocalTimelinePosition();
                    float nextOutTime = m_pNextTransition->getOutLocalTimelinePosition();
                    float time = m_stateTimers[m_pCurrentState->getIndex()];
                    if (outTime < nextOutTime && (time < outTime || time > nextOutTime)) {
                        m_pNextTransition = pTransition;
                    } else if (nextOutTime < outTime && time < outTime && time > nextOutTime) {
                        m_pNextTransition = pTransition;
                    }
                } else {
                    m_pNextTransition = pTransition;
                }
            } else {
                setCurrentTransition(pTransition);
                break;
            }
        }
    }
}

//#include <iostream>
void AnimationInstance::setCurrentTransition(const AnimationStateTransition* pTransition) {
    m_pCurrentTransition = pTransition;
    //std::cout << "Transition: " << (pTransition->getFirstNode() ? pTransition->getFirstNode()->getIndex() : -1) << " --> " << pTransition->getSecondNode()->getIndex() << std::endl;
    m_pNextTransition = nullptr;
    m_stateTimers[pTransition->getSecondNode()->getIndex()] = pTransition->getInLocalTimelinePosition();
    m_currentTransitionTimer = 0.0f;
    if (pTransition->hasTriggerFlag()) m_flags[m_transitionFlagIndices[pTransition->getIndex()]] = false;
    if (pTransition->getFirstNode() != m_pCurrentState) {
        // Transitioning from a canceled transition. We must cache the current pose for correct blending
        // Or could be we are transitioning from the "any" state, doesn't matter this still works.
        m_cachedPose = m_pose;
        m_useCachedPose = true;
    } else {
        m_useCachedPose = false;
    }
}

void AnimationInstance::update(float dt) {
    assert(m_pCurrentState);
    if (m_pCurrentState && !m_pCurrentTransition) {
        bool primed = (m_pNextTransition && m_stateTimers[m_pCurrentState->getIndex()] < m_pNextTransition->getOutLocalTimelinePosition());
        m_pose = updateStatePose(m_pCurrentState, dt);
        if (primed && m_stateTimers[m_pCurrentState->getIndex()] >= m_pNextTransition->getOutLocalTimelinePosition()) {
            setCurrentTransition(m_pNextTransition);
            update(0.0f);
        }
    } else if (m_pCurrentTransition) {
        m_currentTransitionTimer += dt;
        if (m_currentTransitionTimer >= m_pCurrentTransition->getDuration()) {
            setCurrentState(m_pCurrentTransition->getSecondNode());
            return update(0.0f);
        }
        if (m_useCachedPose) {
            m_pose = m_cachedPose;
        } else if (m_pCurrentTransition->getOutTransition() == AnimationStateTransition::TRANSITION_ANIMATED) {
            m_pose = updateStatePose(m_pCurrentState, dt);
        } else {
            m_pose = updateStatePose(m_pCurrentState, 0.0f);
        }
        if (m_pCurrentTransition->getInTransition() == AnimationStateTransition::TRANSITION_ANIMATED) {
            m_pose.lerp(updateStatePose(m_pCurrentTransition->getSecondNode(), dt),
                        m_currentTransitionTimer / m_pCurrentTransition->getDuration());
        } else {
            m_pose.lerp(updateStatePose(m_pCurrentTransition->getSecondNode(), 0.0f),
                        m_currentTransitionTimer / m_pCurrentTransition->getDuration());
        }
    }
}

SkeletonPose AnimationInstance::getPose() const {
    return m_pose;
}

float AnimationInstance::getStateTimeScale(size_t index) {
    return m_stateHasTimeScaleParameter[index] ? m_parameters[m_stateTimeScaleParameterIndices[index]] : m_stateTimeScales[index];
}

SkeletonPose AnimationInstance::updateStatePose(const AnimationStateNode* pState, float dt) {
    size_t index = pState->getIndex();

    if (m_stateTimers[index] >= 1.0f && m_pCurrentState->doesLoop()) m_stateTimers[index] -= 1.0f;
    m_stateTimers[index] += dt * getStateTimeScale(index);
    AnimationInstanceParameterSet parameterSet(this, index);
    return pState->getBlendTree()->getPose(m_stateTimers[index], &parameterSet);
}*/
