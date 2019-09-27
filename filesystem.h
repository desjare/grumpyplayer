
#pragma once

#include <boost/filesystem.hpp>
#include <boost/optional.hpp>
#include <boost/locale.hpp>

#include <algorithm>
#include <iostream>
#include <fstream>

#include "stringext.h"
#include "result.h"

namespace filesystem
{
    inline boost::optional<boost::filesystem::path> FindFile(const boost::filesystem::path& dir, const boost::filesystem::path& file) 
    {
        const boost::filesystem::recursive_directory_iterator end;
        const auto it = std::find_if( boost::filesystem::recursive_directory_iterator(dir), end, [&file](const boost::filesystem::directory_entry& e) {
                                 return e.path().filename() == file;
                              });
        return it == end ? boost::optional<boost::filesystem::path>() : it->path();
    }

    inline boost::optional<boost::filesystem::path> FindFileNoCase(const boost::filesystem::path& dir, const boost::filesystem::path& file) 
    {
        const boost::filesystem::recursive_directory_iterator end;
        const auto it = std::find_if( boost::filesystem::recursive_directory_iterator(dir), end, [&file](const boost::filesystem::directory_entry& e) {
                                 return compare_nocase(e.path().filename().string(),file.string());
                              });
        return it == end ? boost::optional<boost::filesystem::path>() : it->path();
    }

    inline Result ReadTextFileToUTF8(const std::string& path, std::string& utf8)
    {
        Result result;
        std::ifstream is(path, std::ifstream::in | std::ifstream::binary);
        if(is.fail() || is.bad())
        {
            return Result(false, "Could not open %s", path.c_str());
        }

        is.seekg (0, std::ios::end);
        size_t length = is.tellg();
        is.seekg (0, std::ios::beg);
        char* buffer = new char[length];
        is.read (buffer,length);
        is.close();

        if(length > 2)
        {
            // UCS-2 BOM
            if(buffer[0] == '\377' && buffer[1] == '\376')
            {
                utf8 = boost::locale::conv::to_utf<char>(buffer, buffer + length, "UCS-2");
            }
        }

        // assume utf8
        if(utf8.empty())
        {
            utf8.insert(utf8.begin(), buffer, buffer + length);
        }

        delete [] buffer;

        return result;

    }
}
