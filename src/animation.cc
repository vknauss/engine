#include "animation.h"

Animation::Animation(const SkeletonDescription* pSkeletonDesc) : m_pSkeletonDesc(pSkeletonDesc) {
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
            } else if(nit->sampleTime > time) {

                float mix = (time - it->sampleTime) / (nit->sampleTime - it->sampleTime);
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
