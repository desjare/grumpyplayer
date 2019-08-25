
#pragma once

#include <boost/filesystem.hpp>
#include <boost/optional.hpp>

#include <algorithm>

namespace filesystem
{
    boost::optional<boost::filesystem::path> FindFile(const boost::filesystem::path& dir, const boost::filesystem::path& file) 
    {
        const boost::filesystem::recursive_directory_iterator end;
        const auto it = std::find_if( boost::filesystem::recursive_directory_iterator(dir), end, [&file](const boost::filesystem::directory_entry& e) {
                                 return e.path().filename() == file;
                              });
        return it == end ? boost::optional<boost::filesystem::path>() : it->path();
    }
}
