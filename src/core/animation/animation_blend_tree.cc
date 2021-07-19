#include "animation_blend_tree.h"

#include <algorithm>
#include <iostream>
#include <numeric>

#include "animation_instance.h"
#include "core/util/math_util.h"

AnimationBlendTree::AnimationBlendTree() :
    m_pRootNode(nullptr),
    m_numParameters(0),
    m_numClips(0),
    m_pose(nullptr) {
}

AnimationBlendTree::~AnimationBlendTree() {
    for (AnimationBlendNode* pNode : m_pNodes) {
        if (pNode) delete pNode;
    }
}

void AnimationBlendTree::setRootNode(AnimationBlendNode* pNode) {
    m_pRootNode = pNode;
}

uint32_t AnimationBlendTree::getParameterIndex(const std::string& name) {
    if (auto it = m_parameterIndexMap.find(name); it != m_parameterIndexMap.end()) {
        return it->second;
    } else {
        m_parameterIndexMap.insert({name, m_numParameters});
        return m_numParameters++;
    }
}

uint32_t AnimationBlendTree::getClipIndex(const std::string& name) {
    if (auto it = m_clipIndexMap.find(name); it != m_clipIndexMap.end()) {
        return it->second;
    } else {
        m_clipIndexMap.insert({name, m_numClips});
        return m_numClips++;
    }
}

// Call this one each frame
SkeletonPose AnimationBlendTree::getPose(float time,
                         const AnimationInstanceParameterSet* pParameters,
                         const std::vector<uint32_t>& clipIndices,
                         const std::vector<const AnimationClip*> pClipsIn,
                         bool absoluteTime) const {
    std::vector<const AnimationClip*> pClips;
    std::vector<float> weights;

    m_pRootNode->flattenRecursive(pClips, weights, 1.0f, pParameters, clipIndices, pClipsIn);

    if (pClips.empty()) return SkeletonPose(nullptr);

    SkeletonPose pose = pClips[0]->getPose(!absoluteTime ? time : time / (pClips[0]->getDuration()));
    float sumWeight = weights[0];

    for (size_t i = 1; i < pClips.size(); ++i) {
        pose.lerp(pClips[i]->getPose(time), 1.0f - (sumWeight / (sumWeight + weights[i])));
        sumWeight += weights[i];
    }

    return pose;
}

// Don't forget to call this after the tree is built
// It'll be weird otherwise
void AnimationBlendTree::finalize() {
    for (AnimationBlendNode* pNode : m_pNodes) pNode->finalize();
}


void AnimationBlendNodeFactor1f::setFactor(float factor) {
    m_factor = factor;
    m_useParameter = false;
}

void AnimationBlendNodeFactor1f::setFactorParameter(const std::string& name) {
    m_parameterIndex = m_pParentTree->getParameterIndex(name);
    m_useParameter = true;
}

float AnimationBlendNodeFactor1f::getFactor(const AnimationInstanceParameterSet* pParameters) const {
    return m_useParameter ? pParameters->getParameter(m_parameterIndex) : m_factor;
}


void AnimationBlendNodeFactor2f::setFactor(const glm::vec2& factor) {
    m_factorX = factor.x;
    m_factorY = factor.y;

    m_useParameterX = false;
    m_useParameterY = false;
}

void AnimationBlendNodeFactor2f::setFactor(float x, float y) {
    m_factorX = x;
    m_factorY = y;

    m_useParameterX = false;
    m_useParameterY = false;
}

void AnimationBlendNodeFactor2f::setFactorX(float x) {
    m_factorX = x;

    m_useParameterX = false;
}

void AnimationBlendNodeFactor2f::setFactorY(float y) {
    m_factorY = y;

    m_useParameterY = false;
}

void AnimationBlendNodeFactor2f::setFactorParameterX(const std::string& name) {
    m_parameterXIndex = m_pParentTree->getParameterIndex(name);
    m_useParameterX = true;
}

void AnimationBlendNodeFactor2f::setFactorParameterY(const std::string& name) {
    m_parameterYIndex = m_pParentTree->getParameterIndex(name);
    m_useParameterY = true;
}

float AnimationBlendNodeFactor2f::getFactorX(const AnimationInstanceParameterSet* pParameters) const {
    return m_useParameterX ? pParameters->getParameter(m_parameterXIndex) : m_factorX;
}

float AnimationBlendNodeFactor2f::getFactorY(const AnimationInstanceParameterSet* pParameters) const {
    return m_useParameterY ? pParameters->getParameter(m_parameterYIndex) : m_factorY;
}

glm::vec2 AnimationBlendNodeFactor2f::getFactor(const AnimationInstanceParameterSet* pParameters) const {
    return glm::vec2(getFactorX(pParameters), getFactorY(pParameters));
}


/*void AnimationBlendNodeSingleClip::setClip(const AnimationClip* pClip) {
    m_pClip = pClip;
}*/

void AnimationBlendNodeSingleClip::setClipName(const std::string& name) {
    m_clipName = name;
    m_clipIndex = m_pParentTree->getClipIndex(name);
}

void AnimationBlendNodeSingleClip::flattenRecursive(std::vector<const AnimationClip*>& pClips,
                                                    std::vector<float>& weights,
                                                    float weight,
                                                    const AnimationInstanceParameterSet* pParameters,
                                                    const std::vector<uint32_t> clipIndices,
                                                    const std::vector<const AnimationClip*>& pClipsIn) const {
    const AnimationClip* pClip = pClipsIn[clipIndices[m_clipIndex]];
    if (pClip) {
        pClips.push_back(pClip);
        weights.push_back(weight);
    }
}


AnimationBlendNodeLerp& AnimationBlendNodeLerp::setFirstInput(const AnimationBlendNode* pBlendNode) {
    m_pInputFirst = pBlendNode;
    return *this;
}

AnimationBlendNodeLerp& AnimationBlendNodeLerp::setSecondInput(const AnimationBlendNode* pBlendNode) {
    m_pInputSecond = pBlendNode;
    return *this;
}

void AnimationBlendNodeLerp::flattenRecursive(std::vector<const AnimationClip*>& pClips,
                                              std::vector<float>& weights,
                                              float weight,
                                              const AnimationInstanceParameterSet* pParameters,
                                              const std::vector<uint32_t> clipIndices,
                                              const std::vector<const AnimationClip*>& pClipsIn) const {
    assert(m_pInputFirst && m_pInputSecond);

    float factor = getFactor(pParameters);
    if (factor <= 0.0f) {
        m_pInputFirst->flattenRecursive(pClips, weights, weight, pParameters, clipIndices, pClipsIn);
    } else if (factor >= 1.0f) {
        m_pInputSecond->flattenRecursive(pClips, weights, weight, pParameters, clipIndices, pClipsIn);
    } else {
        m_pInputFirst->flattenRecursive(pClips, weights, weight * (1.0f - factor), pParameters, clipIndices, pClipsIn);
        m_pInputSecond->flattenRecursive(pClips, weights, weight * factor, pParameters, clipIndices, pClipsIn);
    }
}


AnimationBlendNode1D& AnimationBlendNode1D::addChild(AnimationBlendNode* pNode, float position) {
    assert(pNode);

    m_pChildren.push_back(pNode);
    m_childPositions.push_back(position);

    return *this;
}

void AnimationBlendNode1D::flattenRecursive(std::vector<const AnimationClip*>& pClips,
                                            std::vector<float>& weights,
                                            float weight,
                                            const AnimationInstanceParameterSet* pParameters,
                                            const std::vector<uint32_t> clipIndices,
                                            const std::vector<const AnimationClip*>& pClipsIn) const {
    assert(m_pChildren.size() > 0);


    if (m_pChildren.size() == 1) {
        m_pChildren[0]->flattenRecursive(pClips, weights, weight, pParameters, clipIndices, pClipsIn);
    } else {
        AnimationBlendNode* pFirst = nullptr;
        AnimationBlendNode* pSecond = nullptr;
        float posFirst = 0.0f, posSecond = 0.0f;

        float factor = getFactor(pParameters);

        for (size_t i = 0; i < m_pChildren.size(); ++i) {

            if (m_childPositions[i] >= factor) {
                pSecond = m_pChildren[i];
                posSecond = m_childPositions[i];
                break;
            }

            pFirst = m_pChildren[i];
            posFirst = m_childPositions[i];
        }

        if (!pSecond) {
            assert(pFirst);
            pFirst->flattenRecursive(pClips, weights, weight, pParameters, clipIndices, pClipsIn);
        } else if (!pFirst || factor == posSecond) {
            assert(pSecond);
            pSecond->flattenRecursive(pClips, weights, weight, pParameters, clipIndices, pClipsIn);
        } else {
            assert(pFirst && pSecond);
            float blend = (factor - posFirst) / (posSecond - posFirst);
            pFirst->flattenRecursive(pClips, weights, weight * (1.0f - blend), pParameters, clipIndices, pClipsIn);
            pSecond->flattenRecursive(pClips, weights, weight * blend, pParameters, clipIndices, pClipsIn);
        }
    }
}

// Sort the children in ascending order by 1D position value
void AnimationBlendNode1D::finalize() {
    std::vector<size_t> inds(m_pChildren.size());
    std::iota(inds.begin(), inds.end(), 0);
    std::sort(inds.begin(), inds.end(), [this] (size_t i0, size_t i1) { return m_childPositions[i0] < m_childPositions[i1]; });
    std::transform(inds.begin(), inds.end(), m_pChildren.begin(), [*this] (size_t i) { return m_pChildren[i]; });
    std::transform(inds.begin(), inds.end(), m_childPositions.begin(), [*this] (size_t i) { return m_childPositions[i]; });
}


AnimationBlendNode2D& AnimationBlendNode2D::addChild(AnimationBlendNode* pNode, glm::vec2 position) {
    assert(pNode);

    m_pChildren.push_back(pNode);
    m_childPositions.push_back(position);

    return *this;
}

void AnimationBlendNode2D::flattenRecursive(std::vector<const AnimationClip*>& pClips,
                                            std::vector<float>& weights,
                                            float weight,
                                            const AnimationInstanceParameterSet* pParameters,
                                            const std::vector<uint32_t> clipIndices,
                                            const std::vector<const AnimationClip*>& pClipsIn) const {
    assert(m_pChildren.size() > 0);

    glm::vec2 factor = getFactor(pParameters);

    if (m_pChildren.size() == 1) {
        m_pChildren[0]->flattenRecursive(pClips, weights, weight, pParameters, clipIndices, pClipsIn);
    } else if (m_pChildren.size() == 2) {  // Degenerate case, treat as a LERP along the line segment
        glm::vec2 e = m_childPositions[1] - m_childPositions[0];
        glm::vec2 f = factor - m_childPositions[0];
        float blend = glm::dot(e, f) / glm::dot(e, e);
        if (blend <= 0.0f) {
            m_pChildren[0]->flattenRecursive(pClips, weights, weight, pParameters, clipIndices, pClipsIn);
        } else if (blend >= 1.0f) {
            m_pChildren[1]->flattenRecursive(pClips, weights, weight, pParameters, clipIndices, pClipsIn);
        } else {
            m_pChildren[0]->flattenRecursive(pClips, weights, weight * (1.0f - blend), pParameters, clipIndices, pClipsIn);
            m_pChildren[1]->flattenRecursive(pClips, weights, weight * blend, pParameters, clipIndices, pClipsIn);
        }
    } else {
        // Use barycentric coordinates to blend

        for (glm::uvec3 tri : m_triangles) {
            // From wikipedia: Barycentric coordinate system
            // https://en.wikipedia.org/wiki/Barycentric_coordinate_system#Barycentric_coordinates_on_triangles
            glm::vec2 p0 = m_childPositions[tri[0]];
            glm::vec2 p1 = m_childPositions[tri[1]];
            glm::vec2 p2 = m_childPositions[tri[2]];

            float denom = (p1.y-p2.y)*(p0.x-p2.x) + (p2.x-p1.x)*(p0.y-p2.y);

            float coord[3];
            coord[0] = ((p1.y-p2.y)*(factor.x-p2.x) + (p2.x-p1.x)*(factor.y-p2.y)) / denom;
            coord[1] = ((p2.y-p0.y)*(factor.x-p2.x) + (p0.x-p2.x)*(factor.y-p2.y)) / denom;
            coord[2] = 1 - coord[0] - coord[1];

            if (std::min({coord[0], coord[1], coord[2]}) < -0.001f) continue;

            bool skip[3] = {false};
            float rw = coord[0] + coord[1] + coord[2];

            for (auto i = 0; i < 3; ++i) {
                if (coord[i] < 0.001f) {
                    skip[i] = true;
                    rw -= coord[i];
                }
            }

            for (auto i = 0; i < 3; ++i) {
                if (!skip[i]) {
                    assert(m_pChildren[tri[i]]);
                    m_pChildren[tri[i]]->flattenRecursive(pClips, weights, weight*coord[i]/rw, pParameters, clipIndices, pClipsIn);
                }
            }
            break;
        }
    }
}

void AnimationBlendNode2D::finalize() {
    m_triangles = math_util::delaunayTriangulation(m_childPositions);
}
