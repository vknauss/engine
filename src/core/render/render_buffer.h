#ifndef RENDER_BUFFER_H_
#define RENDER_BUFFER_H_

#include <GL/glew.h>

// Literally an object to encapsulate OGL RBO, cuz Im a dummy :)

struct RenderBufferParameters {
    int width;
    int height;
    int samples;
    bool hasStencilComponent;
    bool useFloat32;
};

class RenderBuffer {

public:

    RenderBuffer();

    ~RenderBuffer();

    void allocateData();

    void setParameters(RenderBufferParameters parameters);

    RenderBufferParameters getParameters() const {
        return m_parameters;
    }

    GLuint getHandle() const {
        return m_rboID;
    }

private:

    RenderBufferParameters m_parameters;

    GLuint m_rboID;

};

#endif // RENDER_BUFFER_H_
