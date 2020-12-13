#include "mesh.h"

#include <cassert>

Mesh::Mesh() {
    glGenVertexArrays(1, &m_vao);
}

Mesh::~Mesh() {
    if(m_numIndices > 0) glDeleteBuffers(1, &m_ebo);
    for(const VertexBuffer& buffer : m_buffers) glDeleteBuffers(1, &buffer.vbo);
    glDeleteVertexArrays(1, &m_vao);
}

void Mesh::createVertexBuffer(uint32_t index, uint32_t numComponents, const void* data, bool stream, bool integer, bool instanced, uint64_t numElements) {
    assert(m_numVertices > 0);
    glBindVertexArray(m_vao);

    m_buffers.push_back(VertexBuffer {});

    VertexBuffer& buffer = m_buffers.back();
    buffer.index = index;
    buffer.numComponents = numComponents;
    buffer.integer = integer;
    buffer.stream = stream;
    buffer.componentSize = integer ? sizeof(GLint) : sizeof(GLfloat);
    buffer.numElements = (numElements == 0) ? m_numVertices : numElements;

    glGenBuffers(1, &buffer.vbo);
    glBindBuffer(GL_ARRAY_BUFFER, buffer.vbo);
    glBufferData(GL_ARRAY_BUFFER, buffer.numElements*buffer.numComponents*buffer.componentSize, data, stream? GL_STREAM_DRAW : GL_STATIC_DRAW);
    if(buffer.integer) {
        glVertexAttribIPointer(buffer.index, buffer.numComponents, GL_INT, 0, nullptr);
    } else {
        glVertexAttribPointer(buffer.index, buffer.numComponents, GL_FLOAT, GL_FALSE, 0, nullptr);
    }
    glEnableVertexAttribArray(buffer.index);
    glVertexAttribDivisor(buffer.index, instanced ? 1 : 0);
}

void Mesh::createIndexBuffer(size_t numIndices, const void* data) {
    if(m_numIndices > 0 || numIndices == 0) return;
    m_numIndices = numIndices;

    glBindVertexArray(m_vao);

    glGenBuffers(1, &m_ebo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, numIndices*sizeof(GLuint), data, GL_STATIC_DRAW);
}

void Mesh::setDrawType(GLenum type) {
    m_drawType = type;
}

void Mesh::setVertexCount(size_t numVertices) {
    m_numVertices = numVertices;
}

void Mesh::setInstanceCount(size_t numInstances) {
    m_numInstances = numInstances;
}

void Mesh::streamVertexBuffer(uint32_t index, const void* data, uint64_t numElements) {
    for(const VertexBuffer& buffer : m_buffers) {
        if(buffer.index == index) {
            if(numElements == 0) numElements = buffer.numElements;
            glBindBuffer(GL_ARRAY_BUFFER, buffer.vbo);
            glBufferData(GL_ARRAY_BUFFER, buffer.numElements*buffer.numComponents*buffer.componentSize, nullptr, GL_STREAM_DRAW);
            glBufferSubData(GL_ARRAY_BUFFER, 0, numElements*buffer.numComponents*buffer.componentSize, data);
            break;
        }
    }
}

void Mesh::bind() const {
    glBindVertexArray(m_vao);
}

void Mesh::draw() const {
    if(m_numInstances > 1) {
        if(m_numIndices > 0) {
            glDrawElementsInstanced(m_drawType, m_numIndices, GL_UNSIGNED_INT, nullptr, m_numInstances);
        } else {
            glDrawArraysInstanced(m_drawType, 0, m_numVertices, m_numInstances);
        }
    } else {
        if(m_numIndices > 0) {
            glDrawElements(m_drawType, m_numIndices, GL_UNSIGNED_INT, nullptr);
        } else {
            glDrawArrays(m_drawType, 0, m_numVertices);
        }
    }
}
