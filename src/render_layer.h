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

    // TBH I don't love this part
    // The RenderLayer doesn't own nor exactly care about its render targets... Except for this one
    //void createDepthRenderbuffer(int width, int height, bool stencil = false, int samples = 1);

    void setRenderBufferAttachment(RenderBuffer* renderBuffer);

    void setDepthTexture(Texture* texture, int textureArrayLayer = 0);

    void setTextureAttachment(int index, Texture* texture, int textureArrayLayer = 0);

    void setEnabledDrawTargets(std::vector<int> targets);

    void setEnabledReadTarget(int target);

    void bind(GLenum bindTarget = GL_FRAMEBUFFER);

    void unbind();

    void validate();

private:

    //GLuint m_depthRbo;
    GLuint m_fbo;

    int m_width, m_height;

    GLint m_colorInternalFormat, m_colorFormat;
    bool m_fpComponents;

};

#endif // RENDER_LAYER_H_
