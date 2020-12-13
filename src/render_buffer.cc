#include "render_buffer.h"

RenderBuffer::RenderBuffer() {
    glGenRenderbuffers(1, &m_rboID);
}

RenderBuffer::~RenderBuffer() {
    glDeleteRenderbuffers(1, &m_rboID);
}

void RenderBuffer::allocateData() {
    glBindRenderbuffer(GL_RENDERBUFFER, m_rboID);

    GLenum internalFormat;
    if(m_parameters.hasStencilComponent) {
        internalFormat = GL_DEPTH24_STENCIL8;
    } else {
        internalFormat = GL_DEPTH_COMPONENT32;
    }

    if(m_parameters.samples > 1) {
        glRenderbufferStorageMultisample(GL_RENDERBUFFER, m_parameters.samples, internalFormat, m_parameters.width, m_parameters.height);
    } else {
        glRenderbufferStorage(GL_RENDERBUFFER, internalFormat, m_parameters.width, m_parameters.height);
    }
}

void RenderBuffer::setParameters(RenderBufferParameters parameters) {
    m_parameters = parameters;
}
