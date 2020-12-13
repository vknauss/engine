/**
 * Shader class by Vincent Knauss
 * 2018-2019
 **/

#include "shader.h"

#include <cstdio>
#include <fstream>
#include <iostream>
#include <sstream>
#include <vector>

#include <glm/gtc/type_ptr.hpp>

void link(GLuint programID, const std::initializer_list<GLuint>& shaders) {
    for(GLuint shader : shaders) {
        glAttachShader(programID, shader);
    }
    glLinkProgram(programID);

    for(GLuint shader : shaders) {
        glDetachShader(programID, shader);
        glDeleteShader(shader);
    }

    // check shader program
    GLint linkStatus;
    glGetProgramiv(programID, GL_LINK_STATUS, &linkStatus);
    if(linkStatus != GL_TRUE){
        GLint infoLogLength;
        glGetProgramiv(programID, GL_INFO_LOG_LENGTH, &infoLogLength);
        std::vector<char> infoLog(infoLogLength+1);
        glGetProgramInfoLog(programID, infoLogLength, &infoLogLength, &infoLog[0]);
        throw std::runtime_error("Shader program link failed. Info log: " + std::string(infoLog.data()));
    }
}

Shader::Shader() {
    m_programID = glCreateProgram();
}

Shader::~Shader() {
    glDeleteProgram(m_programID);
}

void Shader::linkShaderFiles(const std::string& vertexCodePath, const std::string& fragmentCodePath) {
    // Load vertex and fragment shaders
    GLuint vertexShaderID = loadShader(GL_VERTEX_SHADER, vertexCodePath);
    GLuint fragmentShaderID = loadShader(GL_FRAGMENT_SHADER, fragmentCodePath);

    link(m_programID, {vertexShaderID, fragmentShaderID});
}

void Shader::linkShaderFiles(const std::string& vertexCodePath, const std::string& fragmentCodePath, const std::string& geometryCodePath) {
    // Load vertex and fragment shaders
    GLuint vertexShaderID = loadShader(GL_VERTEX_SHADER, vertexCodePath);
    GLuint fragmentShaderID = loadShader(GL_FRAGMENT_SHADER, fragmentCodePath);
    GLuint geometryShaderID = loadShader(GL_GEOMETRY_SHADER, geometryCodePath);

    link(m_programID, {vertexShaderID, fragmentShaderID, geometryShaderID});
}

void Shader::linkShaderFiles(const std::string& vertexCodePath, const std::string& fragmentCodePath, const std::string& vertexHeader, const std::string& fragmentHeader) {
    // Load vertex and fragment shaders
    GLuint vertexShaderID = loadShader(GL_VERTEX_SHADER, vertexCodePath, vertexHeader);
    GLuint fragmentShaderID = loadShader(GL_FRAGMENT_SHADER, fragmentCodePath, fragmentHeader);

    link(m_programID, {vertexShaderID, fragmentShaderID});
}

void Shader::linkShaderFiles(const std::string& vertexCodePath, const std::string& fragmentCodePath, const std::string& geometryCodePath,
    const std::string& vertexHeader, const std::string& fragmentHeader, const std::string& geometryHeader) {
    // Load vertex and fragment shaders
    GLuint vertexShaderID = loadShader(GL_VERTEX_SHADER, vertexCodePath, vertexHeader);
    GLuint fragmentShaderID = loadShader(GL_FRAGMENT_SHADER, fragmentCodePath, fragmentHeader);
    GLuint geometryShaderID = loadShader(GL_GEOMETRY_SHADER, geometryCodePath, geometryHeader);

    link(m_programID, {vertexShaderID, fragmentShaderID, geometryShaderID});
}

void Shader::linkVertexShader(const std::string& vertexCodePath) {
    GLuint vertexShaderID = loadShader(GL_VERTEX_SHADER, vertexCodePath);

    link(m_programID, {vertexShaderID});
}

void Shader::linkVertexShader(const std::string& vertexCodePath, const std::string& vertexHeader) {
    GLuint vertexShaderID = loadShader(GL_VERTEX_SHADER, vertexCodePath, vertexHeader);

    link(m_programID, {vertexShaderID});
}

void Shader::linkVertexGeometry(const std::string& vertexCodePath, const std::string& geometryCodePath) {
    GLuint vertexShaderID = loadShader(GL_VERTEX_SHADER, vertexCodePath);
    GLuint geometryShaderID = loadShader(GL_GEOMETRY_SHADER, geometryCodePath);

    link(m_programID, {vertexShaderID, geometryShaderID});
}

void Shader::linkVertexGeometry(const std::string& vertexCodePath, const std::string& geometryCodePath,
    const std::string& vertexHeader, const std::string& geometryHeader) {
    GLuint vertexShaderID = loadShader(GL_VERTEX_SHADER, vertexCodePath, vertexHeader);
    GLuint geometryShaderID = loadShader(GL_GEOMETRY_SHADER, geometryCodePath, geometryHeader);

    link(m_programID, {vertexShaderID, geometryShaderID});
}

void Shader::bind() const {
    glUseProgram(m_programID);
}


template<>
bool Shader::setUniform(const std::string& uniformName, const int& v) const {
    GLint upos = glGetUniformLocation(m_programID, uniformName.c_str());
    if(upos<0) return false;
    glUniform1i(upos, v);
    return true;
}

template<>
bool Shader::setUniform(const std::string& uniformName, const unsigned& v) const {
    GLint upos = glGetUniformLocation(m_programID, uniformName.c_str());
    if(upos<0) return false;
    glUniform1ui(upos, v);
    return true;
}

template<>
bool Shader::setUniform(const std::string& uniformName, const float& v) const {
    GLint upos = glGetUniformLocation(m_programID, uniformName.c_str());
    if(upos<0) return false;
    glUniform1f(upos, v);
    return true;
}

template<>
bool Shader::setUniform(const std::string& uniformName, const glm::vec4& v) const {
    GLint upos = glGetUniformLocation(m_programID, uniformName.c_str());
    if(upos<0) return false;
    glUniform4fv(upos, 1, glm::value_ptr(v));
    return true;
}

template<>
bool Shader::setUniform(const std::string& uniformName, const glm::vec3& v) const {
    GLint upos = glGetUniformLocation(m_programID, uniformName.c_str());
    if(upos<0) return false;
    glUniform3fv(upos, 1, glm::value_ptr(v));
    return true;
}

template<>
bool Shader::setUniform(const std::string& uniformName, const glm::vec2& v) const {
    GLint upos = glGetUniformLocation(m_programID, uniformName.c_str());
    if(upos<0) return false;
    glUniform2fv(upos, 1, glm::value_ptr(v));
    return true;
}

template<>
bool Shader::setUniform(const std::string& uniformName, const glm::mat4& v) const {
    GLint upos = glGetUniformLocation(m_programID, uniformName.c_str());
    if(upos<0) return false;
    glUniformMatrix4fv(upos, 1, GL_FALSE, glm::value_ptr(v));
    return true;
}

template<>
bool Shader::setUniform(const std::string& uniformName, const glm::mat3& v) const {
    GLint upos = glGetUniformLocation(m_programID, uniformName.c_str());
    if(upos<0) return false;
    glUniformMatrix3fv(upos, 1, GL_FALSE, glm::value_ptr(v));
    return true;
}

template<>
bool Shader::setUniformArray(const std::string& uniformName, int n, const float* a) const {
    GLint upos = glGetUniformLocation(m_programID, uniformName.c_str());
    if(upos < 0) return false;
    glUniform1fv(upos, n, a);
    return true;
}

template<>
bool Shader::setUniformArray(const std::string& uniformName, int n, const int* a) const {
    GLint upos = glGetUniformLocation(m_programID, uniformName.c_str());
    if(upos < 0) return false;
    glUniform1iv(upos, n, a);
    return true;
}

template<>
bool Shader::setUniformArray(const std::string& uniformName, int n, const glm::vec3* a) const {
    GLint upos = glGetUniformLocation(m_programID, uniformName.c_str());
    if(upos < 0) return false;
    glUniform3fv(upos, n, reinterpret_cast<const GLfloat*>(a));
    return true;
}

template<>
bool Shader::setUniformArray(const std::string& uniformName, int n, const glm::mat3* a) const {
    GLint upos = glGetUniformLocation(m_programID, uniformName.c_str());
    if(upos < 0) return false;
    glUniformMatrix3fv(upos, n, GL_FALSE, reinterpret_cast<const GLfloat*>(a));
    return true;
}

template<>
bool Shader::setUniformArray(const std::string& uniformName, int n, const glm::mat4* a) const {
    GLint upos = glGetUniformLocation(m_programID, uniformName.c_str());
    if(upos < 0) return false;
    glUniformMatrix4fv(upos, n, GL_FALSE, reinterpret_cast<const GLfloat*>(a));
    return true;
}

/**
* Load a single shader for linking
**/
GLuint Shader::loadShader(GLenum shaderType, const std::string& codePath) {
    GLuint shader_id = glCreateShader(shaderType);

    std::ifstream shader_file(codePath);
    if(!shader_file) {
        glDeleteShader(shader_id);
        throw std::runtime_error("Could not open shader code file at " + codePath);
    }
    std::stringstream source_stream;
    source_stream << shader_file.rdbuf();
    std::string source_str = source_stream.str();
    char const* source = source_str.c_str();
    shader_file.close();
    glShaderSource(shader_id, 1, &source, 0);
    GLint compile_status;
    glCompileShader(shader_id);
    glGetShaderiv(shader_id, GL_COMPILE_STATUS, &compile_status);
    if(compile_status != GL_TRUE) {
        GLint info_log_length;
        glGetShaderiv(shader_id, GL_INFO_LOG_LENGTH, &info_log_length);
        std::vector<char> info_log(info_log_length+1);
        glGetShaderInfoLog(shader_id, info_log_length, 0, &info_log[0]);
        glDeleteShader(shader_id);
        throw std::runtime_error("Shader code at " + codePath + " compile failed. Info log: " + std::string(info_log.data()));
    }
    return shader_id;
}

GLuint Shader::loadShader(GLenum shaderType, const std::string& codePath, const std::string& header) {
    GLuint shader_id = glCreateShader(shaderType);

    std::ifstream shader_file(codePath);
    if(!shader_file) {
        glDeleteShader(shader_id);
        throw std::runtime_error("Could not open shader code file at " + codePath);
    }
    std::stringstream source_stream;
    source_stream << shader_file.rdbuf();
    std::string source_str = header + source_stream.str();
    char const* source = source_str.c_str();
    shader_file.close();
    glShaderSource(shader_id, 1, &source, 0);
    GLint compile_status;
    glCompileShader(shader_id);
    glGetShaderiv(shader_id, GL_COMPILE_STATUS, &compile_status);
    if(compile_status != GL_TRUE) {
        GLint info_log_length;
        glGetShaderiv(shader_id, GL_INFO_LOG_LENGTH, &info_log_length);
        std::vector<char> info_log(info_log_length+1);
        glGetShaderInfoLog(shader_id, info_log_length, 0, &info_log[0]);
        glDeleteShader(shader_id);
        throw std::runtime_error("Shader code at " + codePath + " compile failed. Info log: " + std::string(info_log.data()));
    }
    return shader_id;
}
