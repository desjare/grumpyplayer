#pragma once

#include <glm/glm.hpp>
#include <map>

#include "result.h"

namespace subtitle
{
    enum BorderStyle
    {
        OUTLINE,
        OPAQUE
    };

    enum Alignment
    {
        LEFT,
        CENTERED,
        RIGHT
    };

    // https://www.matroska.org/technical/specs/subtitles/ssa.html
    struct SubStationAlphaStyle
    {
        // style
        std::string name;
        std::string fontName;
        uint32_t fontSize = 0;
        // rgb colors
        glm::vec3 primaryColor = {1.0f, 1.0f, 1.0f};
        glm::vec3 secondaryColor = {1.0f, 1.0f, 1.0f};
        glm::vec3 outlineColor;
        glm::vec3 backColor;
        bool bold = false;
        bool italic = false;
        bool underline = false;
        bool strikeout = false;
        uint32_t scaleXPercent = 100;
        uint32_t scaleYPercent = 100;
        uint32_t pixelSpacing = 0;
        uint32_t degreAngle = 0;
        BorderStyle borderStyle = OUTLINE;
        uint32_t outline = 0;
        uint32_t shadow = 0;
        Alignment alignment = CENTERED;
        uint32_t marginL = 0;
        uint32_t marginR = 0;
        uint32_t marginV = 0;
        float alphaLevel = 1.0f;
        std::string encoding;
    };


    struct SubStationAlphaHeader
    {
        // scrypt info
        uint32_t playResX = 0;
        uint32_t playResY = 0;
 
        std::map<std::string, SubStationAlphaStyle> styles;

        std::map<std::string, uint32_t> styleFormatFieldPos;
        std::map<std::string, uint32_t> eventFormatFieldPos;
    };

    struct SubStationAlphaDialogue
    {
        std::string layer;
        std::string text;

        uint64_t startTimeUs = 0;
        uint64_t endTimeUs = 0;

        std::string style;
        std::string name;

        uint32_t marginL = 0;
        uint32_t marginR = 0;
        uint32_t marginV = 0;
        
        std::string effect;
    };


    Result Parse(const std::string& ssa, SubStationAlphaHeader*& header);
    Result Parse(const std::string& ssa, SubStationAlphaHeader* header, SubStationAlphaDialogue*& dialogue);

};
