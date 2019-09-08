
#include "precomp.h"
#include "subtitle.h"
#include "stringext.h"
#include "chrono.h"
#include "logger.h"

#include <boost/algorithm/string.hpp>
#include <vector>
#include <sstream>

namespace
{
    // Script Info
    const char* SECTION_SCRIPT_INFO = "[Script Info]";

    const char* PLAYRESX = "PlayResX:";
    const char* PLAYRESY = "PlayResX:";

    // [V4+ Styles]
    const char* SECTION_V4PSTYLES = "[V4+ Styles]";

    const char* STYLE_HEADER = "Style:";

    const char* FORMAT_FONTNAME = "Fontname";
    const char* FORMAT_FONTSIZE = "Fontsize";
    const char* FORMAT_PRIMARYCOLOR = "PrimaryColour";
    const char* FORMAT_SECONDARYCOLOR = "SecondaryColour";
    const char* FORMAT_OUTLINECOLOR = "OutlineColour";

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
    const char* FORMAT_MARGNIV = "MarginV";
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

    glm::vec3 StyleColorToColor(const std::string& c)
    {
        float r = 0.0f;
        float g = 0.0f;
        float b = 0.0f;

        std::string s = c;

        // &Hffffff
        if(s.size() >= 2)
        {
            // erase &H
            s.erase(0, 2);

            // red
            std::string rstr = s.substr(0,2);
            r = strtol(rstr.c_str(), nullptr, 16) / 255.0f;

            // erase r
            s.erase(0, 2); 

            if(s.size() >= 2)
            {
                // g
                std::string gstr = s.substr(0,2);
                g = strtol(gstr.c_str(), nullptr, 16) / 255.0;

                // erase g
                s.erase(0, 2); 

                if(s.size() >= 2)
                {
                    // b
                    std::string bstr = s.substr(0,2);
                    b = strtol(bstr.c_str(), nullptr, 16) / 255.0;
                }
            }
        }

        return glm::vec3(r,g,b);
    }

    void FetchField(const char* fieldName, std::string& field, std::map<std::string, uint32_t>& pos, std::vector<std::string>& fields)
    {
        auto it = pos.find(fieldName);
        if(it != pos.end())
        {
            field = trimeol(fields[it->second]);
        }
    }

    void FetchField(const char* fieldName, uint32_t& field, std::map<std::string, uint32_t>& pos, std::vector<std::string>& fields)
    {
        auto it = pos.find(fieldName);
        if(it != pos.end())
        {
            field = std::atoi(fields[it->second].c_str());
        }
    }

    void FetchField(const char* fieldName, glm::vec3& field, std::map<std::string, uint32_t>& pos, std::vector<std::string>& fields)
    {
        auto it = pos.find(fieldName);
        if(it != pos.end())
        {
            field = StyleColorToColor(fields[it->second].c_str());
        }
    }

    void FetchFieldTime(const char* fieldName, uint64_t& field, std::map<std::string, uint32_t>& pos, std::vector<std::string>& fields)
    {
        auto it = pos.find(fieldName);
        if(it != pos.end())
        {
            field = chrono::Microseconds(EventTimeToSeconds(fields[it->second]));
        }
    }

    void ParseStyle(subtitle::SubStationAlphaHeader* header, const std::string& line)
    {
        std::string style = line;
        trim(style); 

        std::vector<std::string> fields;
        boost::split(fields, style, boost::is_any_of(","));

        FetchField(FORMAT_NAME, header->name, header->styleFormatFieldPos, fields);
        FetchField(FORMAT_FONTNAME, header->fontName, header->styleFormatFieldPos, fields);
        FetchField(FORMAT_FONTSIZE, header->fontSize, header->styleFormatFieldPos, fields);
        FetchField(FORMAT_PRIMARYCOLOR, header->primaryColor, header->styleFormatFieldPos, fields);
        FetchField(FORMAT_SECONDARYCOLOR, header->secondaryColor, header->styleFormatFieldPos, fields);
        FetchField(FORMAT_OUTLINECOLOR, header->outlineColor, header->styleFormatFieldPos, fields);
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

        logger::Info("SSA Header\n%s", ssa.c_str());

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
 
            if(section->name == SECTION_SCRIPT_INFO)
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
            else if(section->name == SECTION_V4PSTYLES)
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
                        ParseStyle(header, itemFields);
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
                FetchField(FORMAT_MARGNIV, dialogue->marginV, header->eventFormatFieldPos, fields);
                FetchField(FORMAT_STYLE, dialogue->style, header->eventFormatFieldPos, fields);

                // do not support line break
                dialogue->text = replace_all(dialogue->text, "\\N", " ");
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

}
