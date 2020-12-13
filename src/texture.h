#ifndef TEXTURE_H_
#define TEXTURE_H_

#include <cassert>

#include <GL/glew.h>

struct TextureParameters {
    int width;
    int height;
    int arrayLayers;
    int numComponents;
    int bitsPerComponent;
    int samples;
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

    void allocateData(void* data);

    void bind(int index) const;

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

    GLint m_format;
    GLint m_internalFormat;
    GLenum m_textureTarget;
    GLenum m_componentType;


    void updateParameters();

    static void validateParameters(const TextureParameters& parameters);

};


#endif // TEXTURE_H_
