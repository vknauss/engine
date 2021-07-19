#include "mesh.h"

#include <numeric>

Mesh::Mesh() {
    //glGenVertexArrays(1, &m_vao);
}

Mesh::~Mesh() {
    if (m_initialized) cleanup();
}

void Mesh::createFromMeshData(const MeshData* pMeshData, bool useUVs, bool useSkinning, bool useTangentSpace) {
    cleanup();
    m_pMeshData = pMeshData;

    useUVs = (useUVs && pMeshData->hasUVs());
    useSkinning = (useSkinning && pMeshData->hasBoneIndices() && pMeshData->hasBoneWeights());
    useTangentSpace = (useTangentSpace && pMeshData->hasTangents() && pMeshData->hasBitangents());

    setVertexCount(pMeshData->getNumVertices());

    m_attributes.clear();
    createVertexAttribute(0, 3);
    if (pMeshData->hasNormals()) createVertexAttribute(1, 3);
    if (useUVs) createVertexAttribute(2, 2);
    if (useSkinning) {
        createVertexAttribute(3, 4, true);
        createVertexAttribute(4, 4);
    }
    if (useTangentSpace) {
        createVertexAttribute(5, 3);
        createVertexAttribute(6, 3);
    }

    allocateVertexBuffer();

    setVertexAttributeData(0, pMeshData->vertices.data());
    if (pMeshData->hasNormals()) setVertexAttributeData(1, pMeshData->normals.data());
    if (useUVs) setVertexAttributeData(2, pMeshData->uvs.data());
    if (useSkinning) {
        setVertexAttributeData(3, pMeshData->boneIndices.data());
        setVertexAttributeData(4, pMeshData->boneWeights.data());
    }
    if (useTangentSpace) {
        setVertexAttributeData(5, pMeshData->tangents.data());
        setVertexAttributeData(6, pMeshData->bitangents.data());
    }

    createIndexBuffer(pMeshData->indices.size(), pMeshData->indices.data());
}

void Mesh::createIndexBuffer(size_t numIndices, const void* data) {
    m_numIndices = numIndices;
    if (numIndices == 0) return;

    if (!m_initialized) initialize();
    glBindVertexArray(m_vao);

    if (!m_ebo) glGenBuffers(1, &m_ebo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, numIndices*sizeof(GLuint), data, GL_STATIC_DRAW);
}

void Mesh::createVertexAttribute(uint32_t index, uint32_t numComponents, bool integer) {
    if (m_attributes.size() < index + 1) m_attributes.resize(index + 1);
    m_attributes[index].numComponents = numComponents;
    m_attributes[index].componentSize = integer ? sizeof(GLint) : sizeof(GLfloat);
    m_attributes[index].integer = integer;
}

void Mesh::allocateVertexBuffer(const void* data) {
    if (!m_initialized) initialize();
    glBindVertexArray(m_vao);

    size_t bufferSize = std::accumulate(m_attributes.begin(), m_attributes.end(), static_cast<size_t>(0),
        [this] (size_t acc, const VertexAttribute& attrib) {
            return acc + m_numVertices * attrib.numComponents * attrib.componentSize; });

    if (!m_vbo) glGenBuffers(1, &m_vbo);
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBufferData(GL_ARRAY_BUFFER, bufferSize, data, GL_STATIC_DRAW);

    if (data) {  // if data is not null, assume the user will not be calling setVertexAttributeData and set the pointers here
        intptr_t offset = 0;
        for (uint32_t index = 0; index < m_attributes.size(); ++index) {
            if (m_attributes[index].numComponents > 0) {
                if (m_attributes[index].integer) {
                    glVertexAttribIPointer(index, m_attributes[index].numComponents, GL_INT, 0, reinterpret_cast<void*>(offset));
                } else {
                    glVertexAttribPointer(index, m_attributes[index].numComponents, GL_FLOAT, GL_FALSE, 0, reinterpret_cast<void*>(offset));
                }
                glEnableVertexAttribArray(index);
            }
            offset += m_numVertices * m_attributes[index].numComponents * m_attributes[index].componentSize;
        }
    }
}

void Mesh::setVertexAttributeData(uint32_t index, const void* data) {
    glBindVertexArray(m_vao);

    intptr_t offset = std::accumulate(m_attributes.begin(), m_attributes.begin() + index, static_cast<intptr_t>(0),
        [this] (intptr_t acc, const VertexAttribute& attrib) {
            return acc + m_numVertices * attrib.numComponents * attrib.componentSize; });

    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBufferSubData(GL_ARRAY_BUFFER, offset, m_numVertices * m_attributes[index].numComponents * m_attributes[index].componentSize, data);
    if (m_attributes[index].integer) {
        glVertexAttribIPointer(index, m_attributes[index].numComponents, GL_INT, 0, reinterpret_cast<void*>(offset));
    } else {
        glVertexAttribPointer(index, m_attributes[index].numComponents, GL_FLOAT, GL_FALSE, 0, reinterpret_cast<void*>(offset));
    }
    glEnableVertexAttribArray(index);
}

void Mesh::setDrawType(GLenum type) {
    m_drawType = type;
}

void Mesh::setVertexCount(size_t numVertices) {
    m_numVertices = numVertices;
}

/*void Mesh::setInstanceCount(size_t numInstances) {
    m_numInstances = numInstances;
}*/

/*void Mesh::streamVertexBuffer(uint32_t index, const void* data, uint64_t numElements) {
    for(const VertexBuffer& buffer : m_buffers) {
        if(buffer.index == index) {
            if(numElements == 0) numElements = buffer.numElements;
            glBindBuffer(GL_ARRAY_BUFFER, buffer.vbo);
            glBufferData(GL_ARRAY_BUFFER, buffer.numElements*buffer.numComponents*buffer.componentSize, nullptr, GL_STREAM_DRAW);
            glBufferSubData(GL_ARRAY_BUFFER, 0, numElements*buffer.numComponents*buffer.componentSize, data);
            break;
        }
    }
}*/

void Mesh::bind() const {
    glBindVertexArray(m_vao);
}

void Mesh::draw() const {
    /*if(m_numInstances > 1) {
        if(m_numIndices > 0) {
            glDrawElementsInstanced(m_drawType, m_numIndices, GL_UNSIGNED_INT, nullptr, m_numInstances);
        } else {
            glDrawArraysInstanced(m_drawType, 0, m_numVertices, m_numInstances);
        }
    } else {*/
        if(m_numIndices > 0) {
            glDrawElements(m_drawType, m_numIndices, GL_UNSIGNED_INT, nullptr);
        } else {
            glDrawArrays(m_drawType, 0, m_numVertices);
        }
    //}
}

void Mesh::cleanup() {
    if (m_ebo) glDeleteBuffers(1, &m_ebo);
    if (m_vbo) glDeleteBuffers(1, &m_vbo);
    if (m_vao) glDeleteVertexArrays(1, &m_vao);
    m_ebo = m_vbo = m_vao = 0;
    m_initialized = false;
}

void Mesh::initialize() {
    assert(!m_initialized);
    glGenVertexArrays(1, &m_vao);
    m_initialized = true;
}
