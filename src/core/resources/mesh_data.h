#ifndef MESH_DATA_H_
#define MESH_DATA_H_

#include <glm/glm.hpp>

#include <map>
#include <string>
#include <vector>

#include "core/scene/bounding_sphere.h"

// Simple container type that allows editing of procedural meshes
// and keeps them formatted in a way that allows easy transfer to OpenGL VAOs

// operations on this type are defined in the mesh_util namspace
// see mesh_util.h

class MeshData {

public:

    std::vector<glm::vec3> vertices;
    std::vector<glm::vec3> normals;
    std::vector<glm::vec3> tangents;
    std::vector<glm::vec3> bitangents;
    std::vector<glm::vec2> uvs;
    std::vector<glm::vec4> boneWeights;
    std::vector<glm::ivec4> boneIndices;
    std::vector<uint32_t> indices;


    MeshData();

    MeshData(const MeshData& md);

    MeshData(MeshData&& md);

    size_t getNumVertices() const {
        return vertices.size();
    }

    bool hasNormals() const {
        return !vertices.empty() && normals.size() == vertices.size();

    }

    bool hasTangents() const {
        return !vertices.empty() && tangents.size() == vertices.size();
    }

    bool hasBitangents() const {
        return !vertices.empty() && bitangents.size() == vertices.size();
    }

    bool hasUVs() const {
        return !vertices.empty() && uvs.size() == vertices.size();
    }

    bool hasBoneWeights() const {
        return !vertices.empty() && boneWeights.size() == vertices.size();
    }

    bool hasBoneIndices() const {
        return !vertices.empty() && boneIndices.size() == vertices.size();
    }

    BoundingSphere computeBoundingSphere() const;

    std::vector<BoundingSphere> computeJointBoundingSpheres(size_t numJoints) const;

    glm::vec3 computeCentroid() const;

    MeshData& operator=(const MeshData& md);

    MeshData& operator=(MeshData&& md);
};


#endif // MESH_DATA_H_
