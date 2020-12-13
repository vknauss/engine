#ifndef MESH_H_
#define MESH_H_

#include <vector>

#include <GL/glew.h>

#include "mesh_data.h"

class Mesh {

    struct VertexBuffer {
        uint32_t index, numComponents;
        uint64_t numElements;
        size_t componentSize;
        GLuint vbo;
        bool stream, integer;
    };

public:

    Mesh();

    ~Mesh();

    void createVertexBuffer(uint32_t index, uint32_t numComponents, const void* data, bool stream = false, bool integer = false, bool instanced = false, uint64_t numElements = 0);

    void createIndexBuffer(size_t numIndices, const void* data);

    void setDrawType(GLenum type);

    void setVertexCount(size_t numVertices);

    void setInstanceCount(size_t numInstances);

    void streamVertexBuffer(uint32_t index, const void* data, uint64_t numElements = 0);

    void bind() const;

    void draw() const;

    GLenum getDrawType() const {
        return m_drawType;
    }

    size_t getIndexCount() const {
        return m_numIndices;
    }

    size_t getVertexCount() const {
        return m_numVertices;
    }

    GLuint getBufferID(uint32_t index) const {
        return m_buffers[index].vbo;
    }

    GLuint getIndexBufferID() const {
        return m_ebo;
    }

private:

    GLuint m_vao;
    GLuint m_ebo;

    size_t m_numVertices = 0;
    size_t m_numIndices = 0;
    size_t m_numInstances = 0;

    std::vector<VertexBuffer> m_buffers;

    GLenum m_drawType = GL_TRIANGLES;

};



#endif // MESH_H_
