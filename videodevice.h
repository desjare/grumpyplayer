#pragma once

#ifdef WIN32
#include <windows.h>
#endif

#include <GL/gl.h>
#include "result.h"


namespace videodevice
{
    struct Device
    {
        enum Attribs {
            VERTICES = 0,
            TEX_COORDS,
            NB_ATTRIBS
        };

        enum Uniforms {
            MVP_MATRIX = 0,
            FRAME_TEX,
            NB_UNIFORMS
        };

        // texture size
        GLuint width;
        GLuint height;

        // rendering
        GLuint vertexArray;
        GLuint vertexBuffer;
        GLuint elementBuffer;
        GLuint frameTexture;
        GLuint program;
        GLuint attribs[NB_ATTRIBS];
        GLuint uniforms[NB_UNIFORMS];
    };

    Result Init();
    Result Create(Device*& device, uint32_t width, uint32_t height);
    Result DrawFrame(Device* device, uint8_t* frameBuffer, uint32_t width, uint32_t height);
    Result SetVideoSize(Device* device, uint32_t width, uint32_t height);
    Result SetWindowSize(Device* device, uint32_t width, uint32_t height);
    void   Destroy(Device* device);
}



