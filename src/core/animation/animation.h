#ifndef ANIMATION_H_
#define ANIMATION_H_

#include <string>

#include "skeleton.h"


class AnimationClip {

public:

    struct Sample {
        glm::quat rotation;
        glm::vec3 offset;
        float time;
    };

    struct Channel {
        std::vector<Sample> samples;

        size_t joint;
    };

    AnimationClip(const SkeletonDescription* pSkeletonDesc, const std::string& name);

    ~AnimationClip();

    AnimationClip& addSample(size_t joint, float time, const glm::quat& rotation, const glm::vec3& position);

    SkeletonPose getPose(float time, bool normalized = true) const;

    AnimationClip& setLooping(bool isLooping) {
        m_isLooping = isLooping;

        return *this;
    }

    const SkeletonDescription* getSkeletonDescription() const {
        return m_pSkeletonDesc;
    }

    const std::string& getName() const {
        return m_name;
    }

    float getBeginTime() const {
        return m_beginTime;
    }

    float getEndTime() const {
        return m_endTime;
    }

    float getDuration() const {
        return m_endTime - m_beginTime;
    }

    bool isLooping() const {
        return m_isLooping;
    }


private:

    const SkeletonDescription* m_pSkeletonDesc;

    std::string m_name;

    std::vector<Channel> m_channels;

    float m_beginTime = std::numeric_limits<float>::max();
    float m_endTime = std::numeric_limits<float>::min();

    bool m_isLooping = true;

};



/*class Animation {

public:

    struct JointTransformSample {
        float sampleTime;
        size_t jointIndex;
        glm::vec3 offset = {0, 0, 0};
        glm::quat rotation = {1, 0, 0, 0};
    };

    typedef std::list<JointTransformSample> JointSampleList;

    Animation(const SkeletonDescription* pSkeletonDesc, const std::string& name);

    ~Animation();

    // I don't love exposing this internal struct.. Maybe I should just have the caller pass in the parameters
    JointTransformSample& createJointTransformSample(size_t jointIndex, float time);

    void addPoseToSkeleton(float time, Skeleton* pSkeleton);

    SkeletonPose getPose(float time);

    const SkeletonDescription* getSkeletonDescription() const {
        return m_pSkeletonDesc;
    }

    bool isLooping() const {
        return m_loop;
    }

    void setLoop(bool loop) {
        m_loop = loop;
    }

    std::string getName() const {
        return m_name;
    }

private:

    const SkeletonDescription* const m_pSkeletonDesc;

    std::vector<JointSampleList> m_jointSampleLists;
    std::vector<float> m_poseTimes;

    std::string m_name;

    float m_startTime = std::numeric_limits<float>::max();
    float m_endTime = std::numeric_limits<float>::min();

    bool m_loop = true;



};*/

#endif // ANIMATION_H_
