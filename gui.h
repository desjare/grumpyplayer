
#pragma once

#include <boost/function.hpp>

#define GLFW_INCLUDE_GL3
#define GLFW_NO_GLU
#include <GLFW/glfw3.h>

#include "result.h"

namespace gui
{
    struct Handle;

    typedef boost::function<void (Handle*, uint32_t, uint32_t)> WindowSizeChangeCb;
    typedef boost::function<void (Handle*, const std::string&)> FileDropCb;
    typedef boost::function<void (Handle*, double)> SeekCb;
    typedef boost::function<void (Handle*)> PauseCb;

    struct Handle
    {
        GLFWwindow* window;

        WindowSizeChangeCb sizeChangeCb;
        FileDropCb fileDropCb;
        SeekCb seekCb;
        PauseCb pauseCb;

        int32_t posx;
        int32_t posy;
        int32_t width;
        int32_t height;

        int32_t backupWidth;
        int32_t backupHeight;

        bool mouseButtonPress;
        bool mouseButtonShift;
        uint64_t mouseReleaseTimeUs;
    };


    Result Init();
    Result Create(Handle*& handle);
    Result OpenWindow(Handle* handle, uint32_t width, uint32_t height);
    void   SetTitle(Handle* handle, const char* title);

    void   SetWindowSizeChangeCallback(Handle*, WindowSizeChangeCb);
    void   SetFileDropCallback(Handle*, FileDropCb);
    void   SetSeekCallback(Handle*, SeekCb);
    void   SetPauseCallback(Handle*, PauseCb);


    bool   IsFullScreen(Handle* handle);
    void   SetWindowSize(Handle*, uint32_t width, uint32_t height);
    void   SwapBuffers(Handle* handle);
    
    void   ShowWindow(Handle* handle);
    void   HideWindow(Handle* handle);

    bool   ShouldClose(Handle* handle);
    void   PollEvents();
    
    void   Destroy();
}
