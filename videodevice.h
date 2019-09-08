#pragma once

#include <GL/gl.h>
#include <glm/glm.hpp>

#include <freetype2/ft2build.h>
#include FT_FREETYPE_H

#include <map>

#include "result.h"
#include "mediaformat.h"

namespace videodevice
{
    static const uint32_t NUM_FRAME_DATA_POINTERS = 4;
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
        virtual Result SetWindowSize(uint32_t width, uint32_t height) = 0;
    };

    struct FrameRenderer : public Renderer
    {
        virtual Result Render(FrameBuffer*) = 0;
        virtual Result SetTextureSize(uint32_t width, uint32_t height) = 0;
    };

    struct TextRenderer : public Renderer
    {
        virtual Result Render(const std::string& text, uint32_t fontSize, float x, float y, float scale, glm::vec3 color) = 0;
        virtual Result GetSize(const std::string& text, uint32_t fontSize, float& w, float& h) = 0;
    };

    struct Device
    {
        // texture size
        GLuint width = 0;
        GLuint height = 0;

        // video size
        GLuint windowWidth = 0;
        GLuint windowHeight = 0;

        // video renderer
        FrameRenderer* renderer = nullptr;

        // text renderer
        TextRenderer* text = nullptr;
    };

    Result Init();
    void   GetSupportedFormat(VideoFormatList&) ;

    Result Create(Device*& device, VideoFormat outputFormat);

    Result DrawFrame(Device* device, FrameBuffer*);

    Result DrawText(Device* device, const std::string& text, uint32_t fontSize, float x, float y, float scale, glm::vec3 color);
    Result GetTextSize(Device* device, const std::string& text, uint32_t fontSize, float& w, float& h);

    Result SetTextureSize(Device* device, uint32_t width, uint32_t height);
    Result SetWindowSize(Device* device, uint32_t width, uint32_t height);
    Result GetWindowSize(Device* device, uint32_t& width, uint32_t& height);
    
    void   Destroy(Device*& device);
}



