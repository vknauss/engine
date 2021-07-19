#include "texture.h"

#include <cassert>

Texture::Texture() {
    glGenTextures(1, &m_textureID);
}

Texture::Texture(Texture&& texture) :
        m_parameters(std::move(texture.m_parameters)),
        m_name(std::move(texture.m_name)),
        m_textureID(texture.m_textureID),
        m_internalFormat(texture.m_internalFormat),
        m_format(texture.m_format),
        m_textureTarget(texture.m_textureTarget),
        m_componentType(texture.m_componentType) {
    texture.m_textureID = 0;
}

Texture::~Texture() {
    glDeleteTextures(1, &m_textureID);
}

Texture& Texture::operator=(Texture&& texture) {
    m_parameters = std::move(texture.m_parameters);
    m_name = std::move(texture.m_name);

    m_textureID = texture.m_textureID;
    m_internalFormat = texture.m_internalFormat;
    m_format = texture.m_format;
    m_textureTarget = texture.m_textureTarget;
    m_componentType = texture.m_componentType;

    texture.m_textureID = 0;

    return *this;
}

void Texture::allocateData(const void* data) {
    glBindTexture(m_textureTarget, m_textureID);

    if(m_parameters.cubemap) {
        for(GLenum face = GL_TEXTURE_CUBE_MAP_POSITIVE_X; face <= GL_TEXTURE_CUBE_MAP_NEGATIVE_Z; ++face) {
            glTexImage2D(
                face,
                0,
                m_internalFormat,
                static_cast<GLsizei>(m_parameters.width),
                static_cast<GLsizei>(m_parameters.height),
                0,
                m_format,
                m_componentType,
                data);
        }
    } else if(m_parameters.arrayLayers  > 1) {
        if(m_parameters.samples > 1) {
            glTexImage3DMultisample(
                m_textureTarget,
                static_cast<GLsizei>(m_parameters.samples),
                static_cast<GLenum>(m_internalFormat),
                static_cast<GLsizei>(m_parameters.width),
                static_cast<GLsizei>(m_parameters.height),
                static_cast<GLsizei>(m_parameters.arrayLayers),
                GL_TRUE);
        } else {
            glTexImage3D(
                m_textureTarget,
                0,
                m_internalFormat,
                static_cast<GLsizei>(m_parameters.width),
                static_cast<GLsizei>(m_parameters.height),
                static_cast<GLsizei>(m_parameters.arrayLayers),
                0,
                m_format,
                m_componentType,
                data);
        }

    } else {
        if(m_parameters.samples > 1) {
            glTexImage2DMultisample(
                m_textureTarget,
                static_cast<GLsizei>(m_parameters.samples),
                static_cast<GLenum>(m_internalFormat),
                static_cast<GLsizei>(m_parameters.width),
                static_cast<GLsizei>(m_parameters.height),
                GL_TRUE);
        } else {
            glTexImage2D(
                m_textureTarget,
                0,
                m_internalFormat,
                static_cast<GLsizei>(m_parameters.width),
                static_cast<GLsizei>(m_parameters.height),
                0,
                m_format,
                m_componentType,
                data);
        }
    }

    GLenum err = glGetError();
    if (err != GL_NO_ERROR) {
        assert(0);
    }
}

void Texture::bind(uint32_t index) const {
    glActiveTexture(GL_TEXTURE0 + index);
    glBindTexture(m_textureTarget, m_textureID);
}

void Texture::setParameters(const TextureParameters& parameters) {
    validateParameters(parameters);
    m_parameters = parameters;
    updateParameters();
}

void Texture::generateMipmaps() {
    glBindTexture(m_textureTarget, m_textureID);
    glGenerateMipmap(m_textureTarget);
}

void getFormat(uint32_t nComponents, uint32_t nBits, bool useFloat, bool isBGR, GLenum& format, GLint& internalFormat, GLenum& componentType) {
    /*switch(nComponents) {
    case 1:
        format = GL_RED;
        if(useFloat) {
            if (nBits == 32) internalFormat = GL_R32F;
            else             internalFormat = GL_R16F;
        } else {
            if (nBits == 16) internalFormat = GL_R16;
            else                  internalFormat = GL_R8;
        }
        break;
    case 2:
        format = GL_RG;
        if(useFloat) {
            if (nBits == 32) internalFormat = GL_RG32F;
            else             internalFormat = GL_RG16F;
        } else {
            if (nBits == 16) internalFormat = GL_RG16;
            else                  internalFormat = GL_RG8;
        }
        break;
    case 3:
        format = isBGR ? GL_BGR : GL_RGB;
        if(useFloat) {
            if (nBits == 32) internalFormat = GL_RGB32F;
            else             internalFormat = GL_RGB16F;
        } else {
            if (nBits == 16) internalFormat = GL_RGB16;
            else                  internalFormat = GL_RGB8;
        }
        break;
    case 4:
        format = isBGR ? GL_BGRA : GL_RGBA;
        if(useFloat) {
            if (nBits == 32) internalFormat = GL_RGBA32F;
            else             internalFormat = GL_RGBA16F;
        } else {
            if (nBits == 16) internalFormat = GL_RGBA16;
            else                  internalFormat = GL_RGBA8;
        }
        break;
    default:
        assert(0);
        break;
    }

    if(useFloat) {
        if (nBits == 32) componentType = GL_FLOAT;
        else             componentType = GL_HALF_FLOAT;
    } else {
        if (nBits == 16) componentType = GL_UNSIGNED_SHORT;
        else                  componentType = GL_UNSIGNED_BYTE;
    }*/

    switch(nComponents) {
    case 1:
        format = GL_RED;
        if(useFloat) {
            internalFormat = GL_R32F;
        } else {
            if (nBits == 16) internalFormat = GL_R16;
            else             internalFormat = GL_R8;
        }
        break;
    case 2:
        format = GL_RG;
        if(useFloat) {
            internalFormat = GL_RG32F;
        } else {
            if (nBits == 16) internalFormat = GL_RG16;
            else             internalFormat = GL_RG8;
        }
        break;
    case 3:
        format = isBGR ? GL_BGR : GL_RGB;
        if(useFloat) {
            internalFormat = GL_RGB32F;
        } else {
            if (nBits == 16) internalFormat = GL_RGB16;
            else             internalFormat = GL_RGB8;
        }
        break;
    case 4:
        format = isBGR ? GL_BGRA : GL_RGBA;
        if(useFloat) {
            internalFormat = GL_RGBA32F;
        } else {
            if (nBits == 16) internalFormat = GL_RGBA16;
            else             internalFormat = GL_RGBA8;
        }
        break;
    default:
        assert(0);
        break;
    }

    if(useFloat) {
        componentType = GL_FLOAT;
    } else {
        if (nBits == 16) componentType = GL_UNSIGNED_SHORT;
        else             componentType = GL_UNSIGNED_BYTE;
    }
}

void Texture::updateParameters() {
    if(m_parameters.cubemap) {
        m_textureTarget = GL_TEXTURE_CUBE_MAP;
    } else if (m_parameters.is3D) {
        m_textureTarget = GL_TEXTURE_3D;
    } else if(m_parameters.arrayLayers > 1) {
        if(m_parameters.samples > 1) {
            m_textureTarget = GL_TEXTURE_2D_MULTISAMPLE_ARRAY;
        } else {
            m_textureTarget = GL_TEXTURE_2D_ARRAY;
        }
    } else {
        if(m_parameters.samples > 1) {
            m_textureTarget = GL_TEXTURE_2D_MULTISAMPLE;
        } else {
            m_textureTarget = GL_TEXTURE_2D;
        }
    }

    if(!m_parameters.useDepthComponent) {
        GLenum format, componentType;
        GLint internalFormat;
        getFormat(
            m_parameters.numComponents,
            m_parameters.bitsPerComponent,
            m_parameters.useFloatComponents,
            m_parameters.isBGR,
            format, internalFormat, componentType);
        m_format = format;
        m_internalFormat = internalFormat;
        m_componentType = componentType;
    } else {
        if (m_parameters.useStencilComponent) {
            m_format = GL_DEPTH_STENCIL;
            if (m_parameters.useFloatComponents) {
                // There is no half float for the depth component
                m_internalFormat = GL_DEPTH32F_STENCIL8;
                m_componentType = GL_FLOAT_32_UNSIGNED_INT_24_8_REV;
            } else {
                m_internalFormat = GL_DEPTH24_STENCIL8;
                m_componentType = GL_UNSIGNED_INT_24_8;
            }
        } else {
            m_format = GL_DEPTH_COMPONENT;
            if (m_parameters.useFloatComponents) {
                // There is no half float for the depth component
                m_internalFormat = GL_DEPTH_COMPONENT32F;
                m_componentType = GL_FLOAT;
            } else {
                if(m_parameters.bitsPerComponent == 24) {
                    m_internalFormat = GL_DEPTH_COMPONENT24;
                    m_componentType = GL_UNSIGNED_INT;
                } else {
                    m_internalFormat = GL_DEPTH_COMPONENT16;
                    m_componentType = GL_UNSIGNED_SHORT;
                }
            }
        }
    }

    glBindTexture(m_textureTarget, m_textureID);

    if (m_parameters.samples <= 1) {
        if (m_parameters.useLinearFiltering) {
            glTexParameteri(m_textureTarget, GL_TEXTURE_MIN_FILTER, m_parameters.useMipmapFiltering? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR);
            glTexParameteri(m_textureTarget, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        } else {
            glTexParameteri(m_textureTarget, GL_TEXTURE_MIN_FILTER, m_parameters.useMipmapFiltering? GL_NEAREST_MIPMAP_NEAREST : GL_NEAREST);
            glTexParameteri(m_textureTarget, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        }

        if (m_parameters.useEdgeClamping) {
            glTexParameteri(m_textureTarget, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(m_textureTarget, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            if(m_parameters.cubemap || m_parameters.arrayLayers > 1) {
                glTexParameteri(m_textureTarget, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
            }
        } else {
            glTexParameteri(m_textureTarget, GL_TEXTURE_WRAP_S, GL_REPEAT);
            glTexParameteri(m_textureTarget, GL_TEXTURE_WRAP_T, GL_REPEAT);
            if(m_parameters.cubemap || m_parameters.arrayLayers > 1) {
                glTexParameteri(m_textureTarget, GL_TEXTURE_WRAP_R, GL_REPEAT);
            }
        }

        if (m_parameters.useAnisotropicFiltering) {
            float aniso;
            glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &aniso);
            glTexParameterf(m_textureTarget, GL_TEXTURE_MAX_ANISOTROPY_EXT, aniso);
        } else {
            glTexParameterf(m_textureTarget, GL_TEXTURE_MAX_ANISOTROPY_EXT, 1.0);
        }
    }
}

void Texture::validateParameters(const TextureParameters& parameters) {
    assert(parameters.useDepthComponent || (parameters.numComponents > 0 && parameters.numComponents <= 4));
}
