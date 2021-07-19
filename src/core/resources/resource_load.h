#ifndef RESOURCE_LOAD_H_
#define RESOURCE_LOAD_H_

#include <string>

#include "asset.h"
#include "resource_manager.h"
#include "texture.h"

// TODO: create some sort of resource manager class that can load/own/provide access to all resource types

bool loadTexture(const std::string& fileName, Texture* pTexture);
bool loadTexture(const std::string& fileName, Texture* pTexture, TextureParameters parameters);

// https://stackoverflow.com/questions/2912520/read-file-contents-into-a-string-in-c
// Author: Maik Beckmann
std::string file_as_string(std::string fname);

// I found Assimp's OBJ importer had some issues with vertex winding order, and seemed to duplicate vertices
// So instead I use TinyObjLoader to load the .obj file, and wrote this function to import it
Asset* importOBJ(const std::string& fileName, ResourceManager* pResManager);

// Import an asset using Assimp
// Works for all file types supported by Assimp
// Imports Models (Meshes, MeshData, Materials) as well as animation data (SkeletonDescription, AnimationClips)
//   and stores in the ResourceManager
// Converts the internal scene hierarchy of the asset file to a hierarchy of Asset::Node and stores it in
//   the Asset
// Returns: the imported Asset if successful, else nullptr
// The imported asset will be added to the ResourceManager on success, so calling code should not try to add it
Asset* importAsset(const std::string& fileName, ResourceManager* pResManager);

// Load an AnimationStateGraph and its associated AnimationBlendTrees from a JSON file
// upon success, an AnimationStateGraph and its BlendTrees will be allocated and added to the ResourceManager, and the state graph will be returned
AnimationStateGraph* loadAnimationStateGraph(const std::string& fileName, ResourceManager* pResManager);


#endif // RESOURCE_LOAD_H_
