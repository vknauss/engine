#include "mesh_data.h"

#include <algorithm>
#include <numeric>
#include <list>


MeshData::MeshData() :
    vertices(),
    normals(),
    tangents(),
    bitangents(),
    uvs(),
    boneWeights(),
    boneIndices(),
    indices() {
}

MeshData::MeshData(const MeshData& md) :
    vertices(md.vertices),
    normals(md.normals),
    tangents(md.tangents),
    bitangents(md.bitangents),
    uvs(md.uvs),
    boneWeights(md.boneWeights),
    boneIndices(md.boneIndices),
    indices(md.indices) {
}

MeshData::MeshData(MeshData&& md) :
    vertices(std::move(md.vertices)),
    normals(std::move(md.normals)),
    tangents(std::move(md.tangents)),
    bitangents(std::move(md.bitangents)),
    uvs(std::move(md.uvs)),
    boneWeights(std::move(md.boneWeights)),
    boneIndices(std::move(md.boneIndices)),
    indices(std::move(md.indices)) {
}

// Ritter's bounding sphere algorithm. Sub-optimal but simple
BoundingSphere MeshData::computeBoundingSphere() const {
    BoundingSphere bounds;

    if (vertices.empty()) return bounds;
    if (getNumVertices() == 1) return bounds.setPosition(vertices[0]);

    auto it = std::max_element(vertices.begin()+1, vertices.end(),
                               [v=vertices[0]] (const glm::vec3& v0, const glm::vec3& v1) {
                                   return glm::length(v0-v) < glm::length(v1-v); });

    bounds
        .setPosition((vertices[0] + *it) / 2.0f)
        .setRadius(glm::length(vertices[0] - *it));


    while ((it = std::find_if(vertices.begin()+1, vertices.end(),
                                   [b=bounds] (const glm::vec3& v) {
                                       return glm::length(v-b.getPosition()) > b.getRadius(); })
           ) != vertices.end())
    {
        float dist = glm::length(bounds.getPosition() - *it);

        bounds
            .setPosition(glm::mix(*it, bounds.getPosition(), bounds.getRadius()/dist))
            .setRadius((bounds.getRadius() + dist) / 2.0f);
    }

    return bounds;
}

std::vector<BoundingSphere> MeshData::computeJointBoundingSpheres(size_t numJoints) const {
    assert(hasBoneIndices() && hasBoneWeights());

    std::vector<BoundingSphere> bounds(numJoints);
    std::vector<std::list<size_t>> jointVertices(numJoints, std::list<size_t>());

    for (size_t i = 0; i < vertices.size(); ++i) {
        for (int j = 0; j < 4; ++j) {
            if (boneWeights[i][j] > 0.0001) {
                jointVertices[boneIndices[i][j]].push_back(i);
            }
        }
    }

    for (size_t i = 0; i < numJoints; ++i) {
        if (jointVertices[i].empty()) continue;
        if (jointVertices[i].size() == 1) {
            bounds[i].setPosition(vertices[jointVertices[i].front()]);
            continue;
        }

        auto it = std::max_element(std::next(jointVertices[i].begin()), jointVertices[i].end(),
                               [&v=vertices[jointVertices[i].front()], this] (size_t i0, size_t i1) {
                                   return glm::length(vertices[i0]-v) < glm::length(vertices[i1]-v); });

        bounds[i]
            .setPosition((vertices[jointVertices[i].front()] + vertices[*it]) / 2.0f)
            .setRadius(glm::length(vertices[jointVertices[i].front()] - vertices[*it]));


        while ((it = std::find_if(std::next(jointVertices[i].begin()), jointVertices[i].end(),
                                       [&b=bounds[i], this] (size_t i0) {
                                           return glm::length(vertices[i0]-b.getPosition()) > b.getRadius(); })
               ) != jointVertices[i].end())
        {
            float dist = glm::length(bounds[i].getPosition() - vertices[*it]);

            bounds[i]
                .setPosition(glm::mix(vertices[*it], bounds[i].getPosition(), bounds[i].getRadius()/dist))
                .setRadius((bounds[i].getRadius() + dist) / 2.0f);
        }

    }

    return bounds;
}

glm::vec3 MeshData::computeCentroid() const {
    if (vertices.empty()) return glm::vec3(0.0f);

    glm::vec3 sum = std::accumulate(vertices.begin(), vertices.end(), glm::vec3(0.0f));

    return sum / static_cast<float>(getNumVertices());
}

MeshData& MeshData::operator=(const MeshData& md) {
    vertices = md.vertices;
    normals = md.normals;
    tangents = md.tangents;
    bitangents = md.bitangents;
    uvs = md.uvs;
    boneWeights = md.boneWeights;
    boneIndices = md.boneIndices;
    indices = md.indices;

    return *this;
}

MeshData& MeshData::operator=(MeshData&& md) {
    vertices = std::move(md.vertices);
    normals = std::move(md.normals);
    tangents = std::move(md.tangents);
    bitangents = std::move(md.bitangents);
    uvs = std::move(md.uvs);
    boneWeights = std::move(md.boneWeights);
    boneIndices = std::move(md.boneIndices);
    indices = std::move(md.indices);

    return *this;
}
