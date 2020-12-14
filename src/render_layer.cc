#include "render_layer.h"

#include <cassert>
#include <stdexcept>


RenderLayer::RenderLayer() {
    glGenFramebuffers(1, &m_fbo);
}

RenderLayer::~RenderLayer() {
    glDeleteFramebuffers(1, &m_fbo);
}

void RenderLayer::setRenderBufferAttachment(RenderBuffer* renderBuffer) {
    glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);

    glBindRenderbuffer(GL_RENDERBUFFER, renderBuffer->getHandle());

    glFramebufferRenderbuffer(GL_FRAMEBUFFER, renderBuffer->getParameters().hasStencilComponent ? GL_DEPTH_STENCIL_ATTACHMENT : GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, renderBuffer->getHandle());

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void setAttachment(GLuint fboID, GLenum attachment, Texture* texture, uint32_t layer) {
    glBindFramebuffer(GL_FRAMEBUFFER, fboID);

    if(texture == nullptr) {
        glFramebufferTexture(GL_FRAMEBUFFER, attachment, 0, 0);
        return;
    }

    if(texture->getParameters().arrayLayers > 1 || texture->getParameters().cubemap) {
        if(layer >= 0) {
            if(texture->getParameters().cubemap) {
                glFramebufferTexture2D(GL_FRAMEBUFFER, attachment, GL_TEXTURE_CUBE_MAP_POSITIVE_X+layer, texture->getHandle(), 0);
            } else {
                glFramebufferTextureLayer(GL_FRAMEBUFFER, attachment, texture->getHandle(), 0, static_cast<GLint>(layer));
            }
        } else {
            glFramebufferTexture(GL_FRAMEBUFFER, attachment, texture->getHandle(), 0);
        }
    } else {
        if(texture->getParameters().samples > 1) {
            glFramebufferTexture2D(GL_FRAMEBUFFER, attachment, GL_TEXTURE_2D_MULTISAMPLE, texture->getHandle(), 0);
        } else {
            glFramebufferTexture2D(GL_FRAMEBUFFER, attachment, GL_TEXTURE_2D, texture->getHandle(), 0);
        }
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void RenderLayer::setDepthTexture(Texture* texture, uint32_t textureArrayLayer) {
    setAttachment(m_fbo, GL_DEPTH_ATTACHMENT, texture, textureArrayLayer);
}

void RenderLayer::setTextureAttachment(uint32_t index, Texture* texture, uint32_t textureArrayLayer) {
    setAttachment(m_fbo, GL_COLOR_ATTACHMENT0+index, texture, textureArrayLayer);
}

void RenderLayer::setEnabledDrawTargets(std::vector<uint32_t> targets) {
    glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);

    std::vector<GLenum> drawBuffers(targets.size());
    for(size_t i = 0; i < targets.size(); ++i) {
        drawBuffers[i] = GL_COLOR_ATTACHMENT0 + targets[i];
    }

    if(drawBuffers.empty()) {
        drawBuffers.push_back(GL_NONE);
    }

    glDrawBuffers(drawBuffers.size(), drawBuffers.data());

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void RenderLayer::setEnabledReadTarget(uint32_t target) {
    glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);

    glReadBuffer(GL_COLOR_ATTACHMENT0 + target);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void RenderLayer::bind(GLenum bindTarget) {
    glBindFramebuffer(bindTarget, m_fbo);
}

void RenderLayer::unbind() {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void RenderLayer::validate() {
    glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if(status != GL_FRAMEBUFFER_COMPLETE) {
        std::string statusString;
        switch(status) {
        case GL_FRAMEBUFFER_UNDEFINED:
            statusString = "GL_FRAMEBUFFER_UNDEFINED";
            break;
        case GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT:
            statusString = "GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT";
            break;
        case GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT:
            statusString = "GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT";
            break;
        case GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER:
            statusString = "GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER";
            break;
        case GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER:
            statusString = "GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER";
            break;
        case GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE:
            statusString = "GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE";
            break;
        case GL_FRAMEBUFFER_INCOMPLETE_LAYER_TARGETS:
            statusString = "GL_FRAMEBUFFER_INCOMPLETE_LAYER_TARGETS";
            break;
        default:
            statusString = "Status unknown. Enum value: " + std::to_string(status);
            break;
        }

        throw std::runtime_error("Failed to validate framebuffer. Status: " + statusString);
    }
}
