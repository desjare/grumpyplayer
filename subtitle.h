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


    Result Parse(const std::string& ssa, SubStationAlphaHeader*& header);

};
