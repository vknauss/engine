#ifndef ANIMATION_BLEND_TREE_H_INCLUDED
#define ANIMATION_BLEND_TREE_H_INCLUDED

#include "animation.h"
#include "skeleton.h"

class AnimationInstanceParameterSet;

class AnimationBlendTree;

class AnimationBlendNode {

friend class AnimationBlendTree;

public:

    virtual ~AnimationBlendNode() {}

    // Build the flat list of clips and weights that will be used for the final blending operation.
    // Implementations are required to call this method on their children recursively,
    //   as the system does not know the precise logic of how each node type distributes its weight.
    // Implementations must ensure the weights passed into any child nodes add up to the weight parameter
    //   to ensure the final blend behaves as expected.
    virtual void flattenRecursive(std::vector<const AnimationClip*>& pClips,
                                  std::vector<float>& weights,
                                  float weight,
                                  const AnimationInstanceParameterSet* pParameters,
                                  const std::vector<uint32_t> clipIndices,
                                  const std::vector<const AnimationClip*>& pClipsIn) const = 0;

    // This hook is provided so that node types may do some auxiliary computations and safely assume any user-side
    //   modifications to the node hierarchy are complete, to save from doing expensive calculations each frame.
    // Example usages may be sorting a list of child nodes, building spatial data structures, etc.
    // This will be called automatically by the owning AnimationBlendTree for each node.
    virtual void finalize() {}

protected:

    AnimationBlendTree* m_pParentTree = nullptr;

private:

    void setParentTree(AnimationBlendTree* pTree) {
        m_pParentTree = pTree;
    }

};

// Blend tree
class AnimationBlendTree {

public:

    AnimationBlendTree();

    ~AnimationBlendTree();

    template<typename T>
    T* createNode();

    void setRootNode(AnimationBlendNode* pNode);

    const AnimationBlendNode* getRootNode() const {
        return m_pRootNode;
    }

    uint32_t getParameterIndex(const std::string& name);

    uint32_t getClipIndex(const std::string& name);

    const std::map<std::string, uint32_t>& getParameterIndexMap() const {
        return m_parameterIndexMap;
    }

    const std::map<std::string, uint32_t>& getClipIndexMap() const {
        return m_clipIndexMap;
    }

    uint32_t getNumParameters() const {
        return m_numParameters;
    }

    uint32_t getNumClips() const {
        return m_numClips;
    }

    // Call this one each frame
    SkeletonPose getPose(float time,
                         const AnimationInstanceParameterSet* pParameters,
                         const std::vector<uint32_t>& clipIndices,
                         const std::vector<const AnimationClip*> pClipsIn,
                         bool absoluteTime = false) const;

    // Don't forget to call this after the tree is built
    // It'll be weird otherwise
    void finalize();


private:

    std::vector<AnimationBlendNode*> m_pNodes;
    AnimationBlendNode* m_pRootNode;

    std::map<std::string, uint32_t> m_parameterIndexMap;

    std::map<std::string, uint32_t> m_clipIndexMap;

    uint32_t m_numParameters;
    uint32_t m_numClips;

    SkeletonPose m_pose;

};

// Base class for nodes with a 1-D float blend factor
class AnimationBlendNodeFactor1f : public AnimationBlendNode {

public:

    void setFactor(float factor);

    void setFactorParameter(const std::string& name);

    float getFactor(const AnimationInstanceParameterSet* pParameters) const;

private:

    float m_factor = 0.0f;
    size_t m_parameterIndex = 0;
    bool m_useParameter = false;

};

// Base class for nodes with a 2-D float blend factor
class AnimationBlendNodeFactor2f : public AnimationBlendNode {

public:

    void setFactor(const glm::vec2& factor);

    void setFactor(float x, float y);

    void setFactorX(float x);

    void setFactorY(float y);

    void setFactorParameterX(const std::string& name);

    void setFactorParameterY(const std::string& name);

    float getFactorX(const AnimationInstanceParameterSet* pParameters) const;

    float getFactorY(const AnimationInstanceParameterSet* pParameters) const;

    glm::vec2 getFactor(const AnimationInstanceParameterSet* pParameters) const;

private:

    float m_factorX = 0.0f;
    float m_factorY = 0.0f;

    size_t m_parameterXIndex = 0;
    size_t m_parameterYIndex = 0;

    bool m_useParameterX = false;
    bool m_useParameterY = false;

};

// Single clip leaf node
// Currently the only type of leaf node
class AnimationBlendNodeSingleClip : public AnimationBlendNode {

public:

    //void setClip(const AnimationClip* pClip);
    void setClipName(const std::string& name);

    void flattenRecursive(std::vector<const AnimationClip*>& pClips,
                          std::vector<float>& weights,
                          float weight,
                          const AnimationInstanceParameterSet* pParameters,
                          const std::vector<uint32_t> clipIndices,
                          const std::vector<const AnimationClip*>& pClipsIn) const override;

private:

    std::string m_clipName;
    uint32_t m_clipIndex;
    //const AnimationClip* m_pClip = nullptr;

};

// Basic LERP node
class AnimationBlendNodeLerp : public AnimationBlendNodeFactor1f {

public:

    AnimationBlendNodeLerp& setFirstInput(const AnimationBlendNode* pBlendNode);

    AnimationBlendNodeLerp& setSecondInput(const AnimationBlendNode* pBlendNode);

    void flattenRecursive(std::vector<const AnimationClip*>& pClips,
                          std::vector<float>& weights,
                          float weight,
                          const AnimationInstanceParameterSet* pParameters,
                          const std::vector<uint32_t> clipIndices,
                          const std::vector<const AnimationClip*>& pClipsIn) const override;

private:

    const AnimationBlendNode* m_pInputFirst = nullptr;
    const AnimationBlendNode* m_pInputSecond = nullptr;
};

// Generalized 1-D blend node
class AnimationBlendNode1D : public AnimationBlendNodeFactor1f {

public:

    AnimationBlendNode1D& addChild(AnimationBlendNode* pNode, float position);

    void flattenRecursive(std::vector<const AnimationClip*>& pClips,
                          std::vector<float>& weights,
                          float weight,
                          const AnimationInstanceParameterSet* pParameters,
                          const std::vector<uint32_t> clipIndices,
                          const std::vector<const AnimationClip*>& pClipsIn) const override;

    void finalize() override;

private:

    std::vector<AnimationBlendNode*> m_pChildren;
    std::vector<float> m_childPositions;
};


// Generalized 2-D blend node
class AnimationBlendNode2D : public AnimationBlendNodeFactor2f {

public:

    AnimationBlendNode2D& addChild(AnimationBlendNode* pNode, glm::vec2 position);

    void flattenRecursive(std::vector<const AnimationClip*>& pClips,
                          std::vector<float>& weights,
                          float weight,
                          const AnimationInstanceParameterSet* pParameters,
                          const std::vector<uint32_t> clipIndices,
                          const std::vector<const AnimationClip*>& pClipsIn) const override;

    void finalize() override;

private:

    std::vector<AnimationBlendNode*> m_pChildren;
    std::vector<glm::vec2> m_childPositions;

    std::vector<glm::uvec3> m_triangles;
};

template<typename T>
T* AnimationBlendTree::createNode() {
    T* pNode = new T();
    pNode->setParentTree(this);
    m_pNodes.push_back(pNode);
    return pNode;
}

#endif // ANIMATION_BLEND_TREE_H_INCLUDED
