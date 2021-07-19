#ifndef ANIMATION_STATE_GRAPH_H_INCLUDED
#define ANIMATION_STATE_GRAPH_H_INCLUDED

#include "animation_blend_tree.h"

// Really there are 3 classes defined in this file so this should probably get split up
// Contains definitions for: (searchable!)
//  - AnimationStateNode
//  - AnimationStateTransition
//  - AnimationStateGraph

class AnimationStateTransition;

// Animation State
// Holds a pointer to one Blend Tree and some number
// of Animation State Transitions
// Not directly instantiable, use AnimationStateGraph::createNode() to create
class AnimationStateNode {

    friend class AnimationStateGraph;

public:

    void setBlendTree(AnimationBlendTree* pBlendTree);

    void addTransition(const AnimationStateTransition* pTransition);

    void setTimeScale(float timeScale);

    void setTimeScaleParameter(const std::string& name);

    void setName(const std::string& name);

    void setLoop(bool loop);

    void setTimeScaleAbsolute(bool value);

    size_t getIndex() const {
        return m_index;
    }

    AnimationBlendTree* getBlendTree() const {
        return m_pBlendTree;
    }

    std::vector<const AnimationStateTransition*> getOutTransitions() const {
        return m_pOutTransitions;
    }

    float getTimeScale() const {
        return m_timeScale;
    }

    bool hasTimeScaleParameter() const {
        return m_useTimeScaleParameter;
    }

    std::string getTimeScaleParameterName() const {
        return m_timeScaleParameterName;
    }

    std::string getName() const {
        return m_name;
    }

    bool doesLoop() const {
        return m_loop;
    }

    bool isTimeScaleAbsolute() const {
        return m_timeScaleAbsolute;
    }

private:

    AnimationStateNode(size_t index);

    AnimationBlendTree* m_pBlendTree = nullptr;

    std::vector<const AnimationStateTransition*> m_pOutTransitions;

    size_t m_index;

    std::string m_name;

    std::string m_timeScaleParameterName;
    bool m_useTimeScaleParameter;

    float m_timeScale;

    bool m_loop;

    bool m_timeScaleAbsolute = false;

};

// Animation State Transition
// Holds pointers to the two states it transitions between
// The first pointer may be null if the transition is from
// 'any' state
// Also holds data about how the transition should be evaluated
// Not directly instantiable, use AnimationStateGraph::createTransition()
class AnimationStateTransition {

    friend class AnimationStateGraph;

public:

    enum TransitionType { TRANSITION_FROZEN, TRANSITION_ANIMATED };
    enum ConditionType { CONDITION_END_OF_CLIP, CONDITION_TRIGGER };

    void setFirstNode(AnimationStateNode* pNode);

    void setSecondNode(AnimationStateNode* pNode);

    void setOutTransitionType(TransitionType type);

    void setInTransitionType(TransitionType type);

    void setOutLocalTimelinePosition(float pos);

    void setInLocalTimelinePosition(float pos);

    void setDuration(float duration);

    void setTriggerFlag(const std::string& flagName);

    void setWaitForOutTime(bool value);

    void setCancellable(bool value);

    AnimationStateNode* getFirstNode() const {
        return m_pFirst;
    }

    AnimationStateNode* getSecondNode() const {
        return m_pSecond;
    }

    TransitionType getOutTransition() const {
        return m_outTransition;
    }

    TransitionType getInTransition() const {
        return m_inTransition;
    }

    float getOutLocalTimelinePosition() const {
        return m_outLocalTimelinePos;
    }

    float getInLocalTimelinePosition() const {
        return m_inLocalTimelinePos;
    }

    float getDuration() const {
        return m_duration;
    }

    bool hasTriggerFlag() const {
        return m_flagSet;
    }

    std::string getTriggerFlagName() const {
        return m_flagName;
    }

    size_t getIndex() const {
        return m_index;
    }

    ConditionType getCondition() const {
        return m_condition;
    }

    bool waitForOutTime() const {
        return m_wait;
    }

    bool isCancellable() const {
        return m_cancellable;
    }

private:

    AnimationStateTransition(size_t index);

    AnimationStateNode* m_pFirst = nullptr;
    AnimationStateNode* m_pSecond = nullptr;

    TransitionType m_outTransition = TRANSITION_FROZEN;
    TransitionType m_inTransition = TRANSITION_ANIMATED;

    ConditionType m_condition = CONDITION_END_OF_CLIP;

    std::string m_flagName;
    bool m_flagSet = false;

    bool m_wait = true;
    bool m_cancellable = false;

    float m_duration = 0.0f;

    float m_outLocalTimelinePos = 1.0f;
    float m_inLocalTimelinePos = 0.0f;

    size_t m_index;

};

// Animation State Graph
// Provides a layout for a State Machine based on Animation State Nodes
// An owning container for Animation States and Animation State Transitions
// Does not include any state, so is suitable to be assigned as the State Graph
// For multiple Animation Instances
class AnimationStateGraph {

public:

    ~AnimationStateGraph();

    AnimationStateNode* createNode();

    AnimationStateTransition* createTransition(AnimationStateNode* pNodeFirst,
                                               AnimationStateNode* pNodeSecond);

    void setInitialState(const AnimationStateNode* pState);

    // Call this after creating all State Nodes and Transitions
    // to build the parameter/flag index data
    void finalize();

    void setName(const std::string& name) {
        m_name = name;
    }

    const std::string& getName() const {
        return m_name;
    }

    const AnimationStateNode* getInitialState() const {
        return m_pInitialState;
    }

    const std::vector<AnimationStateNode*>& getStates() const {
        return m_pStateNodes;
    }

    const std::vector<AnimationStateTransition*>& getTransitions() const {
        return m_pTransitions;
    }

    const std::map<std::string, uint32_t>& getParameterIndexMap() const {
        return m_parameterIndexMap;
    }

    const std::map<std::string, uint32_t>& getFlagIndexMap() const {
        return m_flagIndexMap;
    }

    const std::map<std::string, uint32_t>& getClipIndexMap() const {
        return m_clipIndexMap;
    }

    const std::vector<uint32_t>& getStateClipIndices(uint32_t stateIndex) const {
        return m_stateClipIndices[stateIndex];
    }

    uint32_t getParameterIndex(const std::string& pName) const {
        return m_parameterIndexMap.at(pName);
    }

    uint32_t getFlagIndex(const std::string& fName) const {
        return m_flagIndexMap.at(fName);
    }

    // Safer versions of above, intended to be used when you don't know if a parameter exists
    // and/or when users may specify parameters
    bool getParameterIndexOpt(const std::string& pName, uint32_t& index) const {
        if (auto it = m_parameterIndexMap.find(pName); it != m_parameterIndexMap.end()) {
            index = it->second;
            return true;
        }
        return false;
    }

    bool getFlagIndexOpt(const std::string& fName, uint32_t& index) const {
        if (auto it = m_flagIndexMap.find(fName); it != m_flagIndexMap.end()) {
            index = it->second;
            return true;
        }
        return false;
    }

    uint32_t getStateParameterIndex(uint32_t stateIndex,
                                    uint32_t parameterIndex) const {
        return m_stateParameterIndices[stateIndex][parameterIndex];
    }

    uint32_t getStateTimeScaleParameterIndex(uint32_t stateIndex) const {
        return m_stateTimeScaleParameterIndices[stateIndex];
    }

    uint32_t getTransitionFlagIndex(uint32_t transitionIndex) const {
        return m_transitionFlagIndices[transitionIndex];
    }

    uint32_t getStateClipIndex(uint32_t stateIndex,
                               uint32_t clipIndex) const {
        return m_stateClipIndices[stateIndex][clipIndex];
    }

    uint32_t getNumParameters() const {
        return m_numParameters;
    }

    uint32_t getNumFlags() const {
        return m_numFlags;
    }

    uint32_t getNumClips() const {
        return m_numClips;
    }

private:

    const AnimationStateNode* m_pInitialState = nullptr;

    std::vector<AnimationStateNode*> m_pStateNodes;
    std::vector<AnimationStateTransition*> m_pTransitions;

    std::map<std::string, uint32_t> m_parameterIndexMap;
    std::vector<std::vector<uint32_t>> m_stateParameterIndices;
    std::vector<uint32_t> m_stateTimeScaleParameterIndices;

    std::map<std::string, uint32_t> m_flagIndexMap;
    std::vector<uint32_t> m_transitionFlagIndices;

    std::map<std::string, uint32_t> m_clipIndexMap;
    std::vector<std::vector<uint32_t>> m_stateClipIndices;

    std::string m_name;

    uint32_t m_numParameters = 0;
    uint32_t m_numFlags = 0;
    uint32_t m_numClips = 0;

    uint32_t getOrAddParameter(const std::string& pName);
    uint32_t getOrAddFlag(const std::string& fName);
    uint32_t getOrAddClip(const std::string& clipName);

};

#endif // ANIMATION_STATE_GRAPH_H_INCLUDED
