
#pragma once

#define GLFW_INCLUDE_GL3
#define GLFW_NO_GLU
#include <GLFW/glfw3.h>

#include "result.h"

namespace gui
{
    struct Handle
    {
        GLFWwindow* window;
    };

    Result Init();
    Result Create(Handle*& handle);
    Result OpenWindow(Handle* handle, uint32_t width, uint32_t height);
    void   SwapBuffers(Handle* handle);
    bool   ShouldClose(Handle* handle);
    void   PollEvents();
    void   Destroy();
}
