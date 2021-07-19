#ifndef RENDER_LAYER_H_
#define RENDER_LAYER_H_

#include <vector>

#include <GL/glew.h>

#include "render_buffer.h"
#include "core/resources/texture.h"

// Literally just an FBO
// Don't know why I called it a Render Layer...

class RenderLayer {

public:

    RenderLayer();

    ~RenderLayer();

    void setRenderBufferAttachment(const RenderBuffer* pRenderBuffer);

    void setDepthTexture(const Texture* pTexture, uint32_t arrayLayer = 0);

    void setTextureAttachment(uint32_t index, const Texture* pTexture, uint32_t arrayLayer = 0);

    void setEnabledDrawTargets(const std::vector<uint32_t>& targets);

    void setEnabledReadTarget(uint32_t target);

    void clearAttachment(GLenum attachment);

    void bind(GLenum bindTarget = GL_FRAMEBUFFER);

    void validate();

    static void unbind();

private:

    GLuint m_fbo;

};

#endif // RENDER_LAYER_H_
