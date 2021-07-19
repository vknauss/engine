#ifndef MESH_H_
#define MESH_H_

#include <string>
#include <vector>

#include <GL/glew.h>

#include "mesh_data.h"

class Mesh {

    struct VertexAttribute {
        uint32_t numComponents = 0;
        size_t componentSize = 0;
        bool integer = false;
    };

public:

    Mesh();

    ~Mesh();

    //void createVertexBuffer(uint32_t index, uint32_t numComponents, const void* data, bool stream = false, bool integer = false, bool instanced = false, uint64_t numElements = 0);

    void createFromMeshData(const MeshData* pMeshData, bool useUVs = true, bool useSkinning = true, bool useTangentSpace = true);

    void createIndexBuffer(size_t numIndices, const void* data);

    void createVertexAttribute(uint32_t index, uint32_t numComponents, bool integer = false);

    void allocateVertexBuffer(const void* data = nullptr);

    void setVertexAttributeData(uint32_t index, const void* data);

    void setDrawType(GLenum type);

    void setVertexCount(size_t numVertices);

    void setName(const std::string& name) {
        m_name = name;
    }

    //void setInstanceCount(size_t numInstances);

    //void streamVertexBuffer(uint32_t index, const void* data, uint64_t numElements = 0);

    void bind() const;

    void draw() const;

    void cleanup();

    GLenum getDrawType() const {
        return m_drawType;
    }

    size_t getIndexCount() const {
        return m_numIndices;
    }

    size_t getVertexCount() const {
        return m_numVertices;
    }

    const MeshData* getMeshData() const {
        return m_pMeshData;
    }

    const std::string& getName() const {
        return m_name;
    }

private:

    size_t m_numVertices = 0;
    size_t m_numIndices = 0;

    GLuint m_vao = 0;
    GLuint m_vbo = 0;
    GLuint m_ebo = 0;

    //size_t m_numInstances = 0;

    std::string m_name;

    std::vector<VertexAttribute> m_attributes;

    GLenum m_drawType = GL_TRIANGLES;

    bool m_initialized = false;

    const MeshData* m_pMeshData = nullptr;

    void initialize();

};

#endif // MESH_H_
