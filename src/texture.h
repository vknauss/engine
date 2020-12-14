#ifndef TEXTURE_H_
#define TEXTURE_H_

#include <cassert>

#include <GL/glew.h>

struct TextureParameters {
    uint32_t width;
    uint32_t height;
    uint32_t arrayLayers;
    uint32_t numComponents;
    uint32_t bitsPerComponent;
    uint32_t samples;
    bool cubemap;
    bool isBGR;
    bool useFloatComponents;
    bool useLinearFiltering;
    bool useMipmapFiltering;
    bool useAnisotropicFiltering;
    bool useEdgeClamping;
    bool useDepthComponent;
};

class Texture {

public:

    Texture();

    ~Texture();

    void allocateData(const void* data);

    void bind(uint32_t index) const;

    void setParameters(TextureParameters parameters);

    void generateMipmaps();

    const TextureParameters& getParameters() const {
        return m_parameters;
    }

    GLuint getHandle() const {
        return m_textureID;
    }

private:

    TextureParameters m_parameters;

    GLuint m_textureID;

    GLint m_internalFormat;
    GLenum m_format;
    GLenum m_textureTarget;
    GLenum m_componentType;


    void updateParameters();

    static void validateParameters(const TextureParameters& parameters);

};


#endif // TEXTURE_H_
