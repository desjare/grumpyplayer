
#pragma once

#include <boost/function.hpp>

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

    typedef boost::function<void (Handle*, uint32_t, uint32_t)> WindowSizeChangeCb;

    Result Init();
    Result Create(Handle*& handle);
    Result OpenWindow(Handle* handle, uint32_t width, uint32_t height);
    void   SetWindowSizeChangeCallback(Handle*, WindowSizeChangeCb);
    void   SwapBuffers(Handle* handle);
    bool   ShouldClose(Handle* handle);
    void   PollEvents();
    void   Destroy();
}
