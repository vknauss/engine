#ifndef MESH_BUILDER_H_INCLUDED
#define MESH_BUILDER_H_INCLUDED

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include "mesh_data.h"
#include "bounding_sphere.h"

class MeshBuilder {

    MeshData m_meshData;

public:

    MeshBuilder();

    explicit MeshBuilder(const MeshData& md);

    explicit MeshBuilder(MeshData&& md);

    MeshBuilder(const MeshBuilder& m) = delete;

    const MeshData& getMeshData() const {
        return m_meshData;
    }

    void moveMeshData(MeshData& md);
    MeshData&& moveMeshData();

    MeshBuilder& add(const MeshData& m);

    MeshBuilder& calculateNormals();

    MeshBuilder& calculateTangentSpace();

    MeshBuilder& translate(const glm::vec3& t);

    MeshBuilder& rotate(const glm::quat& q);

    MeshBuilder& rotate(const glm::mat3& m);

    MeshBuilder& scale(const glm::vec3& v);

    MeshBuilder& scale(float s);

    MeshBuilder& transform(const glm::mat4& m);

    MeshBuilder& cylinder(float radius, float height, int circlePoints, int segments, bool makeClosed = true);

    MeshBuilder& hemisphere(float radius, int circlePoints, int rings, bool makeClosed = true);

    MeshBuilder& sphere(float radius, int circlePoints, int rings);

    MeshBuilder& capsule(float radius, float height, int circlePoints, int segments, int hemisphereRings);

    MeshBuilder& plane(float width);

    MeshBuilder& cube(float width);

    MeshBuilder& uvMapPlanar(const glm::vec3& normal = {0, 1, 0});

};

#endif // MESH_BUILDER_H_INCLUDED
