
#include "precomp.h"
#include "subtitle.h"
#include "stringext.h"

#include <boost/algorithm/string.hpp>
#include <vector>
#include <sstream>

namespace
{
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

        std::vector<SubStationAlphaHeaderSection*> sections;
        SubStationAlphaHeaderSection* section = NULL;
        std::istringstream input(ssa);
        std::string line;

        header = new SubStationAlphaHeader();

        printf("%s\n", ssa.c_str());
        
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
            if(section->name == SECTION_EVENTS)
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
            header = NULL;
            result = Result(false, "No Event format found in ssa/ass header.");
        }
        return result;
    }

    Result Parse(const std::string& ssa, SubStationAlphaHeader* header, SubStationAlphaDialogue*& dialogue)
    {
        Result result;
        if(starts_with(ssa, DIALOGUE))
        {
            std::string itemFields = ssa.substr(strlen(DIALOGUE));
            const size_t numFields = header->eventFormatFieldPos.size();

            std::vector<std::string> fields;
            split(fields, itemFields, ',', numFields);


            if(fields.size() == numFields)
            {
                uint32_t textPos = header->eventFormatFieldPos[FORMAT_TEXT];
                uint32_t startPos = header->eventFormatFieldPos[FORMAT_START];
                uint32_t endPos = header->eventFormatFieldPos[FORMAT_END];

                std::string text = trimeol(fields[textPos]);
                std::string start = fields[startPos];
                std::string end = fields[endPos];

                dialogue = new SubStationAlphaDialogue();
                dialogue->text = text;

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
