#ifndef ASSET_H_INCLUDED
#define ASSET_H_INCLUDED

#include <string>
#include <vector>

#include <glm/glm.hpp>

#include "model.h"
#include "core/animation/animation.h"

struct Asset {

    struct Node {
        Model* pModel = nullptr;
        Node* pParent = nullptr;
        std::vector<Node*> pChildren;

        std::string name;

        glm::mat4 transform = glm::mat4(1.0f);

        ~Node() {
            for (Node* pChild : pChildren) delete pChild;
        }
    };

    std::vector<Model*> pModels;
    std::vector<AnimationClip*> pAnimationClips;

    std::string name;

    Node* pRootNode = nullptr;

    Asset() {}
    Asset(const Asset&) = delete;
    Asset(Asset&& asset) :
        pModels(std::move(asset.pModels)),
        pAnimationClips(std::move(asset.pAnimationClips)),
        name(std::move(asset.name)),
        pRootNode(std::move(asset.pRootNode)) {
        asset.pRootNode = nullptr;
    }

    Asset& operator=(const Asset&) = delete;
    Asset& operator=(Asset&& asset) {
        pModels = std::move(asset.pModels);
        pAnimationClips = std::move(asset.pAnimationClips);
        name = std::move(asset.name);
        pRootNode = std::move(asset.pRootNode);
        asset.pRootNode = nullptr;
        return *this;
    }

    ~Asset() {
        if (pRootNode) delete pRootNode;
    }

};

#endif // ASSET_H_INCLUDED
