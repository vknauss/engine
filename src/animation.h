#ifndef ANIMATION_H_
#define ANIMATION_H_

#include <list>

#include "skeleton.h"

// TODO: support for blending animations / additive or difference animations

class Animation {

public:

    struct JointTransformSample {
        float sampleTime;
        size_t jointIndex;
        glm::vec3 offset = {0, 0, 0};
        glm::quat rotation = {1, 0, 0, 0};
    };

    typedef std::list<JointTransformSample> JointSampleList;

    Animation(const SkeletonDescription* pSkeletonDesc);

    ~Animation();

    // I don't love exposing this internal struct.. Maybe I should just have the caller pass in the parameters
    JointTransformSample& createJointTransformSample(size_t jointIndex, float time);

    void addPoseToSkeleton(float time, Skeleton* pSkeleton);

    const SkeletonDescription* getSkeletonDescription() const {
        return m_pSkeletonDesc;
    }

    bool isLooping() const {
        return m_loop;
    }

    void setLoop(bool loop) {
        m_loop = loop;
    }

private:

    const SkeletonDescription* const m_pSkeletonDesc;

    std::vector<JointSampleList> m_jointSampleLists;
    std::vector<float> m_poseTimes;

    float m_startTime = std::numeric_limits<float>::max();
    float m_endTime = std::numeric_limits<float>::min();

    bool m_loop = true;

    // This works for negative time values as well! Plays in reverse starting at clip end
    float getLoopTime(float time) const {
        float duration = m_endTime - m_startTime;
        time = time / duration;
        time = time - std::floor(time);
        time = time * duration;
        return time;
    }

};

#endif // ANIMATION_H_
