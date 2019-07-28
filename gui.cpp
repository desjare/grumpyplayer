
#include "gui.h"
#include "logger.h"

#include <map>

namespace {

    std::map<GLFWwindow*, gui::Handle*> handles;

    gui::WindowSizeChangeCb windowSizeChangeCb;
    
    void ErrorCallback(int error, const char* description)
    {
        logger::Error("UI %d: %s", error, description);
    }

    void WindowSizeCallback(GLFWwindow* window, int width, int height)
    {
        gui::Handle* handle = handles[window];
        handle->width = width;
        handle->height = height;

        windowSizeChangeCb(handle, width, height);
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


    void KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
    {
        if(action == GLFW_PRESS)
        {
            switch(key)
            {
                case GLFW_KEY_F:
                    ToggleFullScreen(window);
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

        glfwSetKeyCallback(handle->window, KeyCallback);
        glfwMakeContextCurrent(handle->window);

        handles[handle->window] = handle;

        return result;
    }

    void SetWindowSizeChangeCallback(Handle* handle, WindowSizeChangeCb cb)
    {
        windowSizeChangeCb = cb;
        glfwSetWindowSizeCallback(handle->window, WindowSizeCallback);
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



