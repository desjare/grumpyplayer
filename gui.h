
#pragma once

#ifdef WIN32
#pragma warning( push )
#pragma warning( disable : 26495) // uninitialized variable
#endif
#include <boost/function.hpp>
#ifdef WIN32
#pragma warning( pop ) 
#endif

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
    typedef boost::function<void (Handle*)> SubtitleCb;

    struct Handle
    {
        GLFWwindow* window = NULL;

        WindowSizeChangeCb sizeChangeCb;
        FileDropCb fileDropCb;
        SeekCb seekCb;
        PauseCb pauseCb;
        SubtitleCb subtitleCb;

        int32_t posx = 0;
        int32_t posy = 0;
        int32_t width = 0;
        int32_t height = 0;

        int32_t backupWidth = 0;
        int32_t backupHeight = 0;

        bool mouseButtonPress = false;
        bool mouseButtonShift = false;
        uint64_t mouseReleaseTimeUs = 0;
    };


    Result Init();
    Result Create(Handle*& handle);

    Result OpenWindow(Handle* handle, uint32_t width, uint32_t height);
    
    void   SetTitle(Handle* handle, const char* title);

    // ui callbacks
    void   SetWindowSizeChangeCallback(Handle*, WindowSizeChangeCb);
    void   SetFileDropCallback(Handle*, FileDropCb);
    void   SetSeekCallback(Handle*, SeekCb);
    void   SetPauseCallback(Handle*, PauseCb);
    void   SetSubtitleCallback(Handle*, SubtitleCb);

    // windows state
    bool   IsFullScreen(Handle* handle);
    void   SetWindowSize(Handle*, uint32_t width, uint32_t height);
    void   SwapBuffers(Handle* handle);
    
    void   ShowWindow(Handle* handle);
    void   HideWindow(Handle* handle);

    bool   ShouldClose(Handle* handle);

    void   PollEvents();
    void   Destroy();
}
