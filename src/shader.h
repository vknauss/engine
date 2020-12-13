#ifndef SHADER_H_
#define SHADER_H_

#include <string>
#include <GL/glew.h>
#include <glm/glm.hpp>

class Shader {

public:

    Shader();

    ~Shader();

    void linkShaderFiles(const std::string& vertexCodePath, const std::string& fragmentCodePath);
    void linkShaderFiles(const std::string& vertexCodePath, const std::string& fragmentCodePath, const std::string& geometryCodePath);
    void linkShaderFiles(const std::string& vertexCodePath, const std::string& fragmentCodePath, const std::string& vertexHeader, const std::string& fragmentHeader);
    void linkShaderFiles(const std::string& vertexCodePath, const std::string& fragmentCodePath, const std::string& geometryCodePath,
        const std::string& vertexHeader, const std::string& fragmentHeader, const std::string& geometryHeader);

    // useful for depth/stencil only rendering, no fragment needed
    void linkVertexShader(const std::string& vertexCodePath);
    void linkVertexShader(const std::string& vertexCodePath, const std::string& vertexHeader);

    void linkVertexGeometry(const std::string& vertexCodePath, const std::string& geometryCodePath);
    void linkVertexGeometry(const std::string& vertexCodePath, const std::string& geometryCodePath,
        const std::string& vertexHeader, const std::string& geometryHeader);

    void bind() const;

    template<typename T>
    bool setUniform(const std::string& uniformName, const T& v) const ;

    template<typename T>
    bool setUniformArray(const std::string& uniformName, int n, const T* a) const;

    GLuint getProgramID() const {return m_programID;}

private:

    GLuint m_programID;

    static GLuint loadShader(GLenum shaderType, const std::string& codePath);
    static GLuint loadShader(GLenum shaderType, const std::string& codePath, const std::string& header);
};

#endif // SHADER_H_
