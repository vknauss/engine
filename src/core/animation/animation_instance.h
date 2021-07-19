#ifndef ANIMATION_INSTANCE_H_INCLUDED
#define ANIMATION_INSTANCE_H_INCLUDED

#include "animation.h"
#include "animation_blend_tree.h"
#include "animation_state_graph.h"

class AnimationInstanceParameterSet {

public:

    AnimationInstanceParameterSet(const AnimationStateGraph* pStateGraph,
                                  const std::vector<float>& parameters,
                                  size_t stateIndex);

    float getParameter(size_t index) const;

private:

    const AnimationStateGraph* m_pStateGraph;

    const std::vector<float>& m_parameters;

    size_t m_stateIndex;

};

// Deprecated
// AnimationSystem handles all this logic internally in batches
/*
class AnimationInstance {

    friend class AnimationInstanceParameterSet;

public:

    AnimationInstance();

    void setStateGraph(const AnimationStateGraph* pStateGraph);

    void triggerFlag(const std::string& name);

    bool getFlag(const std::string& name);

    bool getFlag(size_t index) const;

    size_t getFlagIndex(const std::string& name);

    void setParameter(const std::string& name, float value);

    void setParameter(size_t index, float value);

    float getParameter(const std::string& name);

    float getParameter(size_t index) const;

    float getStateParameter(size_t state, size_t index) const;

    size_t getParameterIndex(const std::string& name);

    void setCurrentState(const AnimationStateNode* pNode);

    void setCurrentTransition(const AnimationStateTransition* pTransition);

    const AnimationStateNode* getCurrentState() const {
        return m_pCurrentState;
    }

    void update(float dt);

    SkeletonPose getPose() const;

private:

    const AnimationStateGraph* m_pStateGraph = nullptr;

    std::vector<float> m_stateTimers;
    std::vector<float> m_stateTimeScales;

    float m_currentTransitionTimer;

    std::vector<bool> m_flags;
    std::map<std::string, size_t> m_flagIndexMap;

    std::vector<bool> m_transitionHasFlag;
    std::vector<size_t> m_transitionFlagIndices;

    std::vector<float> m_parameters;
    std::map<std::string, size_t> m_parameterIndexMap;

    std::vector<std::vector<size_t>> m_stateParameterIndices;

    std::vector<bool> m_stateHasTimeScaleParameter;
    std::vector<size_t> m_stateTimeScaleParameterIndices;

    const AnimationStateNode* m_pCurrentState = nullptr;
    const AnimationStateTransition* m_pCurrentTransition = nullptr;
    const AnimationStateTransition* m_pNextTransition = nullptr;

    bool m_useCachedPose = false;
    SkeletonPose m_cachedPose;

    SkeletonPose m_pose;


    float getStateTimeScale(size_t index);

    SkeletonPose updateStatePose(const AnimationStateNode* pState, float dt);

};*/


#endif // ANIMATION_INSTANCE_H_INCLUDED
