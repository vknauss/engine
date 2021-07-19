#include "core/animation/animation_state_graph.h"

#include <iostream>

void AnimationStateNode::setBlendTree(AnimationBlendTree* pBlendTree) {
    m_pBlendTree = pBlendTree;
}

void AnimationStateNode::addTransition(const AnimationStateTransition* pTransition) {
    m_pOutTransitions.push_back(pTransition);
}

void AnimationStateNode::setTimeScale(float timeScale) {
    m_timeScale = timeScale;
    m_useTimeScaleParameter = false;
}

void AnimationStateNode::setTimeScaleParameter(const std::string& name) {
    m_timeScaleParameterName = name;
    m_useTimeScaleParameter = true;
}

void AnimationStateNode::setName(const std::string& name) {
    m_name = name;
}

void AnimationStateNode::setLoop(bool loop) {
    m_loop = loop;
}

void AnimationStateNode::setTimeScaleAbsolute(bool value) {
    m_timeScaleAbsolute = value;
}

AnimationStateNode::AnimationStateNode(size_t index) :
    m_index(index),
    m_timeScaleParameterName(),
    m_useTimeScaleParameter(false),
    m_timeScale(1.0f),
    m_loop(true),
    m_timeScaleAbsolute(false) {
}


void AnimationStateTransition::setFirstNode(AnimationStateNode* pNode) {
    m_pFirst = pNode;
}

void AnimationStateTransition::setSecondNode(AnimationStateNode* pNode) {
    m_pSecond = pNode;
}

void AnimationStateTransition::setOutTransitionType(TransitionType type) {
    m_outTransition = type;
}

void AnimationStateTransition::setInTransitionType(TransitionType type) {
    m_inTransition = type;
}

void AnimationStateTransition::setOutLocalTimelinePosition(float pos) {
    m_outLocalTimelinePos = pos;
}

void AnimationStateTransition::setInLocalTimelinePosition(float pos) {
    m_inLocalTimelinePos = pos;
}

void AnimationStateTransition::setDuration(float duration) {
    m_duration = duration;
}

void AnimationStateTransition::setTriggerFlag(const std::string& flagName) {
    m_flagName = flagName;
    m_flagSet = true;
    m_condition = CONDITION_TRIGGER;
}

void AnimationStateTransition::setWaitForOutTime(bool value) {
    m_wait = value;
}

void AnimationStateTransition::setCancellable(bool value) {
    m_cancellable = value;
}

AnimationStateTransition::AnimationStateTransition(size_t index) :
    m_index(index) {
}


AnimationStateGraph::~AnimationStateGraph() {
    for (AnimationStateNode* pNode : m_pStateNodes) if (pNode) delete pNode;
    for (AnimationStateTransition* pTransition : m_pTransitions) if (pTransition) delete pTransition;
}

AnimationStateNode* AnimationStateGraph::createNode() {
    AnimationStateNode* pNode = new AnimationStateNode(m_pStateNodes.size());
    m_pStateNodes.push_back(pNode);
    return pNode;
}

AnimationStateTransition* AnimationStateGraph::createTransition(AnimationStateNode* pNodeFirst, AnimationStateNode* pNodeSecond) {
    AnimationStateTransition* pTransition = new AnimationStateTransition(m_pTransitions.size());
    m_pTransitions.push_back(pTransition);
    pTransition->setFirstNode(pNodeFirst);
    pTransition->setSecondNode(pNodeSecond);
    if (pNodeFirst == nullptr) {
        // transition from any state except from the second state
        for (AnimationStateNode* pNode : m_pStateNodes)
            if (pNode != pNodeSecond) pNode->addTransition(pTransition);
    } else {
        pNodeFirst->addTransition(pTransition);
    }

    return pTransition;
}

void AnimationStateGraph::setInitialState(const AnimationStateNode* pState) {
    m_pInitialState = pState;
}

void AnimationStateGraph::finalize() {
    m_transitionFlagIndices.resize(m_pTransitions.size(), 0);
    for (auto i = 0u; i < m_pTransitions.size(); ++i) {
        if (m_pTransitions[i]->hasTriggerFlag())
            m_transitionFlagIndices[i] = getOrAddFlag(m_pTransitions[i]->getTriggerFlagName());
    }

    m_stateParameterIndices.resize(m_pStateNodes.size());
    m_stateTimeScaleParameterIndices.resize(m_pStateNodes.size());
    m_stateClipIndices.resize(m_pStateNodes.size());

    for (auto i = 0u; i < m_pStateNodes.size(); ++i) {
        AnimationStateNode* pState = m_pStateNodes[i];

        const auto& indexMap = pState->getBlendTree()->getParameterIndexMap();
        m_stateParameterIndices[i].resize(indexMap.size());
        for (auto kv : indexMap) {
            m_stateParameterIndices[i][kv.second] = getOrAddParameter(kv.first);
        }

        const auto& clipIndexMap = pState->getBlendTree()->getClipIndexMap();
        m_stateClipIndices[i].resize(clipIndexMap.size());
        for (auto kv : clipIndexMap) {
            m_stateClipIndices[i][kv.second] = getOrAddClip(kv.first);
        }

        if (pState->hasTimeScaleParameter())
            m_stateTimeScaleParameterIndices[i] = getOrAddParameter(pState->getTimeScaleParameterName());
    }

    if (!m_pInitialState)
        m_pInitialState = m_pStateNodes[0];

}

uint32_t AnimationStateGraph::getOrAddParameter(const std::string& pName) {
    if (auto it = m_parameterIndexMap.find(pName); it != m_parameterIndexMap.end()) {
        return it->second;
    } else {
        m_parameterIndexMap.insert({pName, m_numParameters});
        return m_numParameters++;
    }
}

uint32_t AnimationStateGraph::getOrAddFlag(const std::string& fName) {
    if (auto it = m_flagIndexMap.find(fName); it != m_flagIndexMap.end()) {
        return it->second;
    } else {
        m_flagIndexMap.insert({fName, m_numFlags});
        return m_numFlags++;
    }
}

uint32_t AnimationStateGraph::getOrAddClip(const std::string& clipName) {
    if (auto it = m_clipIndexMap.find(clipName); it != m_clipIndexMap.end()) {
        return it->second;
    } else {
        m_clipIndexMap.insert({clipName, m_numClips});
        return m_numClips++;
    }
}
