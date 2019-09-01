#pragma once


#include <map>

#include "result.h"

namespace subtitle
{
    // https://www.matroska.org/technical/specs/subtitles/ssa.html
    struct SubStationAlphaHeader
    {
        std::map<std::string, uint32_t> eventFormatFieldPos;
    };

    struct SubStationAlphaDialogue
    {
        std::string text;

        uint64_t startTimeUs;
        uint64_t endTimeUs;
    };


    Result Parse(const std::string& ssa, SubStationAlphaHeader*& header);
    Result Parse(const std::string& ssa, SubStationAlphaHeader* header, SubStationAlphaDialogue*& dialogue);

};
