#include "animation.h"

// This works for negative time values as well! Plays in reverse starting at clip end
static float getLoopTime(float beginTime, float endTime, float time) {
    float duration = endTime - beginTime;
    time = time / duration;
    time = time - std::floor(time);
    time = time * duration;
    return time;
}


AnimationClip::AnimationClip(const SkeletonDescription* pSkeletonDesc, const std::string& name) :
    m_pSkeletonDesc(pSkeletonDesc),
    m_name(name),
    m_channels(m_pSkeletonDesc->getNumJoints())
{
    for (size_t i = 0; i < m_channels.size(); ++i) {
        m_channels[i].joint = static_cast<size_t>(i);
    }
}

AnimationClip::~AnimationClip() {
}

//#include <iostream>
AnimationClip& AnimationClip::addSample(size_t joint, float time, const glm::quat& rotation, const glm::vec3& offset) {
    assert(joint < m_channels.size());

    if (!m_channels[joint].samples.empty()) {
        size_t pos = m_channels[joint].samples.size();
        for (; pos > 0 && m_channels[joint].samples[pos-1].time > time; --pos);
        m_channels[joint].samples.insert(m_channels[joint].samples.begin() + pos, {rotation, offset, time});
    } else {
        m_channels[joint].samples.push_back({rotation, offset, time});
    }
    m_beginTime = std::min(m_beginTime, time);
    m_endTime = std::max(m_endTime, time);

    return *this;
}
SkeletonPose AnimationClip::getPose(float time, bool normalized) const {
    SkeletonPose pose(m_pSkeletonDesc);

    if (normalized) time = m_beginTime + time * getDuration();
    else if (m_isLooping) time = getLoopTime(m_beginTime, m_endTime, time);

    for (size_t i = 0; i < m_channels.size(); ++i) {
        bool done = false;
        for (size_t j = 0; !done && j + 1 < m_channels[i].samples.size(); ++j) {
            if (m_channels[i].samples[j+1].time < time) {
                continue;
            }

            float t = (time - m_channels[i].samples[j].time) / (m_channels[i].samples[j+1].time - m_channels[i].samples[j].time);

            pose.setJointRotation(i, glm::slerp(m_channels[i].samples[j].rotation, m_channels[i].samples[j+1].rotation, t));
            pose.setJointOffset(i, glm::mix(m_channels[i].samples[j].offset, m_channels[i].samples[j+1].offset, t));

            done = true;
        }

        if (!done && !m_channels[i].samples.empty()) {
            pose.setJointRotation(i, m_channels[i].samples.back().rotation);
            pose.setJointOffset(i, m_channels[i].samples.back().offset);
        }
    }

    return pose;
}

/*
Animation::Animation(const SkeletonDescription* pSkeletonDesc, const std::string& name) :
    m_pSkeletonDesc(pSkeletonDesc),
    m_name(name) {
    m_jointSampleLists.resize(pSkeletonDesc->getNumJoints());
}

Animation::~Animation() {}

Animation::JointTransformSample& Animation::createJointTransformSample(size_t jointIndex, float time) {
    JointSampleList& jsl = m_jointSampleLists[jointIndex];

    auto it = jsl.rbegin();
    for(; it != jsl.rend(); ++it) {
        if(it->sampleTime < time) {
            break;
        }
    }

    auto iit = jsl.insert(it.base(), JointTransformSample {time, jointIndex});

    if(time < m_startTime) m_startTime = time;
    if(time > m_endTime) m_endTime = time;

    return *iit;
}

//#include <iostream>
void Animation::addPoseToSkeleton(float time, Skeleton* pSkeleton) {
    if(m_loop) {
        time = getLoopTime(time);
    }

    for(size_t i = 0; i < m_jointSampleLists.size(); ++i) {
        Joint* joint = pSkeleton->getJoint(i);
        JointSampleList& samples = m_jointSampleLists[i];

        for(auto it = samples.begin(); it != samples.end(); ++it) {
            auto nit = std::next(it);
            if(nit == samples.end()) {
                JointTransformSample& jt = *it;

                joint->setLocalPoseOffset(joint->getLocalPoseOffset() + jt.offset);
                joint->setLocalPoseRotation(jt.rotation); // * joint->getLocalPoseRotation());

                //std::cout << "glitched?" << std::endl;
            } else if(nit->sampleTime > time) {

                float mix = (time - it->sampleTime) / (nit->sampleTime - it->sampleTime);
                //if (mix < 0.0f || mix > 1.0f) std::cout << "whoah! " << mix << " " << time << std::endl;
                JointTransformSample &jt0 = *it, &jt1 = *nit;

                glm::vec3 offset = glm::mix(jt0.offset, jt1.offset, mix);
                glm::quat rotation = glm::normalize(glm::slerp(jt0.rotation, jt1.rotation, mix));

                joint->setLocalPoseOffset(joint->getLocalPoseOffset() + offset);
                joint->setLocalPoseRotation(rotation * joint->getLocalPoseRotation());
            } else {
                continue;
            }
            break;
        }
    }
}

SkeletonPose Animation::getPose(float time) {
    if(m_loop) {
        time = getLoopTime(time);
    }

    for(size_t i = 0; i < m_jointSampleLists.size(); ++i) {
        JointSampleList& samples = m_jointSampleLists[i];

        for(auto it = samples.begin(); it != samples.end(); ++it) {
            auto nit = std::next(it);
            if(nit == samples.end()) {
                JointTransformSample& jt = *it;

                joint->setLocalPoseOffset(joint->getLocalPoseOffset() + jt.offset);
                joint->setLocalPoseRotation(jt.rotation); // * joint->getLocalPoseRotation());

                //std::cout << "glitched?" << std::endl;
            } else if(nit->sampleTime > time) {

                float mix = (time - it->sampleTime) / (nit->sampleTime - it->sampleTime);
                //if (mix < 0.0f || mix > 1.0f) std::cout << "whoah! " << mix << " " << time << std::endl;
                JointTransformSample &jt0 = *it, &jt1 = *nit;

                glm::vec3 offset = glm::mix(jt0.offset, jt1.offset, mix);
                glm::quat rotation = glm::normalize(glm::slerp(jt0.rotation, jt1.rotation, mix));

                joint->setLocalPoseOffset(joint->getLocalPoseOffset() + offset);
                joint->setLocalPoseRotation(rotation * joint->getLocalPoseRotation());
            } else {
                continue;
            }
            break;
        }
    }
}*/
