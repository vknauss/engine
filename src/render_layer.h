#ifndef RENDER_LAYER_H_
#define RENDER_LAYER_H_

#include <vector>

#include <GL/glew.h>

#include "render_buffer.h"
#include "texture.h"


class RenderLayer {

public:

    RenderLayer();

    ~RenderLayer();

    void setRenderBufferAttachment(RenderBuffer* renderBuffer);

    void setDepthTexture(Texture* texture, uint32_t textureArrayLayer = 0);

    void setTextureAttachment(uint32_t index, Texture* texture, uint32_t textureArrayLayer = 0);

    void setEnabledDrawTargets(std::vector<uint32_t> targets);

    void setEnabledReadTarget(uint32_t target);

    void bind(GLenum bindTarget = GL_FRAMEBUFFER);

    void unbind();

    void validate();

private:

    GLuint m_fbo;

    uint32_t m_width, m_height;

    GLint m_colorInternalFormat, m_colorFormat;
    bool m_fpComponents;

};

#endif // RENDER_LAYER_H_
