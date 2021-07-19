 #include "render_debug.h"

 #include <GL/glew.h>

 namespace VKR_Debug {

 void checkError(const std::string& callString) {
    GLenum err = glGetError();
    if (err) {
        std::string str = "GL Error after call " + callString + ": ";
        switch(err) {
        case GL_INVALID_ENUM:
            str += "GL_INVALID_ENUM";
            break;
        case GL_INVALID_VALUE:
            str += "GL_INVALID_VALUE";
            break;

        case GL_INVALID_OPERATION:
            str += "GL_INVALID_OPERATION";
            break;

        case GL_INVALID_FRAMEBUFFER_OPERATION:
            str += "GL_INVALID_FRAMEBUFFER_OPERATION";
            break;

        case GL_OUT_OF_MEMORY:
            str += "GL_OUT_OF_MEMORY";
            break;

        case GL_STACK_UNDERFLOW:
            str += "GL_STACK_UNDERFLOW";
            break;

        case GL_STACK_OVERFLOW:
            str += "GL_STACK_OVERFLOW";
            break;
        default :
            str += "Unknown error " + std::to_string(err);
            break;
        }
        VKR_DEBUG_PRINT(str)
    }
}

}
