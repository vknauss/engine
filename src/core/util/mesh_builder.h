#ifndef MESH_BUILDER_H_INCLUDED
#define MESH_BUILDER_H_INCLUDED

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include "core/resources/mesh_data.h"
#include "core/scene/bounding_sphere.h"

class MeshBuilderNodes {
public:

    enum Operation {
        ADD,
        CALC_NORMALS,
        CALC_TANGENT,
        TRANSLATE,
        ROTATE,
        SCALE,
        PRIM_CYLINDER,
        PRIM_HEMI,
        PRIM_SPHERE,
        PRIM_CAPSULE,
        PRIM_PLANE,
        PRIM_CUBE,
        UV_PLANAR
    };

    struct Node {
        Operation operation;
        Node* pNext;
        Node* pPrev = nullptr;

        Node(Operation operation, Node* pNext) :
            operation(operation),
            pNext(pNext) {
            if (pNext) pNext->pPrev = this;
        }

        virtual ~Node() {
            if (pNext) delete pNext;
        }
    };

    struct Add : public Node {
        Node* pChild;

        Add(Node* pNext, Node* pNode = nullptr) :
            Node(ADD, pNext),
            pChild(pNode) {
        }

        ~Add() {
            delete pChild;
        }
    };

    struct CalcNormals : public Node {
        CalcNormals(Node* pNext) :
            Node(CALC_NORMALS, pNext) {
        }
    };

    struct CalcTangent : public Node {
        CalcTangent(Node* pNext) :
            Node(CALC_TANGENT, pNext) {
        }
    };

    struct Translate : public Node {
        glm::vec3 translation;

        Translate(Node* pNext, const glm::vec3& v = {0, 0, 0}) :
            Node(TRANSLATE, pNext),
            translation(v) {
        }
    };

    struct Rotate : public Node {
        glm::quat rotation;

        Rotate(Node* pNext, const glm::quat& q = {0, 0, 0, 1}) :
            Node(ROTATE, pNext),
            rotation(q) {
        }
    };

    struct Scale : public Node {
        glm::vec3 scale;

        Scale(Node* pNext, const glm::vec3& v = {1, 1, 1}) :
            Node(SCALE, pNext),
            scale(v) {
        }
    };

    struct PrimCylinder : public Node {
        float radius;
        float height;
        int circlePoints;
        int segments;
        bool makeClosed;

        PrimCylinder(Node* pNext, float radius = 0.5, float height = 1.0, int circlePoints = 32, int segments = 1, bool makeClosed = true) :
            Node(PRIM_CYLINDER, pNext),
            radius(radius),
            height(height),
            circlePoints(circlePoints),
            segments(segments),
            makeClosed(makeClosed) {
        }
    };

    struct PrimHemisphere : public Node {
        float radius;
        int circlePoints;
        int rings;
        bool makeClosed;

        PrimHemisphere(Node* pNext, float radius = 0.5, int circlePoints = 32, int rings = 8, bool makeClosed = true) :
            Node(PRIM_HEMI, pNext),
            radius(radius),
            circlePoints(circlePoints),
            rings(rings),
            makeClosed(makeClosed) {
        }
    };

    struct PrimSphere : public Node {
        float radius;
        int circlePoints;
        int rings;

        PrimSphere(Node* pNext, float radius = 0.5, int circlePoints = 32, int rings = 16) :
            Node(PRIM_SPHERE, pNext),
            radius(radius),
            circlePoints(circlePoints),
            rings(rings) {
        }
    };

    struct PrimCapsule : public Node {
        float radius;
        float height;
        int circlePoints;
        int segments;
        int hemisphereRings;

        PrimCapsule( Node* pNext, float radius = 0.5, float height = 1.0, int circlePoints = 32, int segments = 1, int hemisphereRings = 8) :
            Node(PRIM_CAPSULE, pNext),
            radius(radius),
            height(height),
            circlePoints(circlePoints),
            segments(segments),
            hemisphereRings(hemisphereRings) {
        }
    };

    struct PrimPlane : public Node {
        float width;

        PrimPlane(Node* pNext, float width = 1.0) :
            Node(PRIM_PLANE, pNext),
            width(width) {
        }
    };

    struct PrimCube : public Node {
        float width;

        PrimCube(Node* pNext, float width = 1.0) :
            Node(PRIM_CUBE, pNext),
            width(width) {
        }
    };

    struct UVMapPlanar : public Node {
        glm::vec3 normal;

        UVMapPlanar(Node* pNext, const glm::vec3& normal = {0, 1, 0}) :
            Node(UV_PLANAR, pNext),
            normal(normal) {
        }
    };

    Node* pRootNode = nullptr;
    ~MeshBuilderNodes() {
        if (pRootNode) delete pRootNode;
    }

};


class MeshBuilder {

    MeshData* m_pExternMeshData = nullptr;
    MeshData m_localMeshData;

    MeshData& m_meshData;

public:

    MeshBuilder();

    // The MeshBuilder will have a non-owning pointer to MeshData stored somewhere else,
    // e.g. in a ResourceManager.
    // The MeshBuilder will operate on a reference to that external MeshData, and it's
    // internal MeshData will be empty.
    // the user should not call moveMeshData, since the rvalue reference will be for the local mesh data
    explicit MeshBuilder(MeshData* pExternMeshData);

    explicit MeshBuilder(const MeshData& md);

    explicit MeshBuilder(MeshData&& md);

    MeshBuilder(const MeshBuilder& m) = delete;

    const MeshData& getMeshData() const {
        return m_meshData;
    }

    const MeshData* getExternMeshData() const {
        return m_pExternMeshData;
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

    MeshBuilder& executeNodes(const MeshBuilderNodes& nodes);

    MeshBuilder& clear();

    MeshBuilder& computeBoundingSphere(BoundingSphere& b) {
        b = m_meshData.computeBoundingSphere();
        return *this;
    }

};

#endif // MESH_BUILDER_H_INCLUDED
