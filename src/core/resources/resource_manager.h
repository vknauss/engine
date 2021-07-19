#ifndef RESOURCE_MANAGER_H_INCLUDED
#define RESOURCE_MANAGER_H_INCLUDED

#include <memory>
#include <vector>

#include "asset.h"
#include "mesh.h"
#include "mesh_data.h"
#include "model.h"
#include "material.h"
#include "skeleton_description.h"
#include "core/animation/animation_state_graph.h"


// Storage for the various "Resource Types"
// I differentiate between Resource and Instance types as Resources can be pointed to by multiple Instances which live in the game world.
// These resource types are just heap-allocated and stored in vectors, since it is unlikely cache locality will matter since they are
// almost never going to be iterated directly, and instead will be referenced via pointers from instances
// To avoid fragmentation, it might be good to implement a pool allocation system and possibly use shared_ptr for lifetimes. we will see
class ResourceManager {

public:

    // std::vector<Asset*> pAssets;
    // std::vector<Mesh*> pMeshes;
    // std::vector<MeshData*> pMeshData;
    // std::vector<Model*> pModels;
    // std::vector<Material*> pMaterials;
    // std::vector<Texture*> pTextures;
    // std::vector<SkeletonDescription*> pSkeletonDescriptions;
    // std::vector<AnimationClip*> pAnimationClips;
    // std::vector<AnimationBlendTree*> pAnimationBlendTrees;
    // std::vector<AnimationStateGraph*> pAnimationStateGraphs;

    // void cleanup() {
    //     for (Asset* pAsset : pAssets) if (pAsset) delete pAsset;
    //     pAssets.clear();

    //     for (Mesh* pMesh : pMeshes) if (pMesh) delete pMesh;
    //     pMeshes.clear();

    //     for (MeshData* pMeshData : pMeshData) if (pMeshData) delete pMeshData;
    //     pMeshData.clear();

    //     for (Model* pModel : pModels) if (pModel) delete pModel;
    //     pModels.clear();

    //     for (Material* pMaterial : pMaterials) if (pMaterial) delete pMaterial;
    //     pMaterials.clear();

    //     for (Texture* pTexture : pTextures) if (pTexture) delete pTexture;
    //     pTextures.clear();

    //     for (SkeletonDescription* pSkeletonDescription : pSkeletonDescriptions)
    //         if (pSkeletonDescription) delete pSkeletonDescription;
    //     pSkeletonDescriptions.clear();

    //     for (AnimationClip* pAnimationClip : pAnimationClips)
    //         if (pAnimationClip) delete pAnimationClip;
    //     pAnimationClips.clear();

    //     for (AnimationBlendTree* pAnimationBlendTree : pAnimationBlendTrees)
    //         if (pAnimationBlendTree) delete pAnimationBlendTree;
    //     pAnimationBlendTrees.clear();

    //     for (AnimationStateGraph* pAnimationStateGraph : pAnimationStateGraphs)
    //         if (pAnimationStateGraph) delete pAnimationStateGraph;
    //     pAnimationStateGraphs.clear();
    // }
    std::vector<std::unique_ptr<Asset>> pAssets;
    std::vector<std::unique_ptr<Mesh>> pMeshes;
    std::vector<std::unique_ptr<MeshData>> pMeshData;
    std::vector<std::unique_ptr<Model>> pModels;
    std::vector<std::unique_ptr<Material>> pMaterials;
    std::vector<std::unique_ptr<Texture>> pTextures;
    std::vector<std::unique_ptr<SkeletonDescription>> pSkeletonDescriptions;
    std::vector<std::unique_ptr<AnimationClip>> pAnimationClips;
    std::vector<std::unique_ptr<AnimationBlendTree>> pAnimationBlendTrees;
    std::vector<std::unique_ptr<AnimationStateGraph>> pAnimationStateGraphs;

    void clear() {
        pAssets.clear();
        pMeshes.clear();
        pMeshData.clear();
        pModels.clear();
        pMaterials.clear();
        pTextures.clear();
        pSkeletonDescriptions.clear();
        pAnimationClips.clear();
        pAnimationBlendTrees.clear();
        pAnimationStateGraphs.clear();
    }

};

#endif // RESOURCE_MANAGER_H_INCLUDED
