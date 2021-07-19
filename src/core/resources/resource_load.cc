#include "resource_load.h"

#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <queue>
#include <vector>

#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>

#define TINYOBJLOADER_IMPLEMENTATION
#include <tinyobjloader/tiny_obj_loader.h>

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <assimp/pbrmaterial.h>

#include <nlohmann/json.hpp>

#include "core/util/mesh_builder.h"


bool loadTexture(const std::string& fileName, Texture* pTexture) {
    TextureParameters parameters = {};
    parameters.useFloatComponents = false;
    parameters.useEdgeClamping = false;
    parameters.useLinearFiltering = true;
    parameters.useMipmapFiltering = true;
    parameters.useAnisotropicFiltering = true;

    return loadTexture(fileName, pTexture, parameters);
}

bool loadTexture(const std::string& fileName, Texture* pTexture, TextureParameters parameters) {
    int w, h, n;
    unsigned char* data = stbi_load(fileName.c_str(), &w, &h, &n, 0);
    if (data) {
        parameters.arrayLayers = 1;
        parameters.numComponents = n;
        parameters.width = w;
        parameters.height = h;

        pTexture->setName(std::filesystem::path(fileName).filename());
        pTexture->setParameters(parameters);
        pTexture->allocateData(data);
        if (parameters.useMipmapFiltering) pTexture->generateMipmaps();

        stbi_image_free(data);

        return true;
    } else {
        std::cerr << "ERROR: Failed to load texture from: " << fileName << std::endl;
        return false;
    }
}

// https://stackoverflow.com/questions/2912520/read-file-contents-into-a-string-in-c
// Author: Maik Beckmann
std::string file_as_string(std::string fname) {
  std::ifstream ifs(fname);
  std::string content((std::istreambuf_iterator<char>(ifs)),(std::istreambuf_iterator<char>()));
  return content;
}

void load_skeleton(const std::string& fileName, SkeletonDescription* pSkeletonDesc) {
    std::ifstream fileStream(fileName);
    size_t nBones;
    fileStream >> nBones;

    size_t c_jointIndex = Joint::JOINT_UNINITIALIZED;
    std::map<size_t, size_t> parentMap;

    size_t c_line = 0;
    while(!fileStream.eof()) {
        ++c_line;

        std::string fn;
        fileStream >> fn;

        if(fn.length() == 0) continue;
        if(fn[0] != 'b') {
            std::cerr <<
                "Skeleton syntax error: unknown token '" << fn << "' [at " << fileName << ":" << c_line << "]"
                << std::endl;
            continue;
        }
        if(fn.length() == 1) {
            std::string name;
            fileStream >> name;

            c_jointIndex = pSkeletonDesc->createJoint(name);
        } else {
            if (c_jointIndex == Joint::JOINT_UNINITIALIZED) {
                std::cerr <<
                    "Skeleton syntax error: token '" << fn << "' does not refer to any bone [at " << fileName << ":" << c_line << "]"
                    << std::endl;
                continue;
            }
            switch(fn[1]) {
            case 'p':
            {
                std::string pname;
                fileStream >> pname;

                parentMap.insert(std::make_pair(c_jointIndex, pSkeletonDesc->getJointIndex(pname)));
            }
            break;
            case 'o':
            {
                glm::vec3 ovec;
                fileStream >> ovec.x >> ovec.y >> ovec.z;

                pSkeletonDesc->setBindOffset(c_jointIndex, glm::mat3(1, 0, 0,  0, 0, -1,  0, 1, 0) * ovec);
            }
            break;
            case 'r':
            {
                glm::quat rquat;
                fileStream >> rquat.w >> rquat.x >> rquat.y >> rquat.z;
                // This line is to compensate for Blender's axis orientation (up=+z, forward=+y). Really this should be done at export time.
                pSkeletonDesc->setBindRotation(c_jointIndex, glm::quat(rquat.w, rquat.x, rquat.z, -rquat.y));
            }
            break;
            default:
            std::cerr <<
                "Skeleton syntax error: unknown token '" << fn << "' [at " << fileName << ":" << c_line << "]"
                << std::endl;
            }
        }
    }

    for(auto pr : parentMap) {
        pSkeletonDesc->setParent(pr.first, pr.second);
    }

    pSkeletonDesc->computeBindTransforms();
}

/*void load_pose(std::string fname, Animation* animation, float sampleTime) {
    std::ifstream file_stream;
    file_stream.open(fname);

    size_t c_jointIndex;;
    Animation::JointTransformSample* c_jt = nullptr;

    while(!file_stream.eof()) {
        std::string fn;
        file_stream >> fn;

        if(fn.length() == 0) continue;
        if(fn[0] != 'b') {
            std::cerr <<
                "Unknown token '" << fn << "' in parsing Pose '" << fname << "' will be skipped." <<
            std::endl;
            continue;
        }
        if(fn.length() == 1) {
            std::string name;
            file_stream >> name;

            c_jointIndex = animation->getSkeletonDescription()->getJointIndex(name);
            c_jt = &animation->createJointTransformSample(c_jointIndex, sampleTime);

        } else {
            switch(fn[1]) {
            case 'o':
            file_stream >> c_jt->offset.x >> c_jt->offset.y >> c_jt->offset.z;
            c_jt->offset = glm::mat3(1, 0, 0,  0, 0, -1,  0, 1, 0) * c_jt->offset;
            break;
            case 'r':
            file_stream >> c_jt->rotation.w >> c_jt->rotation.x >> c_jt->rotation.y >> c_jt->rotation.z;
            c_jt->rotation = glm::quat(c_jt->rotation.w, glm::mat3(1, 0, 0,  0, 0, -1,  0, 1, 0) * glm::vec3(c_jt->rotation.x, c_jt->rotation.y, c_jt->rotation.z));
            //c_joint->rotation = glm::quat((float)M_SQRT2/2, -(float)M_SQRT2/2, 0, 0) * c_joint->rotation;
            break;
            default:
            std::cerr <<
                "Unknown token '" << fn << "' in parsing Pose '" << fname << "' will be skipped." <<
            std::endl;
            }
        }
    }
    file_stream.close();
}*/

/*void load_animation(std::string fname, Animation* animation) {
    std::string data_file_name = fname + "data.txt";
    std::ifstream dfile_stream;
    dfile_stream.open(data_file_name);

    int n_poses;
    dfile_stream >> n_poses;

    for(int i = 0; i < n_poses; i++) {
        float t;
        dfile_stream >> t;
        load_pose(fname + "pose" + std::to_string(i) + ".txt", animation, t);
    }

    dfile_stream.close();
}*/

void load_obj(std::string fname, MeshData& md) {
    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::string warning, error;

    if(!tinyobj::LoadObj(&attrib, &shapes, &materials, &warning, &error, fname.c_str())) {
        std::cerr << "Couldn't load OBJ from " << fname << std::endl;
        std::cerr << "TinyObjLoader warning string: " << warning << std::endl;
        std::cerr << "TinyObjLoader error string: " << error << std::endl;
        return;
    }

    size_t numVertices = attrib.vertices.size();
    size_t numNormals = attrib.normals.size();
    size_t numTexCoords = attrib.texcoords.size();

    int vertexBits = (int) std::ceil(std::log2((double) numVertices));
    int normalBits = (int) std::ceil(std::log2((double) numNormals));
    int texCoordBits = (int) std::ceil(std::log2((double) numTexCoords));
    if(vertexBits + normalBits + texCoordBits > 64) {
        std::cerr << "Not enough bits for key generation! Model loading will go ahead but results may be unexpected." << std::endl;
    }

    std::map<size_t, size_t> indexMap;

    for(const tinyobj::shape_t& shape : shapes) {
        for(const tinyobj::index_t& index : shape.mesh.indices) {
            size_t key = std::max(index.vertex_index, 0);
            key = key << normalBits;
            key = key | std::max(index.normal_index, 0);
            key = key << texCoordBits;
            key = key | std::max(index.texcoord_index, 0);

            auto it = indexMap.find(key);
            if(it != indexMap.end()) {
                md.indices.push_back(it->second);
            } else {
                size_t ind = md.vertices.size();
                indexMap.insert(std::make_pair(key, ind));
                if(index.vertex_index >= 0)
                    md.vertices.push_back(
                        glm::vec3(  attrib.vertices[3*index.vertex_index],
                                    attrib.vertices[3*index.vertex_index+1],
                                    attrib.vertices[3*index.vertex_index+2]));
                if(index.normal_index >= 0)
                    md.normals.push_back(
                        glm::vec3(  attrib.normals[3*index.normal_index],
                                    attrib.normals[3*index.normal_index+1],
                                    attrib.normals[3*index.normal_index+2]));
                if(index.texcoord_index >= 0)
                    md.uvs.push_back(
                        glm::vec2(  attrib.texcoords[2*index.texcoord_index],
                                    attrib.texcoords[2*index.texcoord_index+1]));
                md.indices.push_back(ind);
            }
        }
    }
}

Asset* importOBJ(const std::string& fileName, ResourceManager* pResManager) {
    std::filesystem::path filePath(fileName);
    std::filesystem::path baseDir = filePath.has_parent_path() ?
        filePath.parent_path() :
        std::filesystem::current_path();

    tinyobj::ObjReaderConfig tObjConfig;
    tObjConfig.triangulate = true;
    tObjConfig.vertex_color = false;
    tObjConfig.mtl_search_path = baseDir;

    tinyobj::ObjReader tObjReader;
    tObjReader.ParseFromFile(fileName, tObjConfig);

    if (!tObjReader.Valid()) {
        std::cerr << "OBJ Import: Couldn't load OBJ from " << fileName << std::endl;
        std::cerr << "TinyObjLoader warning string: " << tObjReader.Warning() << std::endl;
        std::cerr << "TinyObjLoader error string: " << tObjReader.Error() << std::endl;
        return nullptr;
    }

    auto& pAsset = pResManager->pAssets.emplace_back(std::make_unique<Asset>());

    const tinyobj::attrib_t& attrib = tObjReader.GetAttrib();
    const std::vector<tinyobj::shape_t>& shapes = tObjReader.GetShapes();
    const std::vector<tinyobj::material_t>& mtls = tObjReader.GetMaterials();

    size_t numVertices = attrib.vertices.size();
    size_t numNormals = attrib.normals.size();
    size_t numTexCoords = attrib.texcoords.size();

    int vertexBits = (int) std::ceil(std::log2((double) numVertices));
    int normalBits = (int) std::ceil(std::log2((double) numNormals));
    int texCoordBits = (int) std::ceil(std::log2((double) numTexCoords));
    if (vertexBits + normalBits + texCoordBits > 64) {
        std::cerr << "OBJ Import: Not enough bits for key generation! Model loading will go ahead but results may be unexpected." << std::endl;
    }

    std::map<std::string, Texture*> textureMap;

    auto& pMaterials = pResManager->pMaterials;
    size_t mtlOffset = pMaterials.size();
    pMaterials.resize(mtlOffset + mtls.size());

    auto& pTextures = pResManager->pTextures;

    std::vector<bool> needsTBN(mtls.size(), false);

    auto getTexture = [&] (const std::string& textureFile) {
        if (auto kv = textureMap.find(textureFile); kv != textureMap.end()) {
            return kv->second;
        } else {
            Texture texture;
            std::filesystem::path pathFromBase = baseDir;
            pathFromBase.append(textureFile);
            if (!loadTexture(textureFile, &texture) &&
                    !loadTexture(pathFromBase, &texture)) {
                return (Texture*) nullptr;
            } else {
                auto& pTexture = pTextures.emplace_back(std::make_unique<Texture>(std::move(texture)));
                textureMap.insert({textureFile, pTexture.get()});
                return pTexture.get();
            }
        }
    };

    for (auto i = 0u; i < mtls.size(); ++i) {
        const tinyobj::material_t& mtl = mtls[i];
        auto& pMaterial = pMaterials[mtlOffset + i] = std::make_unique<Material>();

        pMaterial->setTintColor({mtl.diffuse[0], mtl.diffuse[1], mtl.diffuse[2]});
        pMaterial->setRoughness(std::sqrt(1.0f / std::log((float) mtl.shininess)));

        if (!mtl.diffuse_texname.empty()) {
            if (Texture* pTexture = getTexture(mtl.diffuse_texname))
                pMaterial->setDiffuseTexture(pTexture);
        }

        std::string normal_texname;
        bool doNormal = false;
        if (!mtl.normal_texname.empty()) {
            normal_texname = mtl.normal_texname;
            doNormal = true;
        } else if (!mtl.displacement_texname.empty()) {
            normal_texname = mtl.displacement_texname;
            doNormal = true;
        } else if (!mtl.bump_texname.empty()) {
            normal_texname = mtl.bump_texname;
            doNormal = true;
        }
        if (doNormal) {
            if (Texture* pTexture = getTexture(normal_texname)) {
                pMaterial->setNormalsTexture(pTexture);
                needsTBN[i] = true;
            }
        }
    }

    pAsset->name = filePath.filename();
    pAsset->pRootNode = new Asset::Node;
    pAsset->pRootNode->name = filePath.stem();

    for (auto i = 0u; i < shapes.size(); ++i) {
        const tinyobj::shape_t& shape = shapes[i];

        Asset::Node* pNode = pAsset->pRootNode->pChildren.emplace_back(new Asset::Node);
        pNode->pParent = pAsset->pRootNode;
        pNode->name = shape.name;

        std::vector<std::map<uint64_t, uint32_t>> subMeshIndexMaps;  // for each sub mesh, maps the 64-bit key onto a vertex index
        std::map<int, size_t> materialMap;  // indices of each sub mesh, keyed by material ID
        std::vector<int> subMeshMaterialIDs;
        std::vector<MeshData*> pSubMeshes;

        for (auto j = 0u; j < shape.mesh.indices.size(); j += 3) {
            int materialID = shape.mesh.material_ids[j/3];
            size_t subMeshID;
            if (auto kv = materialMap.find(materialID); kv != materialMap.end()) {
                subMeshID = kv->second;
            } else {
                subMeshID = pSubMeshes.size();
                subMeshIndexMaps.emplace_back();
                pSubMeshes.emplace_back(new MeshData);
                subMeshMaterialIDs.push_back(materialID);
                materialMap.insert({materialID, subMeshID});
            }

            MeshData* pSubMesh = pSubMeshes[subMeshID];
            std::map<uint64_t, uint32_t>& indexMap = subMeshIndexMaps[subMeshID];

            for (int k = 0; k < 3; ++k) {
                const tinyobj::index_t& index = shape.mesh.indices[j+k];

                uint64_t key = std::max(index.vertex_index, 0);
                key = key << normalBits;
                key = key | std::max(index.normal_index, 0);
                key = key << texCoordBits;
                key = key | std::max(index.texcoord_index, 0);

                if (auto it = indexMap.find(key); it != indexMap.end()) {
                    pSubMesh->indices.push_back(it->second);
                } else {
                    uint32_t ind = (uint32_t) pSubMesh->vertices.size();
                    indexMap.insert(std::make_pair(key, ind));
                    pSubMesh->indices.push_back(ind);

                    if (index.vertex_index >= 0) {
                        pSubMesh->vertices.emplace_back(attrib.vertices[3*index.vertex_index],
                                                        attrib.vertices[3*index.vertex_index+1],
                                                        attrib.vertices[3*index.vertex_index+2]);
                    }
                    if (index.normal_index >= 0) {
                        pSubMesh->normals.emplace_back(attrib.normals[3*index.normal_index],
                                                       attrib.normals[3*index.normal_index+1],
                                                       attrib.normals[3*index.normal_index+2]);
                    }
                    if (index.texcoord_index >= 0) {
                        pSubMesh->uvs.emplace_back(attrib.texcoords[2*index.texcoord_index],
                                                   attrib.texcoords[2*index.texcoord_index+1]);
                    }
                }
            }
        }

        for (auto& subMesh : pSubMeshes) {
            pResManager->pMeshData.push_back(std::unique_ptr<MeshData>(subMesh));
        }
        for (auto j = 0u; j < pSubMeshes.size(); ++j) {
            if (needsTBN[subMeshMaterialIDs[j]]) {
                MeshBuilder(std::move(*pSubMeshes[j]))
                    .calculateTangentSpace()
                    .moveMeshData(*pSubMeshes[j]);
            }

            auto& pModel = pResManager->pModels.emplace_back(std::make_unique<Model>());
            auto& pMesh = pResManager->pMeshes.emplace_back(std::make_unique<Mesh>());
            pMesh->createFromMeshData(pSubMeshes[j]);
            pModel->setMesh(pMesh.get());
            pModel->setMaterial(pMaterials[mtlOffset + subMeshMaterialIDs[j]].get());
            pModel->setBoundingSphere(pSubMeshes[j]->computeBoundingSphere());

            pAsset->pModels.push_back(pModel.get());
            if (pSubMeshes.size() == 1) {
                pNode->pModel = pModel.get();
            } else {
                Asset::Node* pMeshNode = pNode->pChildren.emplace_back(new Asset::Node);
                pMeshNode->pParent = pNode;
                pMeshNode->pModel = pModel.get();
                pMeshNode->name = shape.name + "_" + mtls[subMeshMaterialIDs[j]].name;
            }
        }
    }

    return pAsset.get();
}

static std::unique_ptr<Texture> loadTextureAssimp(const aiString& texPath, const aiScene* pAiScene, TextureParameters param, std::filesystem::path modelDirectory) {
    std::unique_ptr<Texture> pTexture(nullptr);

    param.arrayLayers = 1;
    param.samples = 1;

    const aiTexture* pAiTex = pAiScene->GetEmbeddedTexture(texPath.C_Str());

    void* pStbData = nullptr;
    int width, height, components;

    std::string name;

    if (pAiTex) {
        // Texture stored in internal scene array
        if (pAiTex->mHeight == 0) {
            // Compressed
            if (param.useFloatComponents) {
                param.bitsPerComponent = 16;
                pStbData = static_cast<void*>(stbi_loadf_from_memory(
                    reinterpret_cast<stbi_uc*>(pAiTex->pcData),
                    static_cast<int>(pAiTex->mWidth),
                    &width, &height, &components, 0));
            } else {
                param.bitsPerComponent = 8;
                pStbData = static_cast<void*>(stbi_load_from_memory(
                    reinterpret_cast<stbi_uc*>(pAiTex->pcData),
                    static_cast<int>(pAiTex->mWidth),
                    &width, &height, &components, 0));
            }

        } else {
            // Uncompressed, stored as BGRA8
            param.width = static_cast<int>(pAiTex->mWidth);
            param.height = static_cast<int>(pAiTex->mHeight);
            param.numComponents = 4;
            param.isBGR = true;
            param.bitsPerComponent = 8;

            pTexture = std::make_unique<Texture>();
            pTexture->setParameters(param);
            pTexture->allocateData(static_cast<void*>(pAiTex->pcData));
            pTexture->generateMipmaps();
        }

        name = std::filesystem::path(pAiTex->mFilename.C_Str()).stem();
    } else {
        std::filesystem::path filepath(texPath.C_Str());

        if (param.useFloatComponents) {
            param.bitsPerComponent = 16;
            pStbData = static_cast<void*>(stbi_loadf(texPath.C_Str(), &width, &height, &components, 0));
            if (!pStbData) {
                filepath = modelDirectory;
                filepath.append(texPath.C_Str());
                pStbData = static_cast<void*>(stbi_loadf(filepath.c_str(), &width, &height, &components, 0));
            }
        } else {
            param.bitsPerComponent = 8;
            pStbData = static_cast<void*>(stbi_load(texPath.C_Str(), &width, &height, &components, 0));
            if (!pStbData) {
                filepath = modelDirectory;
                filepath.append(texPath.C_Str());
                pStbData = static_cast<void*>(stbi_load(filepath.c_str(), &width, &height, &components, 0));
            }
        }
        name = filepath.stem();
    }

    if (!pTexture) {
        if (pStbData) {
            param.width = width;
            param.height = height;
            param.numComponents = components;

            pTexture = std::make_unique<Texture>();
            pTexture->setParameters(param);
            pTexture->allocateData(pStbData);
            pTexture->generateMipmaps();

            stbi_image_free(pStbData);
        } else {
            std::cerr <<
                "Error importing texture. Assimp texture path: " << texPath.C_Str()
                << std::endl;
        }
    }
    if (pTexture) pTexture->setName(name);
    return pTexture;
}

static void buildSkeletonDescriptionAssimp(const aiNode* pAiNode, size_t parentIndex, SkeletonDescription* pSkeletonDesc, std::map<const aiNode*, size_t>& nodeJointIndices) {
    size_t jointIndex = pSkeletonDesc->createJoint(pAiNode->mName.C_Str());
    nodeJointIndices[pAiNode] = jointIndex;
    pSkeletonDesc->setParent(jointIndex, parentIndex);

    const aiMatrix4x4& aiNodeMatrix = pAiNode->mTransformation;
    glm::mat4 bindMatrixLocal;
    for (int i = 0; i < 4; ++i) for (int j = 0; j < 4; ++j) {
        bindMatrixLocal[i][j] = aiNodeMatrix[j][i];  // transposed
    }

    aiQuaternion aiNodeRotation;
    aiVector3D aiNodeOffset;
    aiNodeMatrix.DecomposeNoScaling(aiNodeRotation, aiNodeOffset);

    //pSkeletonDesc->setBindRotation(jointIndex, glm::quat(aiNodeRotation.w, aiNodeRotation.x, aiNodeRotation.y, aiNodeRotation.z));
    //pSkeletonDesc->setBindOffset(jointIndex, glm::vec3(aiNodeOffset.x, aiNodeOffset.y, aiNodeOffset.z));

    /*{
        std::cout << "Matrix (Joint " << jointIndex << "): " << std::endl;
        std::cout << glm::to_string(bindMatrixLocal[0]) << std::endl;
        std::cout << glm::to_string(bindMatrixLocal[1]) << std::endl;
        std::cout << glm::to_string(bindMatrixLocal[2]) << std::endl;
        std::cout << glm::to_string(bindMatrixLocal[3]) << std::endl;
    }*/

    //pSkeletonDesc->setBindMatrix(jointIndex, glm::inverse(bindMatrixLocal));
    //pSkeletonDesc->setBindMatrix(jointIndex, bindMatrixLocal);
    for (size_t i = 0; i < pAiNode->mNumChildren; ++i) {
        buildSkeletonDescriptionAssimp(pAiNode->mChildren[i], jointIndex, pSkeletonDesc, nodeJointIndices);
    }
}

static glm::mat4 aiToGLM(const aiMatrix4x4& aiMat) {
    return glm::mat4(aiMat.a1, aiMat.b1, aiMat.c1, aiMat.d1,
                     aiMat.a2, aiMat.b2, aiMat.c2, aiMat.d2,
                     aiMat.a3, aiMat.b3, aiMat.c3, aiMat.d3,
                     aiMat.a4, aiMat.b4, aiMat.c4, aiMat.d4);
}

static Asset::Node* buildAssetNodeHierarchy(const aiNode* pAiNode, glm::mat4 tfmAcc, const Asset* pAsset) {
    tfmAcc = tfmAcc * aiToGLM(pAiNode->mTransformation);
    Asset::Node* pNode = nullptr;
    if (pAiNode->mNumMeshes > 0) {
        pNode = new Asset::Node;
        if (pAiNode->mNumMeshes == 1) {
            pNode->pModel = pAsset->pModels[pAiNode->mMeshes[0]];
        } else {
            for (auto i = 0u; i < pAiNode->mNumMeshes; ++i) {
                Asset::Node* pChildNode = new Asset::Node;
                pChildNode->transform = glm::mat4(1.0f);
                pChildNode->pModel = pAsset->pModels[pAiNode->mMeshes[i]];
                pChildNode->pParent = pNode;
                pChildNode->name = pChildNode->pModel->getMesh()->getName();
                pNode->pChildren.push_back(pChildNode);
            }
        }
    } else if (pAiNode->mNumChildren == 1) {
        return buildAssetNodeHierarchy(pAiNode->mChildren[0], tfmAcc, pAsset);
    }
    if (pAiNode->mNumChildren > 0) {
        if (!pNode) pNode = new Asset::Node;
        for (auto i = 0u; i < pAiNode->mNumChildren; ++i) {
            Asset::Node* pChildNode = buildAssetNodeHierarchy(pAiNode->mChildren[i], glm::mat4(1.0f), pAsset);
            if (pChildNode) {
                pChildNode->pParent = pNode;
                pNode->pChildren.push_back(pChildNode);
            }
        }
        if (pNode->pChildren.empty()) {
            delete pNode;
            return nullptr;
        } else if (pNode->pChildren.size() == 1) {
            Asset::Node* pChildNode = pNode->pChildren[0];
            pNode->pChildren[0] = nullptr;
            delete pNode;
            return pChildNode;
        }
    }
    if (pNode) {
        pNode->name = pAiNode->mName.C_Str();
        pNode->transform = tfmAcc;
    }
    return pNode;
}

Asset* importAsset(const std::string& fileName, ResourceManager* pResManager) {
    Assimp::Importer importer;

    unsigned int flags =
        aiProcess_CalcTangentSpace  |
        aiProcess_Triangulate   |
        aiProcess_JoinIdenticalVertices |
        aiProcess_SortByPType |
        aiProcess_FixInfacingNormals |
        aiProcess_GenNormals |
        aiProcess_PopulateArmatureData;  // Note that this flag is not safe unless Assimp::ArmaturePopulate::Execute has been fixed to not cause an assert failure when the model has no bones.

    const aiScene* pAiScene = importer.ReadFile(fileName, flags);

    importer.GetException();

    if (!pAiScene || importer.GetErrorString()[0] != '\0') {
        std::cerr << "Assimp error: " << importer.GetErrorString() << std::endl;
        return nullptr;
    }


    std::filesystem::path filepath(fileName);

    std::filesystem::path modelDirectory = filepath.has_parent_path() ? filepath.parent_path() : std::filesystem::current_path();

    auto& pAsset = pResManager->pAssets.emplace_back(std::make_unique<Asset>());

    pAsset->name = filepath.stem();

    auto& pMaterials = pResManager->pMaterials;
    auto&  pTextures  = pResManager->pTextures;

    if (pAiScene->HasMaterials()) {
        size_t offset = pMaterials.size();
        pMaterials.resize(offset + pAiScene->mNumMaterials);
        for (size_t i = offset; i < pMaterials.size(); ++i) {
            const aiMaterial* pAiMat = pAiScene->mMaterials[i-offset];

            pMaterials[i] = std::make_unique<Material>();
            pMaterials[i]->setName(pAiMat->GetName().C_Str());

            aiColor3D diffuseColor;
            if (pAiMat->Get(AI_MATKEY_COLOR_DIFFUSE, diffuseColor) == AI_SUCCESS) {
                pMaterials[i]->setTintColor({diffuseColor.r, diffuseColor.g, diffuseColor.b});
            }

            aiColor3D emissionColor;
            if (pAiMat->Get(AI_MATKEY_COLOR_EMISSIVE, emissionColor) == AI_SUCCESS) {
                pMaterials[i]->setEmissionIntensity({emissionColor.r, emissionColor.g, emissionColor.b});
            }

            float metallic;
            if (pAiMat->Get(AI_MATKEY_GLTF_PBRMETALLICROUGHNESS_METALLIC_FACTOR, metallic) == AI_SUCCESS) {
                pMaterials[i]->setMetallic(metallic);
            }

            float roughness;
            if (pAiMat->Get(AI_MATKEY_GLTF_PBRMETALLICROUGHNESS_ROUGHNESS_FACTOR, roughness) == AI_SUCCESS) {
                pMaterials[i]->setRoughness(roughness);
            }

            aiString diffuseTexPath;
            if (pAiMat->Get(AI_MATKEY_TEXTURE_DIFFUSE(0), diffuseTexPath) == AI_SUCCESS) {
                TextureParameters param = {};
                param.useAnisotropicFiltering = true;
                param.useEdgeClamping = true;
                param.useLinearFiltering = true;
                param.useMipmapFiltering = true;
                auto pDiffuseTexture = loadTextureAssimp(diffuseTexPath, pAiScene, param, modelDirectory);
                if (pDiffuseTexture) {
                    pTextures.push_back(std::move(pDiffuseTexture));
                    pMaterials[i]->setDiffuseTexture(pTextures.back().get());
                }
            }

            aiString normalTexPath;
            if (pAiMat->Get(AI_MATKEY_TEXTURE_NORMALS(0), normalTexPath) == AI_SUCCESS) {
                TextureParameters param = {};
                param.useFloatComponents = false;
                param.useEdgeClamping = true;
                param.useLinearFiltering = true;
                param.useMipmapFiltering = false;
                auto pNormalTexture = loadTextureAssimp(normalTexPath, pAiScene, param, modelDirectory);
                if (pNormalTexture) {
                    pTextures.push_back(std::move(pNormalTexture));
                    pMaterials[i]->setNormalsTexture(pTextures.back().get());
                }
            }

            aiString emissionTexPath;
            if (pAiMat->Get(AI_MATKEY_TEXTURE_EMISSIVE(0), emissionTexPath) == AI_SUCCESS) {
                TextureParameters param = {};
                param.useFloatComponents = true;
                param.useEdgeClamping = true;
                param.useLinearFiltering = true;
                auto pEmissionTexture = loadTextureAssimp(emissionTexPath, pAiScene, param, modelDirectory);
                if (pEmissionTexture) {
                    pTextures.push_back(std::move(pEmissionTexture));
                    pMaterials[i]->setEmissionTexture(pTextures.back().get());
                }
            }

            aiString metallicRoughnessTexPath;
            if (pAiMat->GetTexture(AI_MATKEY_GLTF_PBRMETALLICROUGHNESS_METALLICROUGHNESS_TEXTURE, &metallicRoughnessTexPath) == AI_SUCCESS) {
                TextureParameters param = {};
                param.useFloatComponents = true;
                param.useEdgeClamping = true;
                param.useLinearFiltering = true;
                auto pMetallicRoughnessTexture = loadTextureAssimp(metallicRoughnessTexPath, pAiScene, param, modelDirectory);
                if (pMetallicRoughnessTexture) {
                    pTextures.push_back(std::move(pMetallicRoughnessTexture));
                    pMaterials[i]->setMetallicRoughnessTexture(pTextures.back().get());
                }
            }
        }
    }

    auto& pMeshData = pResManager->pMeshData;
    auto& pMeshes = pResManager->pMeshes;
    auto& pModels = pResManager->pModels;
    auto& pSkeletonDescriptions = pResManager->pSkeletonDescriptions;

    if (pAiScene->HasMeshes()) {
        size_t offset = pMeshData.size();
        pMeshData.resize(offset + pAiScene->mNumMeshes);
        pMeshes.resize(offset + pAiScene->mNumMeshes);
        pModels.resize(offset + pAiScene->mNumMeshes);
        pAsset->pModels.resize(pAiScene->mNumMeshes);
        //std::map<const aiMesh*, size_t> meshMap;
        //std::vector<int> materialIDs(pMeshData.size(), -1);
        //std::vector<int> skeletonDescIndices(pMeshData.size(), -1);
        //int c_skeletonIndex = 0;
        std::map<const aiNode*, size_t> skeletonNodeMap;
        for (size_t i = offset; i < pMeshData.size(); ++i) {
            const aiMesh* pAiMesh = pAiScene->mMeshes[i-offset];

            //meshMap[pAiMesh] = i-offset;

            //std::cout << "Mesh: " << pAiMesh->mName.C_Str() << std::endl;
            pMeshData[i] = std::make_unique<MeshData>();
            pMeshes[i] = std::make_unique<Mesh>();
            pModels[i] = std::make_unique<Model>();
            pAsset->pModels[i-offset] = pModels[i].get();

            pMeshes[i]->setName(pAiMesh->mName.C_Str());

            pModels[i]->setMesh(pMeshes[i].get());

            pMeshData[i]->vertices.resize(pAiMesh->mNumVertices);
            memcpy(pMeshData[i]->vertices.data(), &pAiMesh->mVertices[0], 3*sizeof(float)*pAiMesh->mNumVertices);

            pMeshData[i]->normals.resize(pAiMesh->mNumVertices);
            memcpy(pMeshData[i]->normals.data(), &pAiMesh->mNormals[0], 3*sizeof(float)*pAiMesh->mNumVertices);

            pMeshData[i]->indices.resize(pAiMesh->mNumFaces*3);
            for (size_t j = 0; j < pAiMesh->mNumFaces; ++j) {
                memcpy(&pMeshData[i]->indices[3*j], pAiMesh->mFaces[j].mIndices, 3*sizeof(unsigned int));
            }

            if (pAiMesh->HasTextureCoords(0)) {
                pMeshData[i]->uvs.resize(pAiMesh->mNumVertices);
                std::transform(pAiMesh->mTextureCoords[0], pAiMesh->mTextureCoords[0]+pAiMesh->mNumVertices,
                               pMeshData[i]->uvs.begin(),
                               [] (const aiVector3D& v) { return glm::vec2(v.x, v.y); });
            }

            if (pAiMesh->HasTangentsAndBitangents()) {
                pMeshData[i]->tangents.resize(pAiMesh->mNumVertices);
                pMeshData[i]->bitangents.resize(pAiMesh->mNumVertices);
                memcpy(pMeshData[i]->tangents.data(), &pAiMesh->mTangents[0], 3*sizeof(float)*pAiMesh->mNumVertices);
                memcpy(pMeshData[i]->bitangents.data(), &pAiMesh->mBitangents[0], 3*sizeof(float)*pAiMesh->mNumVertices);
            }

            if (pAiMesh->mMaterialIndex < pAiScene->mNumMaterials) {
                //materialIDs[i] = static_cast<int>(pMaterials.size() - pAiScene->mNumMaterials + pAiMesh->mMaterialIndex);
                pModels[i]->setMaterial(pMaterials[pMaterials.size() - pAiScene->mNumMaterials + pAiMesh->mMaterialIndex].get());
            }

            if (pAiMesh->HasBones()) {

                pMeshData[i]->boneIndices.resize(pAiMesh->mNumVertices, glm::ivec4(0));
                pMeshData[i]->boneWeights.resize(pAiMesh->mNumVertices, glm::vec4(0.0));
                std::vector<int> numVertexBones(pAiMesh->mNumVertices, 0);

                SkeletonDescription* pSkeletonDesc;
                if (auto it = skeletonNodeMap.find(pAiMesh->mBones[0]->mArmature); it != skeletonNodeMap.end()) {
                    pSkeletonDesc = pSkeletonDescriptions[it->second].get();
                } else {
                    //skeletonDescIndices[i] = c_skeletonIndex;
                    skeletonNodeMap[pAiMesh->mBones[0]->mArmature] = pSkeletonDescriptions.size();
                    //++c_skeletonIndex;

                    //std::cout << "Skeleton: " << pAiMesh->mBones[0]->mArmature->mName.C_Str() << std::endl;

                    pSkeletonDesc = pSkeletonDescriptions.emplace_back(std::make_unique<SkeletonDescription>()).get();

                    pSkeletonDesc->setName(pAiMesh->mBones[0]->mArmature->mName.C_Str());

                    std::map<const aiNode*, size_t> nodeJointIndices;

                    buildSkeletonDescriptionAssimp(pAiMesh->mBones[0]->mArmature, SkeletonDescription::JOINT_NONE, pSkeletonDesc, nodeJointIndices);
                    if (!pSkeletonDesc->validate()) {
                        std::cerr <<
                            "Failed to validate skeleton description, imported from Assimp." <<
                            std::endl;
                    }

                    for (size_t k = 0; k < pAiMesh->mNumBones; ++k) {
                    const aiBone* pAiBone = pAiMesh->mBones[k];
                    size_t jointIndex = nodeJointIndices[pAiBone->mNode];

                    const aiMatrix4x4& aiBoneOffsetMatrix = pAiBone->mOffsetMatrix;
                    glm::mat4 bindMatrix;
                    for (int i = 0; i < 4; ++i) for (int j = 0; j < 4; ++j) {
                        bindMatrix[i][j] = aiBoneOffsetMatrix[j][i];  // transposed
                    }

                    pSkeletonDesc->setBindMatrix(jointIndex, glm::inverse(bindMatrix));
                    }
                    pSkeletonDesc->computeBindTransforms();
                }

                //std::cout << "Skeleton: " << pAiMesh->mBones[0]->mArmature->mName.C_Str() << " Assigned to Mesh: " << pAiMesh->mName.C_Str() << std::endl;

                assert(pSkeletonDesc);
                pModels[i]->setSkeletonDescription(pSkeletonDesc);

                for (size_t k = 0; k < pAiMesh->mNumBones; ++k) {
                    const aiBone* pAiBone = pAiMesh->mBones[k];
                    size_t jointIndex = pSkeletonDesc->getJointIndex(pAiBone->mName.C_Str()); //nodeJointIndices[pAiBone->mNode];

                    for (size_t l = 0; l < pAiBone->mNumWeights; ++l) {
                        size_t vertexIndex = pAiBone->mWeights[l].mVertexId;
                        if (numVertexBones[vertexIndex] < 4) {
                            pMeshData[i]->boneIndices[vertexIndex][numVertexBones[vertexIndex]] = static_cast<int>(jointIndex);
                            pMeshData[i]->boneWeights[vertexIndex][numVertexBones[vertexIndex]] = pAiBone->mWeights[l].mWeight;
                            ++numVertexBones[vertexIndex];
                        } else {
                            int minWeightInd = 0;
                            float minWeight = 1.0f;
                            for (int m = 0; m < 4; ++m) {
                                float weight = pMeshData[i]->boneWeights[vertexIndex][m];
                                if (weight < minWeight) {
                                    minWeight = weight;
                                    minWeightInd = m;
                                }
                            }
                            //size_t oldJointInd = static_cast<size_t>(pMeshData[i]->boneIndices[vertexIndex][minWeightInd]);
                            //std::cout << "removing influence of bone " << oldJointInd << " (name: " << pSkeletonDesc->getJointName(oldJointInd) << " weight: " << minWeight << " position: " << minWeightInd << ")" << std::endl;
                            pMeshData[i]->boneIndices[vertexIndex][minWeightInd] = static_cast<int>(jointIndex);
                            pMeshData[i]->boneWeights[vertexIndex][minWeightInd] = pAiBone->mWeights[l].mWeight;
                        }
                    }
                }

                std::transform(pMeshData[i]->boneWeights.begin(), pMeshData[i]->boneWeights.end(),
                               pMeshData[i]->boneWeights.begin(),
                               [] (const glm::vec4& weights) {
                                    float sum = weights.x + weights.y + weights.z + weights.w;
                                    return (sum > 0.0) ? weights/sum : glm::vec4(0.0);
                               });
            }

            pMeshes[i]->createFromMeshData(pMeshData[i].get());
            pModels[i]->setBoundingSphere(pMeshData[i]->computeBoundingSphere());
            if (pModels[i]->getSkeletonDescription()) {
                pModels[i]->setJointBoundingSpheres(
                    pMeshData[i]->computeJointBoundingSpheres(pModels[i]->getSkeletonDescription()->getNumJoints()));
            }

        }
    }

    if (pAiScene->HasAnimations()) {
        auto& pAnimationClips = pResManager->pAnimationClips;

        for (size_t i = 0; i < pAiScene->mNumAnimations; ++i) {
            const aiAnimation* pAiAnimation = pAiScene->mAnimations[i];
            //std::cout << "Animation: " << pAiAnimation->mName.C_Str() << std::endl;

            for (const auto& pSkeletonDesc : pSkeletonDescriptions) {

                bool hasChannels = false;
                std::vector<bool> includeChannels(pAiAnimation->mNumChannels, false);
                for (size_t j = 0; j < pAiAnimation->mNumChannels; ++j) {
                    aiNodeAnim* pAiChannel = pAiAnimation->mChannels[j];
                    if (pSkeletonDesc->getJointIndex(pAiChannel->mNodeName.C_Str()) != SkeletonDescription::JOINT_NONE) {
                        hasChannels = true;
                        includeChannels[j] = true;
                    }
                }

                if (!hasChannels) continue;

                auto& pClip = pAnimationClips.emplace_back(std::make_unique<AnimationClip>(pSkeletonDesc.get(), pAiAnimation->mName.C_Str()));
                
                pAsset->pAnimationClips.push_back(pClip.get());

                for (size_t j = 0; j < pAiAnimation->mNumChannels; ++j) {
                    if (!includeChannels[j]) continue;
                    aiNodeAnim* pAiChannel = pAiAnimation->mChannels[j];
                    size_t jointIndex = pSkeletonDesc->getJointIndex(pAiChannel->mNodeName.C_Str());
                    const SkeletonDescription::JointInfo& jointInfo = pSkeletonDesc->getJointInfo(jointIndex);

                    glm::quat inverseLocalBindRotation = glm::inverse(jointInfo.bindRotation);
                    glm::mat4 inverseLocalBindMatrix = glm::inverse(jointInfo.bindMatrixLocal);

                    assert(pAiChannel->mNumPositionKeys == pAiChannel->mNumRotationKeys);
                    for (size_t k = 0; k < pAiChannel->mNumRotationKeys; ++k) {
                        const aiQuatKey& rotKey = pAiChannel->mRotationKeys[k];
                        const aiVectorKey& posKey = pAiChannel->mPositionKeys[k];
                        float time = rotKey.mTime;
                        if (pAiAnimation->mTicksPerSecond > 0.0) time /= pAiAnimation->mTicksPerSecond;

                        //Animation::JointTransformSample& sample = animation.createJointTransformSample(jointIndex, time);

                        // The animation system expects the joint transforms to be relative to the joint's bind pose
                        // So we must transform them by the inverse of the local bind pose since they are specified in the parent's local frame

                        pClip->addSample(jointIndex, time,
                            inverseLocalBindRotation * glm::quat(rotKey.mValue.w, rotKey.mValue.x, rotKey.mValue.y, rotKey.mValue.z),
                            glm::vec3(inverseLocalBindMatrix * glm::vec4(posKey.mValue.x, posKey.mValue.y, posKey.mValue.z, 1.0)));

                        //sample.rotation = inverseLocalBindRotation * glm::quat(rotKey.mValue.w, rotKey.mValue.x, rotKey.mValue.y, rotKey.mValue.z);
                        //sample.offset = glm::vec3(inverseLocalBindMatrix * glm::vec4(posKey.mValue.x, posKey.mValue.y, posKey.mValue.z, 1.0));
                    }
                }

            }
        }
    }

    pAsset->pRootNode = buildAssetNodeHierarchy(pAiScene->mRootNode, glm::mat4(1.0f), pAsset.get());
    if (!pAsset->pRootNode) {
        pResManager->pAssets.erase(std::find(pResManager->pAssets.begin(), pResManager->pAssets.end(), pAsset));
        return nullptr;
    }

    return pAsset.get();
}


static AnimationBlendNode* parseBlendTreeJSON(const nlohmann::json& treeJS, AnimationBlendTree* pOutBlendTree) {
    if (auto it = treeJS.find("clip"); it != treeJS.end()) {
        AnimationBlendNodeSingleClip* pNode = pOutBlendTree->createNode<AnimationBlendNodeSingleClip>();
        pNode->setClipName(it->get<std::string>());
        return pNode;
    }

    if (auto it = treeJS.find("lerp"); it != treeJS.end()) {
        AnimationBlendNodeLerp* pNode = pOutBlendTree->createNode<AnimationBlendNodeLerp>();
        pNode->setFirstInput(parseBlendTreeJSON((*it)["first"], pOutBlendTree));
        pNode->setSecondInput(parseBlendTreeJSON((*it)["second"], pOutBlendTree));

        if (auto itP = it->find("parameter"); itP != it->end()) {
            pNode->setFactorParameter(itP->get<std::string>());
        }

        return pNode;
    }

    if (auto it = treeJS.find("blend1D"); it != treeJS.end()) {
        AnimationBlendNode1D* pNode = pOutBlendTree->createNode<AnimationBlendNode1D>();

        for (auto childJS : (*it)["children"]) {
            pNode->addChild(parseBlendTreeJSON(childJS["node"], pOutBlendTree), childJS["position"].get<float>());
        }

        if (auto itP = it->find("parameter"); itP != it->end()) {
            pNode->setFactorParameter(itP->get<std::string>());
        }

        return pNode;
    }

    throw std::runtime_error("No node to parse");
}

AnimationStateGraph* loadAnimationStateGraph(const std::string& fileName, ResourceManager* pResManager) {

    using json = nlohmann::json;
    json animJS;

    try {
        std::ifstream fileStream(fileName);
        fileStream >> animJS;
    } catch (std::exception& e) {
        std::cerr << "Animation State Graph Importer: Error loading JSON: "
                  << e.what()
                  << std::endl;
        return nullptr;
    }

    std::map<std::string, size_t> blendTreeIndexMap;

    auto& pBlendTrees = pResManager->pAnimationBlendTrees;

    for (const json& treeJS : animJS["blendtrees"]) {
        std::string treeID = treeJS["id"].get<std::string>();

        auto pBlendTree = std::make_unique<AnimationBlendTree>();

        try {
            AnimationBlendNode* pBlendTreeRoot = parseBlendTreeJSON(treeJS["node"], pBlendTree.get());
            pBlendTree->setRootNode(pBlendTreeRoot);
            pBlendTree->finalize();

            blendTreeIndexMap[treeID] = pBlendTrees.size();
            pBlendTrees.push_back(std::move(pBlendTree));
        } catch (std::exception& e) {
            std::cerr << "Animation State Graph Importer: Failed to parse Blend Tree: "
                      << e.what()
                      << std::endl;
        }
    }

    auto& pStateGraph = pResManager->pAnimationStateGraphs.emplace_back(std::make_unique<AnimationStateGraph>());
    pStateGraph->setName(std::filesystem::path(fileName).stem());
    std::map<std::string, AnimationStateNode*> pStateMap;

    for (const json& stateJS : animJS["states"]) {
        std::string id = stateJS["id"].get<std::string>();
        if (id == std::string("any")) {
            std::cerr << "Animation State Graph Importer: "
                      << "Keyword 'any' is reserved and cannot be used as a state ID."
                      << std::endl;
            continue;
        }

        AnimationStateNode* pNode = pStateGraph->createNode();

        if (auto itClipJS = stateJS.find("clip"); itClipJS != stateJS.end()) {
            auto& pClipBlendTree = pBlendTrees.emplace_back(std::make_unique<AnimationBlendTree>());

            AnimationBlendNodeSingleClip* pClipNode = pClipBlendTree->createNode<AnimationBlendNodeSingleClip>();

            pClipNode->setClipName(itClipJS->get<std::string>());
            pClipBlendTree->setRootNode(pClipNode);
            pClipBlendTree->finalize();

            pNode->setBlendTree(pClipBlendTree.get());
        } else if (auto itTree = stateJS.find("blendtree-id"); itTree != stateJS.end()) {

            if (auto itTreeInd = blendTreeIndexMap.find(itTree->get<std::string>());
                    itTreeInd != blendTreeIndexMap.end()) {
                pNode->setBlendTree(pBlendTrees[itTreeInd->second].get());
            } else {
                std::cerr << "Animation State Graph Importer: Blend tree not found: "
                          << itTree->get<std::string>()
                          << std::endl;
                continue;
            }
        } else {
            std::cerr << "Animation State Graph Importer: State '"
                      << id
                      << "' does not specify a clip or blend tree id."
                      << std::endl;
            continue;
        }

        if (auto itTSP = stateJS.find("timescale-parameter"); itTSP != stateJS.end()) {
            pNode->setTimeScaleParameter(itTSP->get<std::string>());
        } else if (auto itTS = stateJS.find("timescale"); itTS != stateJS.end()) {
            if (itTS->is_number()) {
                pNode->setTimeScale(itTS->get<float>());
            } else if (itTS->is_string()) {
                std::cerr << "Animation State Graph Importer: Unknown string "
                          << itTS->get<std::string>()
                          << " cannot be used as a timescale. Did you mean timescale-parameter?"
                          << std::endl;
                // No 'continue' here, since this is not a fatal error and we can just use default
            }
        }
        if (auto itTSA = stateJS.find("timescale-absolute"); itTSA != stateJS.end()) {
            pNode->setTimeScaleAbsolute(itTSA->get<bool>());
        }

        if (auto it = stateJS.find("loop"); it != stateJS.end()) {
            pNode->setLoop(it->get<bool>());
        }

        pStateMap[id] = pNode;
        pNode->setName(id);
    }

    for (const json& transitionJS : animJS["transitions"]) {
        AnimationStateNode *pNodeFirst = nullptr, *pNodeSecond = nullptr;

        std::string fromName = transitionJS["from-id"].get<std::string>();
        if (fromName == std::string("any")) {
            pNodeFirst = nullptr;
        } else if (auto itNode = pStateMap.find(fromName); itNode != pStateMap.end()) {
            pNodeFirst = itNode->second;
        } else {
            std::cerr << "Animation State Graph Importer: "
                      << "State not found: "
                      << transitionJS["from-id"].get<std::string>()
                      << " specified as from-id."
                      << std::endl;
            continue;
        }

        if (auto itNode = pStateMap.find(transitionJS["to-id"].get<std::string>()); itNode != pStateMap.end()) {
            pNodeSecond = itNode->second;
        } else {
            std::cerr << "Animation State Graph Importer: "
                      << "State not found: "
                      << transitionJS["to-id"].get<std::string>()
                      << " specified as to-id."
                      << std::endl;
            continue;
        }

        AnimationStateTransition* pTransition = pStateGraph->createTransition(pNodeFirst, pNodeSecond);

        if (auto it = transitionJS.find("trigger-flag"); it != transitionJS.end()) {
            pTransition->setTriggerFlag(it->get<std::string>());
        }

        if (auto it = transitionJS.find("duration"); it != transitionJS.end()) {
            pTransition->setDuration(it->get<float>());
        }

        if (auto it = transitionJS.find("from-time"); it != transitionJS.end()) {
            pTransition->setOutLocalTimelinePosition(it->get<float>());
        }

        if (auto it = transitionJS.find("to-time"); it != transitionJS.end()) {
            pTransition->setInLocalTimelinePosition(it->get<float>());
        }

        if (auto it = transitionJS.find("wait"); it != transitionJS.end()) {
            pTransition->setWaitForOutTime(it->get<bool>());
        }

        if (auto it = transitionJS.find("cancellable"); it != transitionJS.end()) {
            pTransition->setCancellable(it->get<bool>());
        }
    }

    if (auto it = animJS.find("initial-state-id"); it != animJS.end()) {
        std::string stateID = it->get<std::string>();
        const auto& pStates = pStateGraph->getStates();
        if (auto it2 = std::find_if(pStates.begin(), pStates.end(),
                [&stateID] (const AnimationStateNode* pState) {
                    return pState->getName() == stateID;
                }); it2 != pStates.end()) {
            pStateGraph->setInitialState(*it2);
        } else {
            std::cerr << "Animation State Graph Importer: "
                      << "State not found: "
                      << stateID
                      << " specified as initial-state-id."
                      << std::endl;
        }
    }

    pStateGraph->finalize();

    return pStateGraph.get();
}
