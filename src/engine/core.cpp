#include "core.h"

void GLFW_check_error()
{
    const char *description;
    int code = glfwGetError(&description);
    std::cout << description << std::endl;
}

void GLclearError()
{
    while (glGetError() != GL_NO_ERROR)
        ;
}

bool GLlogCall(const char *function, const char *file, int line)
{
    while (GLenum error = glGetError())
    {
        std::cout << "[OpenGL Error] (" << error << ")" << function << " " << file << ":" << line << std::endl;
        return false;
    }
    return true;
}