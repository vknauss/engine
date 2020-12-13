#ifndef RESOURCE_LOAD_H_
#define RESOURCE_LOAD_H_

#include <string>

#include "animation.h"
#include "mesh_data.h"
#include "skeleton.h"
#include "texture.h"
#include "material.h"

// TODO: create some sort of resource manager class that can load/own/provide access to all resource types

void load_texture(const std::string& texturePath, Texture* texture);
void load_texture(const std::string& texturePath, Texture* texture, TextureParameters parameters);

// https://stackoverflow.com/questions/2912520/read-file-contents-into-a-string-in-c
// Author: Maik Beckmann
std::string file_as_string(std::string fname);

void load_skeleton(std::string fname, Skeleton* skeleton);

void load_pose(std::string fname, Animation* animation, float sampleTime);

void load_animation(std::string fname, Animation* animation);

void load_obj(std::string fname, MeshData& md);
void load_obj_multi(std::string fname, std::vector<MeshData>& meshes, std::vector<Material>& materials, std::vector<int>& materialIDs, std::vector<Texture*>& textures);

void load_model(std::string fname, MeshData& md);

void importAssimp(
    const std::string& fileName,
    std::vector<MeshData>& meshes,
    std::vector<Material>& materials,
    std::vector<int>& materialIDs,
    std::vector<Texture*>& textures,
    std::vector<SkeletonDescription>& skeletonDescriptions,
    std::vector<int>& skeletonDescIndices,
    std::vector<Animation>& animations);


#endif // RESOURCE_LOAD_H_
