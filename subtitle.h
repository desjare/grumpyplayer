#pragma once

#include <glm/glm.hpp>
#include <map>

#include "result.h"

namespace subtitle
{
    // https://www.matroska.org/technical/specs/subtitles/ssa.html
    struct SubStationAlphaHeader
    {
        // scrypt info
        uint32_t playResX = 0;
        uint32_t playResY = 0;
 
        // style
        std::string name;
        std::string fontName;
        uint32_t fontSize = 0;
        glm::vec3 primaryColor;
        glm::vec3 secondaryColor;
        glm::vec3 outlineColor;

        std::map<std::string, uint32_t> styleFormatFieldPos;

        // events
        std::map<std::string, uint32_t> eventFormatFieldPos;
    };

    struct SubStationAlphaDialogue
    {
        std::string layer;
        std::string text;

        uint64_t startTimeUs;
        uint64_t endTimeUs;

        std::string style;
        std::string name;

        uint32_t marginL;
        uint32_t marginR;
        uint32_t marginV;
        
        std::string effect;
    };


    Result Parse(const std::string& ssa, SubStationAlphaHeader*& header);
    Result Parse(const std::string& ssa, SubStationAlphaHeader* header, SubStationAlphaDialogue*& dialogue);

};
