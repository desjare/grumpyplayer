#include "precomp.h"
#include "gui.h"
#include "logger.h"
#include "chrono.h"
#include "icon.h"

#include <lodepng/picopng.h>
#include <map>

namespace {

    std::map<GLFWwindow*, gui::Handle*> handles;

    void ErrorCallback(int error, const char* description)
    {
        logger::Error("UI %d: %s", error, description);
    }

    void WindowSizeCallback(GLFWwindow* window, int width, int height)
    {
        gui::Handle* handle = handles[window];
        handle->width = width;
        handle->height = height;

        handle->sizeChangeCb(handle, width, height);
    }

    void DropCallback(GLFWwindow* window, int count, const char** paths)
    {
        gui::Handle* handle = handles[window];
        if( count > 1 || count == 0 )
        {
            return;
        }
        handle->fileDropCb(handle, paths[0]);
    }

    void ToggleFullScreen(GLFWwindow* window)
    {
        gui::Handle* handle = handles[window];

        if( gui::IsFullScreen(handle) )
        {
            glfwSetWindowMonitor( handle->window, NULL,  handle->posx, handle->posy, handle->backupWidth, handle->backupHeight, 0 );
        }
        else
        {
            // backup window position and window size
            glfwGetWindowPos( handle->window, &handle->posx, &handle->posy );
            glfwGetWindowSize( handle->window, &handle->backupWidth, &handle->backupHeight );

            // get reolution of monitor
            GLFWmonitor* monitor = glfwGetPrimaryMonitor();
            const GLFWvidmode* mode = glfwGetVideoMode(monitor);

            // switch to full screen
            glfwSetWindowMonitor( handle->window, monitor, 0, 0, mode->width, mode->height, 0 );
        }
    }

    void MouseDoubleClick(GLFWwindow* window)
    {
        ToggleFullScreen(window);
    }

    void MouseButtonCallback(GLFWwindow* window, int button, int action, int mods)
    {
        gui::Handle* handle = handles[window];

        if( button == GLFW_MOUSE_BUTTON_LEFT )
        {
            if( action == GLFW_PRESS )
            {
                handle->mouseButtonPress = true;
                handle->mouseButtonShift = mods & GLFW_MOD_SHIFT;

                if( handle->mouseButtonShift )
                {
                    const double seekPercent 
                                      = static_cast<double>(handle->posx) / static_cast<double>(handle->width);

                    logger::Debug("Seeking %f %%", seekPercent);
                    handle->seekCb(handle, seekPercent);
                }
            }
            else if( action == GLFW_RELEASE ) 
            {
                handle->mouseButtonPress = false;
                handle->mouseButtonShift = mods & GLFW_MOD_SHIFT;

                // double click handling
                if( handle->mouseReleaseTimeUs == 0 )
                {
                    handle->mouseReleaseTimeUs = chrono::Now();
                }
                else
                {
                    uint64_t diffUs = chrono::Now() - handle->mouseReleaseTimeUs;
                    double diffMs = chrono::Milliseconds(diffUs);
                    if(diffMs >10.0 && diffMs < 200.0)
                    {
                        MouseDoubleClick(window);
                    }
                    handle->mouseReleaseTimeUs = 0;
                }
            }

        }
    }

    void CursorPositionCallback(GLFWwindow* window, double xpos, double ypos)
    {
        gui::Handle* handle = handles[window];

        handle->posx = static_cast<int32_t>(xpos);
        handle->posy = static_cast<int32_t>(ypos);
    }

    void KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
    {
        gui::Handle* handle = handles[window];
        if(action == GLFW_PRESS)
        {
            switch(key)
            {
                case GLFW_KEY_F:
                    ToggleFullScreen(window);
                    break;
                case GLFW_KEY_SPACE:
                    handle->pauseCb(handle);
                    break;
            }
        }
    }
}

namespace gui
{
    Result Init()
    {
        Result result;
        if(!glfwInit() )
        {
            result = Result(false, std::string("gwfw failed to init"));
            return result;
        }

        glfwSetErrorCallback(ErrorCallback);

        return result;
    }

    Result Create(Handle*& handle)
    {
        Result result;
        handle = new Handle();
        memset(handle,0, sizeof(Handle));

        return result;
    }

    Result OpenWindow(Handle* handle, uint32_t width, uint32_t height)
    {
        Result result;

        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
        glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

        handle->window = glfwCreateWindow(width, height, "grumpy", NULL, NULL);
        if(!handle->window)
        {
            result = Result(false, std::string("gwfw failed to open window"));
            return result;
        }
        handle->width = width;
        handle->height = height;

        glfwSetKeyCallback(handle->window, KeyCallback);
        glfwSetMouseButtonCallback(handle->window, MouseButtonCallback);
        glfwSetCursorPosCallback(handle->window, CursorPositionCallback);
        glfwSetDropCallback(handle->window, DropCallback);

        glfwMakeContextCurrent(handle->window);

        std::vector<unsigned char> imageBuffer;
        unsigned long iconWidth;
        unsigned long iconHeight;

        int outcome = decodePNG(imageBuffer, iconWidth, iconHeight, icon_png, icon_png_len);
        if( outcome == 0 )
        {
            GLFWimage image;
            image.width = iconWidth;
            image.height = iconHeight;
            image.pixels = &imageBuffer[0];
            glfwSetWindowIcon(handle->window, 1, &image);

        }


        handles[handle->window] = handle;

        return result;
    }

    void SetTitle(Handle* handle, const char* title)
    {
        glfwSetWindowTitle(handle->window, title);
    }

    void SetWindowSizeChangeCallback(Handle* handle, WindowSizeChangeCb cb)
    {
        handle->sizeChangeCb = cb;
        glfwSetWindowSizeCallback(handle->window, WindowSizeCallback);
    }

    void SetFileDropCallback(Handle* handle, FileDropCb cb)
    {
        handle->fileDropCb = cb;
    }

    void SetSeekCallback(Handle* handle, SeekCb seekCb)
    {
        handle->seekCb = seekCb;
    }

    void SetPauseCallback(Handle* handle, PauseCb pauseCb)
    {
        handle->pauseCb = pauseCb;
    }

    void SetWindowSize(Handle* handle, uint32_t width, uint32_t height)
    {
        glfwSetWindowSize(handle->window, width, height);
    }

    bool IsFullScreen(Handle* handle)
    {
        return glfwGetWindowMonitor(handle->window)  != NULL;
    }

    void SwapBuffers(Handle* handle)
    {
        glfwSwapBuffers(handle->window);
    }

    bool ShouldClose(Handle* handle)
    {
        return glfwGetKey(handle->window, GLFW_KEY_ESCAPE) || glfwWindowShouldClose(handle->window);
    }

    void PollEvents()
    {
        glfwPollEvents();
    }

    void Destroy()
    {
        glfwTerminate(); 
    }
}



