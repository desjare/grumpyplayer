
#include "gui.h"

namespace {
    gui::WindowSizeChangeCb windowSizeChangeCb;
    
    void ErrorCallback(int error, const char* description)
    {
        fprintf(stderr, "UI Error %d: %s\n", error, description);
    }

    void WindowSizeCallback(GLFWwindow* window, int width, int height)
    {
        gui::Handle handle;
        handle.window = window;
        windowSizeChangeCb(&handle, width, height);
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
        handle->window = NULL;
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

        glfwMakeContextCurrent(handle->window);

        return result;
    }

    void SetWindowSizeChangeCallback(Handle* handle, WindowSizeChangeCb cb)
    {
        windowSizeChangeCb = cb;
        glfwSetWindowSizeCallback(handle->window, WindowSizeCallback);
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



