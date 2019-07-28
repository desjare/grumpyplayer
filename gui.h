
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

        int32_t posx;
        int32_t posy;
        int32_t width;
        int32_t height;

        int32_t backupWidth;
        int32_t backupHeight;
    };

    typedef boost::function<void (Handle*, uint32_t, uint32_t)> WindowSizeChangeCb;

    Result Init();
    Result Create(Handle*& handle);
    Result OpenWindow(Handle* handle, uint32_t width, uint32_t height);

    void   SetWindowSizeChangeCallback(Handle*, WindowSizeChangeCb);
    bool   IsFullScreen(Handle* handle);
    void   SwapBuffers(Handle* handle);
    
    bool   ShouldClose(Handle* handle);
    void   PollEvents();
    
    void   Destroy();
}
