#pragma once

#ifdef WIN32
#include <windows.h>
#endif

#include <GL/gl.h>
#include "result.h"
#include "mediaformat.h"


namespace videodevice
{
    static const uint32_t NUM_FRAME_DATA_POINTERS = 3;
    struct Device;

    struct FrameBuffer
    {
        uint8_t* frameData[NUM_FRAME_DATA_POINTERS];
        uint32_t lineSize[NUM_FRAME_DATA_POINTERS];
        uint32_t width;
        uint32_t height;
    };

    struct Renderer
    {
        virtual ~Renderer(){}

        virtual Result Create() = 0;
        virtual Result Draw(FrameBuffer*) = 0;
        virtual Result SetVideoSize(uint32_t width, uint32_t height) = 0;
        virtual Result SetWindowSize(uint32_t width, uint32_t height) = 0;
    };

    struct Device
    {
        // texture size
        GLuint width;
        GLuint height;

        // video size
        GLuint windowWidth;
        GLuint windowHeight;

        Renderer* renderer;
    };

    Result Init();
    Result Create(Device*& device, VideoFormat outputFormat);
    Result DrawFrame(Device* device, FrameBuffer*);
    Result SetVideoSize(Device* device, uint32_t width, uint32_t height);
    Result SetWindowSize(Device* device, uint32_t width, uint32_t height);
    void   Destroy(Device*& device);
}



