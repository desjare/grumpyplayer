#pragma once

#ifdef WIN32
#pragma warning( push )
#pragma warning( disable : 26495) // uninitialized variable
#endif
#include <boost/function.hpp>
#ifdef WIN32
#pragma warning( pop ) 
#endif

#include <glm/glm.hpp>
#include <map>

#include "result.h"

namespace subtitle
{
    typedef boost::function<Result (const std::string& text, const std::string& fontName, uint32_t fontSize, float& w, float& h)> GetTextSizeCb;

    enum BorderStyle
    {
        OUTLINE,
        OPAQUE
    };

    enum Alignment
    {
        LEFT,
        CENTERED,
        RIGHT,
        LEFT_TOPTITLE,
        CENTERED_TOPTITLE,
        RIGHT_TOPTITLE,
        LEFT_MIDDLETITLE,
        RIGHT_MIDDLETITLE,
        CENTERED_MIDDLETITLE
    };

    struct Line
    {
        std::string text;
        float x = 0.0f;
        float y = 0.0f;
        float w = 0.0f;
        float h = 0.0f;
    };

    typedef std::vector<Line> SubLineList;

    // https://www.matroska.org/technical/specs/subtitles/ssa.html
    struct SubStationAlphaStyle
    {
        // style
        std::string name;
        std::string fontName = "Arial";
        uint32_t fontSize = 48;
        // rgba colors
        glm::vec4 primaryColor = {1.0f, 1.0f, 1.0f, 1.0f};
        glm::vec4 secondaryColor = {1.0f, 1.0f, 1.0f, 1.0f};
        glm::vec4 outlineColor  {1.0f, 1.0f, 1.0f, 1.0f};
        glm::vec4 backColor = {1.0f, 1.0f, 1.0f, 1.0f};
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
        uint32_t marginL = 10;
        uint32_t marginR = 10;
        uint32_t marginV = 10;
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

    typedef std::vector<Line> SubStationAlphaLineList;

    // .srt files https://en.wikipedia.org/wiki/SubRip

    struct SubRipDialogue
    {
        uint32_t counter = 0;
        uint64_t startTimeUs = 0;
        uint64_t endTimeUs = 0;
        SubLineList lines; 
    };
    typedef std::vector<SubRipDialogue> SubRipDialogueList;

    struct SubRip
    {
        SubRipDialogueList diags;
    };


    // ssa / ass
    Result Parse(const std::string& ssa, SubStationAlphaHeader*& header);
    Result Parse(const std::string& ssa, SubStationAlphaHeader* header, SubStationAlphaDialogue*& dialogue);

    // src
    Result Parse(const std::string& path, SubRip*& subRip);

    Result GetDisplayInfo(const std::vector<std::string>& lines,
                          GetTextSizeCb& textSizeCb,
                          uint32_t windowWidth,
                          uint32_t windowHeight,
                          SubStationAlphaHeader* header,
                          SubStationAlphaDialogue* dialogue, 
                          uint64_t& startTimeUs,
                          uint64_t& endTimeUs,
                          std::string& fontName,
                          uint32_t& fontSize,
                          glm::vec3& color, 
                          SubLineList& sublines);



};
