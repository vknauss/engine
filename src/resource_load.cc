#include "resource_load.h"

#include <iostream>
#include <map>
#include <vector>
#include <queue>

#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>

#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <assimp/pbrmaterial.h>

#include "mesh_builder.h"


void load_texture(const std::string& texturePath, Texture* texture) {
    int w, h, n;
    unsigned char* data = stbi_load(texturePath.c_str(), &w, &h, &n, 0);
    if(data) {
        TextureParameters parameters = {};
        parameters.arrayLayers = 1;
        parameters.numComponents = n;
        parameters.width = w;
        parameters.height = h;
        parameters.useFloatComponents = false;
        parameters.useEdgeClamping = false;
        parameters.useLinearFiltering = true;
        parameters.useMipmapFiltering = true;
        parameters.useAnisotropicFiltering = true;

        texture->setParameters(parameters);
        texture->allocateData(data);
        texture->generateMipmaps();

        stbi_image_free(data);
    } else {
        std::cerr << "ERROR: Failed to load texture from: " << texturePath << std::endl;
    }
}

void load_texture(const std::string& texturePath, Texture* texture, TextureParameters parameters) {
    int w, h, n;
    unsigned char* data = stbi_load(texturePath.c_str(), &w, &h, &n, 0);
    if(data) {
        parameters.arrayLayers = 1;
        parameters.numComponents = n;
        parameters.width = w;
        parameters.height = h;

        texture->setParameters(parameters);
        texture->allocateData(data);
        if(parameters.useMipmapFiltering) texture->generateMipmaps();

        stbi_image_free(data);
    } else {
        std::cerr << "ERROR: Failed to load texture from: " << texturePath << std::endl;
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

void load_pose(std::string fname, Animation* animation, float sampleTime) {
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
}

void load_animation(std::string fname, Animation* animation) {
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
}

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

void load_obj_multi(std::string fname, std::vector<MeshData>& meshes, std::vector<Material>& materials, std::vector<int>& materialIDs, std::vector<Texture*>& textures) {
    std::string baseDir;
    std::istringstream pss(fname);
    std::string tok;
    std::vector<std::string> toks;
    while(std::getline(pss, tok, '/')) toks.push_back(tok);
    for(size_t i = 0; i < toks.size()-1; ++i) baseDir += toks[i] + "/";

    tinyobj::ObjReaderConfig tObjConfig;
    tObjConfig.triangulate = true;
    tObjConfig.vertex_color = false;
    tObjConfig.mtl_search_path = baseDir;

    tinyobj::ObjReader tObjReader;
    tObjReader.ParseFromFile(fname, tObjConfig);

    if(!tObjReader.Valid()) {
        std::cerr << "Couldn't load OBJ from " << fname << std::endl;
        std::cerr << "TinyObjLoader warning string: " << tObjReader.Warning() << std::endl;
        std::cerr << "TinyObjLoader error string: " << tObjReader.Error() << std::endl;
        return;
    }

    const tinyobj::attrib_t& attrib = tObjReader.GetAttrib();
    const std::vector<tinyobj::shape_t>& shapes = tObjReader.GetShapes();
    const std::vector<tinyobj::material_t>& mtls = tObjReader.GetMaterials();

    std::cout << "TinyOBJ loaded " << (shapes.size()) << " meshes and " << (mtls.size()) << " materials." << std::endl;

    size_t numVertices = attrib.vertices.size();
    size_t numNormals = attrib.normals.size();
    size_t numTexCoords = attrib.texcoords.size();

    int vertexBits = (int) std::ceil(std::log2((double) numVertices));
    int normalBits = (int) std::ceil(std::log2((double) numNormals));
    int texCoordBits = (int) std::ceil(std::log2((double) numTexCoords));
    if(vertexBits + normalBits + texCoordBits > 64) {
        std::cerr << "Not enough bits for key generation! Model loading will go ahead but results may be unexpected." << std::endl;
    }

    std::map<std::string, Texture*> textureMap;

    materials.resize(mtls.size());
    std::vector<bool> needsTBN(mtls.size(), false);

    for(size_t i = 0; i < mtls.size(); ++i) {
        const tinyobj::material_t& mtl = mtls[i];
        Material& material = materials[i];
        material.setTintColor(glm::vec3(mtl.diffuse[0], mtl.diffuse[1], mtl.diffuse[2]));
        material.setRoughness(std::sqrt(1.0f / std::log((float) mtl.shininess)));

        if(!mtl.diffuse_texname.empty()) {
            Texture* tex = nullptr;
            if(auto kv = textureMap.find(mtl.diffuse_texname); kv != textureMap.end()) {
                tex = kv->second;
            } else {
                tex = new Texture();
                std::string path = (mtl.diffuse_texname[0] == '/') ? mtl.diffuse_texname : baseDir + mtl.diffuse_texname;
                load_texture(path, tex);
                textures.push_back(tex);
                textureMap.insert(std::make_pair(mtl.diffuse_texname, tex));
            }
            material.setDiffuseTexture(tex);
        }

        std::string normal_texname;
        bool doNormal = false;
        if(!mtl.normal_texname.empty()) {
            normal_texname = mtl.normal_texname;
            doNormal = true;
        } else if(!mtl.displacement_texname.empty()) {
            normal_texname = mtl.displacement_texname;
            doNormal = true;
        } else if(!mtl.bump_texname.empty()) {
            normal_texname = mtl.bump_texname;
            doNormal = true;
        }
        if(doNormal) {
            Texture* tex = nullptr;
            if(auto kv = textureMap.find(normal_texname); kv != textureMap.end()) {
                tex = kv->second;
            } else {
                tex = new Texture();
                std::string path = (normal_texname[0] == '/') ? normal_texname : baseDir + normal_texname;
                load_texture(path, tex);
                textures.push_back(tex);
                textureMap.insert(std::make_pair(normal_texname, tex));
            }
            material.setNormalsTexture(tex);
            needsTBN[i] = true;
        }
    }

    for(size_t i = 0; i < shapes.size(); ++i) {
        const tinyobj::shape_t& shape = shapes[i];

        std::vector<std::map<size_t, int>> subMeshIndexMaps;
        std::map<int, size_t> materialMap;
        std::vector<int> subMeshMaterialIDs;
        std::vector<MeshData> subMeshes;

        for(size_t j = 0; j < shape.mesh.indices.size(); j += 3) {
            int materialID = shape.mesh.material_ids[j/3];
            size_t subMeshID;
            if(auto kv = materialMap.find(materialID); kv != materialMap.end()) {
                subMeshID = kv->second;
            } else {
                subMeshID = subMeshes.size();
                subMeshIndexMaps.push_back(std::map<size_t, int>());
                subMeshes.push_back(MeshData());
                subMeshMaterialIDs.push_back(materialID);
                materialMap.insert(std::make_pair(materialID, subMeshID));
            }

            MeshData& subMesh = subMeshes[subMeshID];
            std::map<size_t, int>& indexMap = subMeshIndexMaps[subMeshID];

            for(size_t k = 0; k < 3; ++k) {
                const tinyobj::index_t& index = shape.mesh.indices[j+k];
                size_t key = std::max(index.vertex_index, 0);
                key = key << normalBits;
                key = key | std::max(index.normal_index, 0);
                key = key << texCoordBits;
                key = key | std::max(index.texcoord_index, 0);

                if(auto it = indexMap.find(key); it != indexMap.end()) {
                    subMesh.indices.push_back(it->second);
                } else {
                    size_t ind = subMesh.vertices.size();
                    indexMap.insert(std::make_pair(key, ind));
                    if(index.vertex_index >= 0)
                        subMesh.vertices.push_back(
                            glm::vec3(  attrib.vertices[3*index.vertex_index],
                                        attrib.vertices[3*index.vertex_index+1],
                                        attrib.vertices[3*index.vertex_index+2]));
                    if(index.normal_index >= 0)
                        subMesh.normals.push_back(
                            glm::vec3(  attrib.normals[3*index.normal_index],
                                        attrib.normals[3*index.normal_index+1],
                                        attrib.normals[3*index.normal_index+2]));
                    if(index.texcoord_index >= 0)
                        subMesh.uvs.push_back(
                            glm::vec2(  attrib.texcoords[2*index.texcoord_index],
                                        attrib.texcoords[2*index.texcoord_index+1]));
                    subMesh.indices.push_back(ind);
                }
            }
        }
        for(size_t j = 0; j < subMeshes.size(); ++j) {
            if(needsTBN[subMeshMaterialIDs[j]]) MeshBuilder(std::move(subMeshes[j])).calculateTangentSpace().moveMeshData(subMeshes[j]);
        }

        meshes.insert(meshes.end(), subMeshes.begin(), subMeshes.end());
        materialIDs.insert(materialIDs.end(), subMeshMaterialIDs.begin(), subMeshMaterialIDs.end());
    }
}

void load_model(std::string fname, MeshData& md) {
    std::ifstream file_stream;
    file_stream.open(fname);

    int v_count = 0;
    while(!file_stream.eof()) {

        // get the v/vt/vn etc

        std::string fn;
        file_stream >> fn;
        if(fn.length() == 0) continue;
        switch(fn[0]) {
        case 'v':
            if(fn.length() == 1) {
                // vertex
                float x, y, z;
                file_stream >> x >> y >> z;
                md.vertices.push_back({x, z, -y});
                v_count++;
            } else {
                switch(fn[1]) {
                    case 'n':
                    // normal
                    if(true) {
                        float x, y, z;
                        file_stream >> x >> y >> z;
                        md.normals.push_back({x, z, -y});
                    }
                    break;
                    case 't':
                    //texcoord
                    if(true) {
                        float u, v;
                        file_stream >> u >> v;
                        md.uvs.push_back({u, v});
                    }
                    break;
                    case 'i':
                    //bone index
                    if(true) {
                        int i0, i1, i2, i3;
                        file_stream >> i0 >> i1 >> i2 >> i3;
                        md.boneIndices.push_back({i0, i1, i2, i3});
                    }
                    break;
                    case 'w':
                    // bone weights
                    if(true) {
                        float w0, w1, w2, w3;
                        file_stream >> w0 >> w1 >> w2 >> w3;
                        md.boneWeights.push_back({w0, w1, w2, w3});
                    }
                    break;
                    default:
                        std::cout << "Unknown token '" << fn << "'" << std::endl;
                }
            }
            break;
        case 'f':
            // face
            if(true) {
                uint32_t v0, v1, v2;
                file_stream >> v0 >> v1 >> v2;
                md.indices.insert(md.indices.end(), {v0, v1, v2});
            }
            break;
        default:
            std::cout << "Unknown token '" << fn << "'" << std::endl;
        }
    }

    file_stream.close();
}

Texture* loadTextureAssimp(const aiString& texPath, const aiScene* pAiScene, TextureParameters param) {
    Texture* pTexture = nullptr;

    param.arrayLayers = 1;
    param.samples = 1;

    const aiTexture* pAiTex = pAiScene->GetEmbeddedTexture(texPath.C_Str());

    void* pStbData = nullptr;
    int width, height, components;

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

            pTexture = new Texture();
            pTexture->setParameters(param);
            pTexture->allocateData(static_cast<void*>(pAiTex->pcData));
            pTexture->generateMipmaps();
        }
    } else {
        if (param.useFloatComponents) {
            param.bitsPerComponent = 16;
            pStbData = static_cast<void*>(stbi_loadf(texPath.C_Str(), &width, &height, &components, 0));
        } else {
            param.bitsPerComponent = 8;
            pStbData = static_cast<void*>(stbi_load(texPath.C_Str(), &width, &height, &components, 0));
        }
    }

    if (!pTexture) {
        if (pStbData) {
            param.width = width;
            param.height = height;
            param.numComponents = components;

            pTexture = new Texture();
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
    return pTexture;
}

    //#include <glm/gtx/string_cast.hpp>
void buildSkeletonDescriptionAssimp(const aiNode* pAiNode, size_t parentIndex, SkeletonDescription& skeletonDesc, std::map<const aiNode*, size_t>& nodeJointIndices) {
    size_t jointIndex = skeletonDesc.createJoint(pAiNode->mName.C_Str());
    nodeJointIndices[pAiNode] = jointIndex;
    skeletonDesc.setParent(jointIndex, parentIndex);

    const aiMatrix4x4& aiNodeMatrix = pAiNode->mTransformation;
    glm::mat4 bindMatrixLocal;
    for (int i = 0; i < 4; ++i) for (int j = 0; j < 4; ++j) {
        bindMatrixLocal[i][j] = aiNodeMatrix[j][i];  // transposed
    }

    aiQuaternion aiNodeRotation;
    aiVector3D aiNodeOffset;
    aiNodeMatrix.DecomposeNoScaling(aiNodeRotation, aiNodeOffset);

    //skeletonDesc.setBindRotation(jointIndex, glm::quat(aiNodeRotation.w, aiNodeRotation.x, aiNodeRotation.y, aiNodeRotation.z));
    //skeletonDesc.setBindOffset(jointIndex, glm::vec3(aiNodeOffset.x, aiNodeOffset.y, aiNodeOffset.z));

    /*{
        std::cout << "Matrix (Joint " << jointIndex << "): " << std::endl;
        std::cout << glm::to_string(bindMatrixLocal[0]) << std::endl;
        std::cout << glm::to_string(bindMatrixLocal[1]) << std::endl;
        std::cout << glm::to_string(bindMatrixLocal[2]) << std::endl;
        std::cout << glm::to_string(bindMatrixLocal[3]) << std::endl;
    }*/

    //skeletonDesc.setBindMatrix(jointIndex, glm::inverse(bindMatrixLocal));
    //skeletonDesc.setBindMatrix(jointIndex, bindMatrixLocal);
    for (size_t i = 0; i < pAiNode->mNumChildren; ++i) {
        buildSkeletonDescriptionAssimp(pAiNode->mChildren[i], jointIndex, skeletonDesc, nodeJointIndices);
    }
}

void importAssimp(
    const std::string& fileName,
    std::vector<MeshData>& meshes,
    std::vector<Material>& materials,
    std::vector<int>& materialIDs,
    std::vector<Texture*>& textures,
    std::vector<SkeletonDescription>& skeletonDescriptions,
    std::vector<int>& skeletonDescIndices,
    std::vector<Animation>& animations)
{
    Assimp::Importer importer;

    const aiScene* pAiScene = importer.ReadFile(fileName,
        aiProcess_CalcTangentSpace  |
        aiProcess_Triangulate   |
        aiProcess_JoinIdenticalVertices |
        aiProcess_SortByPType |
        aiProcess_FixInfacingNormals |
        aiProcess_GenNormals |
        aiProcess_PopulateArmatureData);

    if (!pAiScene) {
        std::cerr << "Assimp error: " << importer.GetErrorString() << std::endl;
        return;
    }

    if (pAiScene->HasMaterials()) {
        materials.resize(pAiScene->mNumMaterials);
        for (size_t i = 0; i < materials.size(); ++i) {
            const aiMaterial* pAiMat = pAiScene->mMaterials[i];

            aiColor3D diffuseColor;
            if (pAiMat->Get(AI_MATKEY_COLOR_DIFFUSE, diffuseColor) == AI_SUCCESS) {
                materials[i].setTintColor({diffuseColor.r, diffuseColor.g, diffuseColor.b});
            }

            aiColor3D emissionColor;
            if (pAiMat->Get(AI_MATKEY_COLOR_EMISSIVE, emissionColor) == AI_SUCCESS) {
                materials[i].setEmissionIntensity({emissionColor.r, emissionColor.g, emissionColor.b});
            }

            float metallic;
            if (pAiMat->Get(AI_MATKEY_GLTF_PBRMETALLICROUGHNESS_METALLIC_FACTOR, metallic) == AI_SUCCESS) {
                materials[i].setMetallic(metallic);
            }

            float roughness;
            if (pAiMat->Get(AI_MATKEY_GLTF_PBRMETALLICROUGHNESS_ROUGHNESS_FACTOR, roughness) == AI_SUCCESS) {
                materials[i].setRoughness(roughness);
            }

            aiString diffuseTexPath;
            if (pAiMat->Get(AI_MATKEY_TEXTURE_DIFFUSE(0), diffuseTexPath) == AI_SUCCESS) {
                TextureParameters param = {};
                param.useAnisotropicFiltering = true;
                param.useEdgeClamping = true;
                param.useLinearFiltering = true;
                param.useMipmapFiltering = true;
                Texture* pDiffuseTexture = loadTextureAssimp(diffuseTexPath, pAiScene, param);
                if (pDiffuseTexture) {
                    textures.push_back(pDiffuseTexture);
                    materials[i].setDiffuseTexture(pDiffuseTexture);
                }
            }

            aiString normalTexPath;
            if (pAiMat->Get(AI_MATKEY_TEXTURE_NORMALS(0), normalTexPath) == AI_SUCCESS) {
                TextureParameters param = {};
                param.useFloatComponents = false;
                param.useEdgeClamping = true;
                param.useLinearFiltering = true;
                param.useMipmapFiltering = false;
                Texture* pNormalTexture = loadTextureAssimp(normalTexPath, pAiScene, param);
                if (pNormalTexture) {
                    textures.push_back(pNormalTexture);
                    materials[i].setNormalsTexture(pNormalTexture);
                }
            }

            aiString emissionTexPath;
            if (pAiMat->Get(AI_MATKEY_TEXTURE_EMISSIVE(0), emissionTexPath) == AI_SUCCESS) {
                TextureParameters param = {};
                param.useFloatComponents = true;
                param.useEdgeClamping = true;
                param.useLinearFiltering = true;
                Texture* pEmissionTexture = loadTextureAssimp(emissionTexPath, pAiScene, param);
                if (pEmissionTexture) {
                    textures.push_back(pEmissionTexture);
                    materials[i].setEmissionTexture(pEmissionTexture);
                }
            }

            aiString metallicRoughnessTexPath;
            if (pAiMat->GetTexture(AI_MATKEY_GLTF_PBRMETALLICROUGHNESS_METALLICROUGHNESS_TEXTURE, &metallicRoughnessTexPath) == AI_SUCCESS) {
                TextureParameters param = {};
                param.useFloatComponents = true;
                param.useEdgeClamping = true;
                param.useLinearFiltering = true;
                Texture* pMetallicRoughnessTexture = loadTextureAssimp(metallicRoughnessTexPath, pAiScene, param);
                if (pMetallicRoughnessTexture) {
                    textures.push_back(pMetallicRoughnessTexture);
                    materials[i].setMetallicRoughnessTexture(pMetallicRoughnessTexture);
                }
            }
        }
    }

    if (pAiScene->HasMeshes()) {
        meshes.resize(pAiScene->mNumMeshes);
        materialIDs.resize(pAiScene->mNumMeshes, static_cast<int>(materials.size()));
        skeletonDescIndices.resize(pAiScene->mNumMeshes, -1);
        int c_skeletonIndex = 0;
        for (size_t i = 0; i < meshes.size(); ++i) {
            const aiMesh* pAiMesh = pAiScene->mMeshes[i];

            meshes[i].vertices.resize(pAiMesh->mNumVertices);
            memcpy(meshes[i].vertices.data(), &pAiMesh->mVertices[0], 3*sizeof(float)*pAiMesh->mNumVertices);

            meshes[i].normals.resize(pAiMesh->mNumVertices);
            memcpy(meshes[i].normals.data(), &pAiMesh->mNormals[0], 3*sizeof(float)*pAiMesh->mNumVertices);

            meshes[i].indices.resize(pAiMesh->mNumFaces*3);
            for (size_t j = 0; j < pAiMesh->mNumFaces; ++j) {
                memcpy(&meshes[i].indices[3*j], pAiMesh->mFaces[j].mIndices, 3*sizeof(unsigned int));
            }

            if (pAiMesh->HasTextureCoords(0)) {
                meshes[i].uvs.resize(pAiMesh->mNumVertices);
                std::transform(pAiMesh->mTextureCoords[0], pAiMesh->mTextureCoords[0]+pAiMesh->mNumVertices,
                               meshes[i].uvs.begin(),
                               [] (const aiVector3D& v) { return glm::vec2(v.x, v.y); });
            }

            if (pAiMesh->HasTangentsAndBitangents()) {
                meshes[i].tangents.resize(pAiMesh->mNumVertices);
                meshes[i].bitangents.resize(pAiMesh->mNumVertices);
                memcpy(meshes[i].tangents.data(), &pAiMesh->mTangents[0], 3*sizeof(float)*pAiMesh->mNumVertices);
                memcpy(meshes[i].bitangents.data(), &pAiMesh->mBitangents[0], 3*sizeof(float)*pAiMesh->mNumVertices);
            }

            if (pAiMesh->mMaterialIndex < pAiScene->mNumMaterials) {
                materialIDs[i] = static_cast<int>(pAiMesh->mMaterialIndex);
            }

            if (pAiMesh->HasBones()) {
                skeletonDescIndices[i] = c_skeletonIndex++;

                meshes[i].boneIndices.resize(pAiMesh->mNumVertices, glm::ivec4(0));
                meshes[i].boneWeights.resize(pAiMesh->mNumVertices, glm::vec4(0.0));

                std::vector<int> numVertexBones(pAiMesh->mNumVertices, 0);

                skeletonDescriptions.emplace_back();
                SkeletonDescription& skeletonDesc = skeletonDescriptions.back();

                std::map<const aiNode*, size_t> nodeJointIndices;

                buildSkeletonDescriptionAssimp(pAiMesh->mBones[0]->mArmature, SkeletonDescription::JOINT_NONE, skeletonDesc, nodeJointIndices);
                if (!skeletonDesc.validate()) {
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

                    skeletonDesc.setBindMatrix(jointIndex, glm::inverse(bindMatrix));
                }
                skeletonDesc.computeBindTransforms();

                for (size_t k = 0; k < pAiMesh->mNumBones; ++k) {
                    const aiBone* pAiBone = pAiMesh->mBones[k];
                    size_t jointIndex = nodeJointIndices[pAiBone->mNode];

                    for (size_t l = 0; l < pAiBone->mNumWeights; ++l) {
                        size_t vertexIndex = pAiBone->mWeights[l].mVertexId;
                        if (numVertexBones[vertexIndex] < 4) {
                            meshes[i].boneIndices[vertexIndex][numVertexBones[vertexIndex]] = static_cast<int>(jointIndex);
                            meshes[i].boneWeights[vertexIndex][numVertexBones[vertexIndex]] = pAiBone->mWeights[l].mWeight;
                            ++numVertexBones[vertexIndex];
                        } else {
                            int minWeightInd = 0;
                            float minWeight = 1.0f;
                            for (int m = 0; m < 4; ++m) {
                                float weight = meshes[i].boneWeights[vertexIndex][m];
                                if (weight < minWeight) {
                                    minWeight = weight;
                                    minWeightInd = m;
                                }
                            }
                            meshes[i].boneIndices[vertexIndex][minWeightInd] = static_cast<int>(jointIndex);
                            meshes[i].boneWeights[vertexIndex][minWeightInd] = pAiBone->mWeights[l].mWeight;
                        }
                    }
                }

                std::transform(meshes[i].boneWeights.begin(), meshes[i].boneWeights.end(),
                               meshes[i].boneWeights.begin(),
                               [] (const glm::vec4& weights) {
                                    float sum = weights.x + weights.y + weights.z + weights.w;
                                    return (sum > 0.0) ? weights/sum : glm::vec4(0.0);
                               });
            }
        }
    }

    if (pAiScene->HasAnimations()) {
        for (size_t i = 0; i < pAiScene->mNumAnimations; ++i) {
            const aiAnimation* pAiAnimation = pAiScene->mAnimations[i];
            std::cout << pAiAnimation->mName.C_Str() << std::endl;

            for (const SkeletonDescription& skeletonDesc : skeletonDescriptions) {

                bool hasChannels = false;
                std::vector<bool> includeChannels(pAiAnimation->mNumChannels, false);
                for (size_t j = 0; j < pAiAnimation->mNumChannels; ++j) {
                    aiNodeAnim* pAiChannel = pAiAnimation->mChannels[j];
                    if (skeletonDesc.getJointIndex(pAiChannel->mNodeName.C_Str()) != SkeletonDescription::JOINT_NONE) {
                        hasChannels = true;
                        includeChannels[j] = true;
                    }
                }

                if (!hasChannels) continue;

                Animation& animation = animations.emplace_back(&skeletonDesc);

                for (size_t j = 0; j < pAiAnimation->mNumChannels; ++j) {
                    if (!includeChannels[j]) continue;
                    aiNodeAnim* pAiChannel = pAiAnimation->mChannels[j];
                    size_t jointIndex = skeletonDesc.getJointIndex(pAiChannel->mNodeName.C_Str());

                    glm::quat inverseLocalBindRotation = glm::inverse(skeletonDesc.getJointBindRotation(jointIndex));
                    glm::mat4 inverseLocalBindMatrix = glm::inverse(skeletonDesc.getJointBindMatrixLocal(jointIndex));

                    assert(pAiChannel->mNumPositionKeys == pAiChannel->mNumRotationKeys);
                    for (size_t k = 0; k < pAiChannel->mNumRotationKeys; ++k) {
                        const aiQuatKey& rotKey = pAiChannel->mRotationKeys[k];
                        const aiVectorKey& posKey = pAiChannel->mPositionKeys[k];
                        float time = rotKey.mTime;
                        if (pAiAnimation->mTicksPerSecond > 0.0) time /= pAiAnimation->mTicksPerSecond;

                        Animation::JointTransformSample& sample = animation.createJointTransformSample(jointIndex, time);
                        // The animation system expects the joint transforms to be relative to the joint's bind pose
                        // So we must transform them by the inverse of the local bind pose since they are specified in the parent's local frame
                        sample.rotation = inverseLocalBindRotation * glm::quat(rotKey.mValue.w, rotKey.mValue.x, rotKey.mValue.y, rotKey.mValue.z);
                        sample.offset = glm::vec3(inverseLocalBindMatrix * glm::vec4(posKey.mValue.x, posKey.mValue.y, posKey.mValue.z, 1.0));
                    }
                }

            }
        }
    }
}
