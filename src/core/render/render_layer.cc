#include "render_layer.h"

#include <cassert>
#include <stdexcept>


RenderLayer::RenderLayer() {
    glGenFramebuffers(1, &m_fbo);
}

RenderLayer::~RenderLayer() {
    glDeleteFramebuffers(1, &m_fbo);
}

void RenderLayer::setRenderBufferAttachment(const RenderBuffer* pRenderBuffer) {
    glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);

    glBindRenderbuffer(GL_RENDERBUFFER, pRenderBuffer->getHandle());

    GLenum attachment = (pRenderBuffer && pRenderBuffer->getParameters().hasStencilComponent) ?
        GL_DEPTH_STENCIL_ATTACHMENT :
        GL_DEPTH_ATTACHMENT;
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, attachment, GL_RENDERBUFFER, pRenderBuffer->getHandle());

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

static void setAttachment(GLuint fboID, GLenum attachment, const Texture* pTexture, uint32_t arrayLayer) {
    glBindFramebuffer(GL_FRAMEBUFFER, fboID);

    if (pTexture) {
        if (pTexture->getParameters().arrayLayers > 1 || pTexture->getParameters().cubemap) {
            if (arrayLayer >= 0) {
                if (pTexture->getParameters().cubemap) {
                    GLenum face = GL_TEXTURE_CUBE_MAP_POSITIVE_X + arrayLayer;
                    glFramebufferTexture2D(GL_FRAMEBUFFER, attachment, face, pTexture->getHandle(), 0);
                } else {
                    glFramebufferTextureLayer(GL_FRAMEBUFFER, attachment, pTexture->getHandle(), 0, arrayLayer);
                }
            } else {
                glFramebufferTexture(GL_FRAMEBUFFER, attachment, pTexture->getHandle(), 0);
            }
        } else {
            if (pTexture->getParameters().samples > 1) {
                glFramebufferTexture2D(GL_FRAMEBUFFER, attachment, GL_TEXTURE_2D_MULTISAMPLE, pTexture->getHandle(), 0);
            } else {
                glFramebufferTexture2D(GL_FRAMEBUFFER, attachment, GL_TEXTURE_2D, pTexture->getHandle(), 0);
            }
        }
    } else {
        glFramebufferTexture(GL_FRAMEBUFFER, attachment, 0, 0);
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void RenderLayer::setDepthTexture(const Texture* pTexture, uint32_t arrayLayer) {
    GLenum attachment = (pTexture && pTexture->getParameters().useStencilComponent) ?
        GL_DEPTH_STENCIL_ATTACHMENT :
        GL_DEPTH_ATTACHMENT;
    setAttachment(m_fbo, attachment, pTexture, arrayLayer);
}

void RenderLayer::setTextureAttachment(uint32_t index, const Texture* pTexture, uint32_t arrayLayer) {
    setAttachment(m_fbo, GL_COLOR_ATTACHMENT0 + index, pTexture, arrayLayer);
}

void RenderLayer::setEnabledDrawTargets(const std::vector<uint32_t>& targets) {
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

void RenderLayer::clearAttachment(GLenum attachment) {
    setAttachment(m_fbo, attachment, nullptr, 0);
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
        case GL_FRAMEBUFFER_UNSUPPORTED:
            statusString = "GL_FRAMEBUFFER_UNSUPPORTED";
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
