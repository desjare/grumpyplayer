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
        virtual Result Draw(FrameBuffer*) = 0;
        virtual Result SetTextureSize(uint32_t width, uint32_t height) = 0;
        virtual Result SetWindowSize(uint32_t width, uint32_t height) = 0;
    };

    struct TextCharacter 
    {
        GLuint texture =  0;           // ID handle of the glyph texture
        glm::ivec2 size = { 0, 0 };    // Size of glyph
        glm::ivec2 bearing = { 0, 0 }; // Offset from baseline to left/top of glyph
        GLuint advance = 0;            // Horizontal offset to advance to next glyph
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
        Renderer* renderer = NULL;

        // text
        FT_Library ft;
        FT_Face face;

        GLuint textProgram;
        GLuint textVertexBuffer;
        GLuint textVertexArray;
        GLuint textUniformColor;
        GLuint textUniformProjection;

        std::map<char, TextCharacter> textCharacters;
    };

    Result Init();
    void   GetSupportedFormat(VideoFormatList&) ;

    Result Create(Device*& device, VideoFormat outputFormat);

    Result DrawFrame(Device* device, FrameBuffer*);
    Result DrawText(Device* device, const std::string& text, float x, float y, float scale, glm::vec3 color);

    Result SetTextureSize(Device* device, uint32_t width, uint32_t height);
    Result SetWindowSize(Device* device, uint32_t width, uint32_t height);
    
    void   Destroy(Device*& device);
}



