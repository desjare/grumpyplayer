
#include "precomp.h"
#include "subtitle.h"
#include "stringext.h"
#include "chrono.h"
#include "logger.h"
#include "filesystem.h"

#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/regex.hpp>
#include <boost/regex.hpp>

#include <vector>
#include <iostream>
#include <fstream>
#include <sstream>

namespace
{
    // https://www.matroska.org/technical/specs/subtitles/ssa.html

    // Script Info
    const char* SECTION_SCRIPT_INFO = "[Script Info]";

    const char* PLAYRESX = "PlayResX:";
    const char* PLAYRESY = "PlayResY:";

    // [V4+ Styles]
    const char* SECTION_V4PSTYLES = "[V4+ Styles]";
    // [V4 Styles]
    const char* SECTION_V4STYLES = "[V4 Styles]";

    const char* STYLE_HEADER = "Style:";

    const char* FORMAT_FONTNAME = "Fontname";
    const char* FORMAT_FONTSIZE = "Fontsize";
    const char* FORMAT_PRIMARYCOLOR = "PrimaryColour";
    const char* FORMAT_SECONDARYCOLOR = "SecondaryColour";
    const char* FORMAT_OUTLINECOLOR = "OutlineColour";
    const char* FORMAT_BACKCOLOR = "BackColour";
    const char* FORMAT_BOLD = "Bold";
    const char* FORMAT_ITALIC = "Italic";
    const char* FORMAT_UNDERLINE = "Underline";
    const char* FORMAT_STRIKEOUT = "StrikeOut";
    const char* FORMAT_SCALEX = "ScaleX";
    const char* FORMAT_SCALEY = "ScaleY";
    const char* FORMAT_SPACING = "Spacing";
    const char* FORMAT_ANGLE = "Angle";
    const char* FORMAT_BORDERSTYLE = "BorderStyle";
    const char* FORMAT_OUTLINE = "Outline";
    const char* FORMAT_SHADOW = "Shadow";
    const char* FORMAT_ALIGNMENT = "Alignment";
    const char* FORMAT_ENCODING = "Encoding";

    // Events
    const char* SECTION_EVENTS = "[Events]";

    const char* FORMAT_HEADER = "Format:";
    const char* FORMAT_LAYER = "Layer";
    const char* FORMAT_START = "Start";
    const char* FORMAT_END = "End";
    const char* FORMAT_STYLE = "Style";
    const char* FORMAT_NAME = "Name";
    const char* FORMAT_MARGINL = "MarginL";
    const char* FORMAT_MARGINR = "MarginR";
    const char* FORMAT_MARGINV = "MarginV";
    const char* FORMAT_EFFECT = "Effect";
    const char* FORMAT_TEXT = "Text";

    // Dialogue
    const char* DIALOGUE = "Dialogue:";

    double EventTimeToSeconds(const std::string& time)
    {
        // format 00:00:00.00
        std::vector<std::string> fields;
        boost::split(fields, time, boost::is_any_of(":"));

        double seconds = 0.0;

        if(fields.size() == 3)
        {
            int32_t hour = std::atoi(fields[0].c_str());
            int32_t min = std::atoi(fields[1].c_str());

            std::vector<std::string> secondsFields;
            boost::split(secondsFields, fields[2], boost::is_any_of("."));

            int32_t sec = std::atoi(secondsFields[0].c_str());
            int32_t milli = 0;
            if(secondsFields.size() > 1)
            {
                milli = std::atoi(secondsFields[1].c_str()) * 10;
            }

            seconds = hour * 60.0 * 60.0;
            seconds += min * 60.0;
            seconds += sec;
            seconds += milli / 1000.0;
        }
        return seconds;
    }

    glm::vec4 StyleColorToColor(const std::string& c)
    {
        const uint32_t abgrSize = 10;
        const size_t size = c.size();

        float a = 1.0f;
        float r = 0.0f;
        float g = 0.0f;
        float b = 0.0f;

        std::string s = c;

        if(s.size() >= 2)
        {
            // erase &H
            s.erase(0, 2);
        }

        // alpha
        if(size == abgrSize)
        {
            std::string rstr = s.substr(0,2);
            a = strtol(rstr.c_str(), nullptr, 16) / 255.0f;
            s.erase(0, 2); 
        }

        // grayscale
        if(s.size() == 1)
        {
            b = r = g = strtol(s.c_str(), nullptr, 16) / 255.0f;
        }

        // BBGGRR
        // ffffff
        // ff
        if(s.size() >= 2)
        {
            // b
            std::string rstr = s.substr(0,2);
            b = r = g = strtol(rstr.c_str(), nullptr, 16) / 255.0f;

            // erase b
            s.erase(0, 2); 

            if(s.size() >= 2)
            {
                // g
                std::string gstr = s.substr(0,2);
                g = strtol(gstr.c_str(), nullptr, 16) / 255.0f;

                // erase g
                s.erase(0, 2); 

                if(s.size() >= 2)
                {
                    // r
                    std::string bstr = s.substr(0,2);
                    r = strtol(bstr.c_str(), nullptr, 16) / 255.0f;
                }
            }
        }

        return glm::vec4(r,g,b,a);
    }

    void FetchField(const char* fieldName, std::string& field, std::map<std::string, uint32_t>& pos, std::vector<std::string>& fields)
    {
        auto it = pos.find(fieldName);
        if(it != pos.end())
        {
            field = trimeol(fields[it->second]);
        }
        else
        {
            logger::Error("Could not find field %s", fieldName);
        }
    }

    void FetchField(const char* fieldName, uint32_t& field, std::map<std::string, uint32_t>& pos, std::vector<std::string>& fields)
    {
        auto it = pos.find(fieldName);
        if(it != pos.end())
        {
            field = std::atoi(fields[it->second].c_str());
        }
        else
        {
            logger::Error("Could not find field %s", fieldName);
        }
    }

    void FetchField(const char* fieldName, bool& field, std::map<std::string, uint32_t>& pos, std::vector<std::string>& fields)
    {
        auto it = pos.find(fieldName);
        if(it != pos.end())
        {
            int32_t v = std::atoi(fields[it->second].c_str());
            if(v == 0)
            {
                field = false;
            }
            else
            {
                field = true;
            }

        }
        else
        {
            logger::Error("Could not find field %s", fieldName);
        }
    }

    void FetchField(const char* fieldName, glm::vec4& field, std::map<std::string, uint32_t>& pos, std::vector<std::string>& fields)
    {
        auto it = pos.find(fieldName);
        if(it != pos.end())
        {
            field = StyleColorToColor(fields[it->second].c_str());
        }
        else
        {
            logger::Error("Could not find field %s", fieldName);
        }
    }

    void FetchField(const char* fieldName, subtitle::Alignment& field, std::map<std::string, uint32_t>& pos, std::vector<std::string>& fields)
    {
        auto it = pos.find(fieldName);
        if(it != pos.end())
        {
            int32_t v = std::atoi(fields[it->second].c_str());

            if(v == 1)
            {
                field = subtitle::LEFT;
            }
            else if(v == 2)
            {
                field = subtitle::CENTERED;
            }
            else if(v == 3)
            {
                field = subtitle::RIGHT;
            }
            else if(v == 5) // 1 + 4 for TOPTITLE
            {
                field = subtitle::LEFT_TOPTITLE;
            }
            else if(v == 6)
            {
                field = subtitle::CENTERED_TOPTITLE;
            }
            else if(v == 7)
            {
                field = subtitle::RIGHT_TOPTITLE;
            }
            else if(v == 9) // 1 + 8 for MIDDLETITLE
            { 
                field = subtitle::LEFT_MIDDLETITLE;
            }
            else if(v == 10)
            {
                field = subtitle::CENTERED_MIDDLETITLE;
            }
            else if(v == 11)
            {
                field = subtitle::RIGHT_MIDDLETITLE;
            }
            else
            {
                logger::Error("Unknown alignment field %d", v);
                assert(0);
            }
        }
        else
        {
            logger::Error("Could not find field %s", fieldName);
        }

    }

    void FetchField(const char* fieldName, subtitle::BorderStyle& field, std::map<std::string, uint32_t>& pos, std::vector<std::string>& fields)
    {
        auto it = pos.find(fieldName);
        if(it != pos.end())
        {
            int32_t v = std::atoi(fields[it->second].c_str());

            if(v == 1)
            {
                field = subtitle::OUTLINE;
            }
            else if(v == 3)
            {
                field = subtitle::OPAQUE;
            }
            else
            {
                logger::Error("Unknown border style field %d", v);
                assert(0);
            }
        }
        else
        {
            logger::Error("Could not find field %s", fieldName);
        }

    }

    void FetchFieldTime(const char* fieldName, uint64_t& field, std::map<std::string, uint32_t>& pos, std::vector<std::string>& fields)
    {
        auto it = pos.find(fieldName);
        if(it != pos.end())
        {
            field = chrono::Microseconds(EventTimeToSeconds(fields[it->second]));
        }
        else
        {
            logger::Error("Could not find field %s", fieldName);
        }
    }

    void ParseStyle(subtitle::SubStationAlphaHeader* header, subtitle::SubStationAlphaStyle* style, const std::string& line)
    {
        std::string styleLine = line;
        trim(styleLine); 

        std::vector<std::string> fields;
        boost::split(fields, styleLine, boost::is_any_of(","));

        FetchField(FORMAT_NAME, style->name, header->styleFormatFieldPos, fields);
        FetchField(FORMAT_FONTNAME, style->fontName, header->styleFormatFieldPos, fields);
        FetchField(FORMAT_FONTSIZE, style->fontSize, header->styleFormatFieldPos, fields);
        FetchField(FORMAT_PRIMARYCOLOR, style->primaryColor, header->styleFormatFieldPos, fields);
        FetchField(FORMAT_SECONDARYCOLOR, style->secondaryColor, header->styleFormatFieldPos, fields);
        FetchField(FORMAT_OUTLINECOLOR, style->outlineColor, header->styleFormatFieldPos, fields);
        FetchField(FORMAT_BACKCOLOR, style->backColor, header->styleFormatFieldPos, fields);
        FetchField(FORMAT_BOLD, style->bold, header->styleFormatFieldPos, fields);
        FetchField(FORMAT_ITALIC, style->italic, header->styleFormatFieldPos, fields);
        FetchField(FORMAT_UNDERLINE, style->underline, header->styleFormatFieldPos, fields);
        FetchField(FORMAT_STRIKEOUT, style->strikeout, header->styleFormatFieldPos, fields);
        FetchField(FORMAT_SCALEX, style->scaleXPercent, header->styleFormatFieldPos, fields);
        FetchField(FORMAT_SCALEY, style->scaleYPercent, header->styleFormatFieldPos, fields);
        FetchField(FORMAT_SPACING, style->pixelSpacing, header->styleFormatFieldPos, fields);
        FetchField(FORMAT_ANGLE, style->degreAngle, header->styleFormatFieldPos, fields);
        FetchField(FORMAT_BORDERSTYLE, style->borderStyle, header->styleFormatFieldPos, fields);
        FetchField(FORMAT_OUTLINE, style->outline, header->styleFormatFieldPos, fields);
        FetchField(FORMAT_SHADOW, style->shadow, header->styleFormatFieldPos, fields);
        FetchField(FORMAT_ALIGNMENT, style->alignment, header->styleFormatFieldPos, fields);
        FetchField(FORMAT_MARGINL, style->marginL, header->styleFormatFieldPos, fields);
        FetchField(FORMAT_MARGINR, style->marginR, header->styleFormatFieldPos, fields);
        FetchField(FORMAT_MARGINV, style->marginV, header->styleFormatFieldPos, fields);
        FetchField(FORMAT_ENCODING, style->encoding, header->styleFormatFieldPos, fields);

        // unsupported
        if(style->bold)
        {
            logger::Warn("Style bold unsupported");
        }
        if(style->italic)
        {
            logger::Warn("Style italic unsupported");
        }
        if(style->strikeout)
        {
            logger::Warn("Style strikeout unsupported");
        }
        if(style->scaleXPercent != 100)
        {
            logger::Warn("Style scaleXPercent unsupported");
        }
        if(style->scaleYPercent != 100)
        {
            logger::Warn("Style scaleYPercent unsupported");
        }
        if(style->pixelSpacing != 0)
        {
            logger::Warn("Style pixelSpacing unsupported");
        }
        if(style->degreAngle != 0)
        {
            logger::Warn("Style degreAngle unsupported");
        }
        if(style->borderStyle != subtitle::OUTLINE)
        {
            logger::Warn("Style borderStyle unsupported");
        }
        if(style->outline != 0)
        {
            logger::Warn("Style outline unsupported");
        }
        if(style->shadow != 0)
        {
            logger::Warn("Style shadow unsupported");
        }
        if(style->alphaLevel != 1.0f)
        {
            logger::Warn("Style alphaLevel unsupported");
        }
    }

    // .srt files https://en.wikipedia.org/wiki/SubRip
    Result ParseSubRipTime(const std::string& t, uint64_t& timeUs)
    {
        Result result;
        // format 00:00:00.000
        std::vector<std::string> fields;
        boost::split(fields, t, boost::is_any_of(":"));

        timeUs = 0;

        if(fields.size() == 3)
        {
            uint64_t hour = std::atoi(fields[0].c_str());
            uint64_t min = std::atoi(fields[1].c_str());

            std::vector<std::string> secondsFields;
            boost::split(secondsFields, fields[2], boost::is_any_of(","));

            uint64_t sec = std::atoi(secondsFields[0].c_str());
            uint64_t milli = 0;
            if(secondsFields.size() > 1)
            {
                milli = std::atoi(secondsFields[1].c_str());
            }

            timeUs = hour * 60 * 60 * 1000000;
            timeUs += min * 60 * 1000000;
            timeUs += sec * 1000000;
            timeUs += milli * 1000;

            if( chrono::Seconds(timeUs) > 20000) 
            {
            logger::Info("shit");
            }
        }
        else
        {
            return Result(false, "ParseSubRipTime invalid format %s", t.c_str());
        }


        return result;
    }

    Result ParseSubRipTime(const std::string& line, uint64_t& startTimeUs, uint64_t& endTimeUs)
    {
        Result result;

        std::vector<std::string> times;
        auto regex =  boost::regex(R"(-->)", boost::regex_constants::perl | boost::regex::no_mod_s | boost::regex::no_mod_m | boost::regex_constants::mod_x);
        boost::algorithm::split_regex(times, line, regex);
        if(times.size() != 2)
        {
            return Result(false, "ParseSubRipTime invalid format %s", line.c_str());
        }

        std::string startTimeStr = trim(times[0]);
        std::string endTimeStr = trim(times[1]);

        result = ParseSubRipTime(startTimeStr, startTimeUs);
        if(!result)
        {
            return result;
        }

        result = ParseSubRipTime(endTimeStr, endTimeUs);
        if(!result)
        {
            return result;
        }

        return result;
    }

}

namespace subtitle
{
    struct SubStationAlphaHeaderSection
    {
        std::string name;
        std::vector<std::string> items;
    };

    Result Parse(const std::string& ssa, SubStationAlphaHeader*& header)
    {
        Result result;

        logger::Debug("SSA Header\n%s", ssa.c_str());

        std::vector<SubStationAlphaHeaderSection*> sections;
        SubStationAlphaHeaderSection* section = nullptr;
        std::istringstream input(ssa);
        std::string line;

        header = new SubStationAlphaHeader();
        
        while( getline(input, line) )
        {
            line = trim(line);

            if(!line.empty())
            {
                // check for section
                bool isSectionBegin = (*line.begin() == '[' && *line.rbegin() == ']');
                if(isSectionBegin)
                {
                    if(section)
                    {
                        sections.push_back(section);
                    }

                    section = new SubStationAlphaHeaderSection();
                    section->name = line;
                }
                else if(section)
                {
                    section->items.push_back(line);
                }
            }
        }

        // append last section
        if(section)
        {
            sections.push_back(section);
        }

        // parse sections

        for(auto it = sections.begin(); it != sections.end(); ++it)
        {
            auto section = *it;
 
            if(compare_nocase(section->name,SECTION_SCRIPT_INFO))
            {
                for(auto itemsIt = section->items.begin(); itemsIt != section->items.end(); ++itemsIt)
                {
                    const std::string& item = *itemsIt;

                    if(starts_with(item, PLAYRESX))
                    {
                        std::string itemFields = item.substr(strlen(PLAYRESX));
                        header->playResX = std::atoi(itemFields.c_str());
                    }  
                    else if(starts_with(item, PLAYRESY))
                    {
                        std::string itemFields = item.substr(strlen(PLAYRESX));
                        header->playResY = std::atoi(itemFields.c_str());
                    }  
                }
            }
            else if(compare_nocase(section->name,SECTION_V4PSTYLES) || compare_nocase(section->name,SECTION_V4STYLES))
            {
                for(auto itemsIt = section->items.begin(); itemsIt != section->items.end(); ++itemsIt)
                {
                    const std::string& item = *itemsIt;

                    // parse format
                    if(starts_with(item, FORMAT_HEADER))
                    {
                        std::string itemFields = item.substr(strlen(FORMAT_HEADER));

                        std::vector<std::string> fields;
                        boost::split(fields, itemFields, boost::is_any_of(","));
                        
                        uint32_t i = 0;

                        for(auto fieldIt = fields.begin(); fieldIt != fields.end(); ++fieldIt)
                        {
                            std::string field = trim(*fieldIt);
                            header->styleFormatFieldPos[field] = i++;
                        }
                    }

                    if(starts_with(item, STYLE_HEADER))
                    {
                        std::string itemFields = item.substr(strlen(STYLE_HEADER));
                        SubStationAlphaStyle style;
                        ParseStyle(header, &style, itemFields);
                        header->styles[style.name] = style;
                    }

                }
                
            }
            else if(section->name == SECTION_EVENTS)
            {
                for(auto itemsIt = section->items.begin(); itemsIt != section->items.end(); ++itemsIt)
                {
                    const std::string& item = *itemsIt;

                    // parse format
                    if(starts_with(item, FORMAT_HEADER))
                    {
                        std::string itemFields = item.substr(strlen(FORMAT_HEADER));

                        std::vector<std::string> fields;
                        boost::split(fields, itemFields, boost::is_any_of(","));
                        
                        uint32_t i = 0;

                        for(auto fieldIt = fields.begin(); fieldIt != fields.end(); ++fieldIt)
                        {
                            std::string field = trim(*fieldIt);
                            header->eventFormatFieldPos[field] = i++;
                        }
                    }
                }
            }

            delete section;
        }

        if(header->eventFormatFieldPos.empty())
        {
            delete header;
            header = nullptr;
            result = Result(false, "No Event format found in ssa/ass header.");
        }
        return result;
    }

    Result Parse(const std::string& ssa, SubStationAlphaHeader* header, SubStationAlphaDialogue*& dialogue)
    {
        Result result;
        logger::Debug("Parse SSA Dialogue: %s", ssa.c_str());

        if(starts_with(ssa, DIALOGUE))
        {
            std::string itemFields = ssa.substr(strlen(DIALOGUE));
            const size_t numFields = header->eventFormatFieldPos.size();

            std::vector<std::string> fields;
            split(fields, itemFields, ',', numFields);

            if(fields.size() == numFields)
            {
                dialogue = new SubStationAlphaDialogue();

                FetchField(FORMAT_LAYER, dialogue->layer, header->eventFormatFieldPos, fields);
                FetchField(FORMAT_TEXT, dialogue->text, header->eventFormatFieldPos, fields);
                FetchFieldTime(FORMAT_START, dialogue->startTimeUs, header->eventFormatFieldPos, fields);
                FetchFieldTime(FORMAT_END, dialogue->endTimeUs, header->eventFormatFieldPos, fields);
                FetchField(FORMAT_EFFECT, dialogue->effect, header->eventFormatFieldPos, fields);
                FetchField(FORMAT_MARGINL, dialogue->marginL, header->eventFormatFieldPos, fields);
                FetchField(FORMAT_MARGINR, dialogue->marginR, header->eventFormatFieldPos, fields);
                FetchField(FORMAT_MARGINV, dialogue->marginV, header->eventFormatFieldPos, fields);
                FetchField(FORMAT_STYLE, dialogue->style, header->eventFormatFieldPos, fields);
            }
            else
            {
                result = Result(false, "Dialogue fields does not match format.");
            }
        }
        else
        {
            result = Result(false, "Dialogue not found in events.");
        }


        return result;
    }

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
                          SubLineList& sublines)
    {
        Result result;

        const float playScaleX = static_cast<float>(windowWidth) / static_cast<float>(header != nullptr ? header->playResX : windowWidth);
        const float playScaleY = static_cast<float>(windowHeight) / static_cast<float>(header != nullptr ? header->playResY : windowHeight);

        float tw = 0.0f;
        float th = 0.0f;
        float x = 0.0f;
        float y = 0.0f;

        if(dialogue && dialogue->startTimeUs != 0)
        {
            startTimeUs = dialogue->startTimeUs;
        }

        if(dialogue && dialogue->endTimeUs != 0)
        {
            endTimeUs = dialogue->endTimeUs;
        }

        static SubStationAlphaStyle defaultStyle;
        SubStationAlphaStyle& style = defaultStyle; 

        if(header != nullptr)
        {
            auto styleIt = header->styles.find(dialogue->style);
            if(styleIt == header->styles.end())
            {
                return Result(false, "Style not found %s", dialogue->style.c_str());
            }
            else
            {
                style = styleIt->second;
            }
        }

        
        float marginL = static_cast<float>(style.marginL);
        float marginR = static_cast<float>(style.marginR);
        float marginV = static_cast<float>(style.marginV);

        if(dialogue && dialogue->marginL != 0)
        {
            marginL = static_cast<float>(dialogue->marginL);
        }
        if(dialogue && dialogue->marginR != 0)
        {
            marginR = static_cast<float>(dialogue->marginR);
        }
        if(dialogue && dialogue->marginV != 0)
        {
            marginV = static_cast<float>(dialogue->marginV);
        }

        marginL *= playScaleX;
        marginR *= playScaleX;
        marginV *= playScaleY;

        fontName = style.fontName;
        fontSize = style.fontSize;
        color = style.primaryColor;
        fontSize = static_cast<uint32_t>(ceil(static_cast<float>(fontSize) * playScaleY));

        std::vector<std::string> allLines;

        for(auto it = lines.begin(); it != lines.end(); ++it)
        {
            std::vector<std::string> splitlines;
            auto regex =  boost::regex(R"(\\N)", boost::regex_constants::perl | boost::regex::no_mod_s | boost::regex::no_mod_m | boost::regex_constants::mod_x);
            boost::algorithm::split_regex(splitlines, *it, regex);
            allLines.insert(allLines.end(), splitlines.begin(), splitlines.end());
        }
 
        sublines.clear();
        sublines.reserve(allLines.size());
       
        for(auto it = allLines.begin(); it != allLines.end(); ++it)
        {
            Line subline;
            subline.text = *it;

            result = textSizeCb(subline.text, fontName, fontSize, subline.w, subline.h);
            if (!result)
            {
                logger::Error("Could not get text size %s", result.getError().c_str());
            }

            th += subline.h;
            tw = std::max(tw, subline.w);

            sublines.push_back(subline);
        }

        switch(style.alignment)
        {
            case LEFT:
                x = marginL;
                y = marginV;
            break;
            case CENTERED:
                x = windowWidth / 2.0f - tw / 2.0f;
                y = marginV;
            break;
            case RIGHT:
                x = tw - marginR;
                y = marginV;
            break;
            case LEFT_TOPTITLE:
                x = marginL;
                y = windowHeight - th - marginV;
            break;
            case CENTERED_TOPTITLE:
                x = windowWidth / 2.0f - tw / 2.0f;
                y = windowHeight - th - marginV;
            break;
            case RIGHT_TOPTITLE:
                x = tw - marginR;
                y = windowHeight - th - marginV;
            break;
            case LEFT_MIDDLETITLE:
                x = marginL;
                y = windowHeight / 2.0f - th / 2.0f;
            break;
            case RIGHT_MIDDLETITLE:
                x = tw - marginR;
                y = windowHeight / 2.0f - th / 2.0f;
            break;
            case CENTERED_MIDDLETITLE:
                x = windowWidth / 2.0f - tw / 2.0f;
                y = windowHeight / 2.0f - th / 2.0f;
            break;
        }

        for(size_t i = 0; i < sublines.size(); i++)
        {
            Line& subline = sublines[i];
            subline.x = x;
            subline.y = y;
            for(size_t j = i + 1; j < sublines.size(); j++)
            {
                Line& sublineBelow = sublines[j];
                subline.y += sublineBelow.h;
            }
        }

        return result;
    }

    Result Parse(const std::string& path, SubRip*& subRip)
    {
        Result result;
        std::string utf8;
         
        result = filesystem::ReadTextFileToUTF8(path, utf8);
        if(!result)
        {
            return result;
        }

        std::istringstream file(utf8);
        std::string line;
        subRip = new SubRip();

        // Example format

        // 168
        // 00:20:41,150 --> 00:20:45,109
        // - How did he do that?
        // - Made him an offer he couldn't refuse.
        while( getline(file, line) )
        {
            line = trim(line);
            if(line.empty())
            {
                continue;
            }

            SubRipDialogue diag;
            diag.counter = std::atoi(line.c_str());

            if(getline(file,line))
            {
                line = trim(line);
                result = ParseSubRipTime(line, diag.startTimeUs, diag.endTimeUs);
                if(!result)
                {
                    return result;
                }
            }
            else
            {
                return Result(false, "Invalid format. Expecting a time range.");
            }

            while( getline(file, line) )
            {
                line = trim(line);
                if(line.empty())
                {
                    break;
                }
                auto regex =  boost::regex(R"(<b>|</b>|<i>|</i>|<u>|</u>)", boost::regex_constants::perl | 
                                                                            boost::regex::no_mod_s | boost::regex::no_mod_m | 
                                                                            boost::regex_constants::mod_x);
                line = boost::regex_replace(line, regex, "");

                Line diagLine;
                diagLine.text = line;
                diag.lines.push_back(diagLine);
            }
            subRip->diags.push_back(diag);
        }

        return result;
    }
}
