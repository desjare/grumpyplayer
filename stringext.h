
#pragma once

#include <string>
#include <codecvt>
#include <iostream>
#include <algorithm>
#include <vector>
#include <locale>
#include <cctype>

inline std::string & ltrim(std::string & str)
{
  auto it2 =  std::find_if( str.begin() , str.end() , [](char ch){ return !std::isspace<char>(ch , std::locale::classic() ) ; } );
  str.erase( str.begin() , it2);
  return str;   
}

inline std::string & rtrim(std::string & str)
{
  auto it1 =  std::find_if( str.rbegin() , str.rend() , [](char ch){ return !std::isspace<char>(ch , std::locale::classic() ) ; } );
  str.erase( it1.base() , str.end() );
  return str;   
}

inline std::string & trimeol(std::string & str)
{
  auto it1 =  std::find_if( str.rbegin() , str.rend() , [](char ch){ return !(ch == '\n' || ch == '\r'); } );
  str.erase( it1.base() , str.end() );
  return str;   
}

inline std::string& trim(std::string & str)
{
   return ltrim(rtrim(str));
}

inline bool starts_with(const std::string& haystack, const std::string& needle) 
{
    return needle.length() <= haystack.length() 
        && std::equal(needle.begin(), needle.end(), haystack.begin());
}

inline std::string replace_all(std::string subject, const std::string& search, const std::string& replace) 
{
    size_t pos = 0;
    while ((pos = subject.find(search, pos)) != std::string::npos) {
         subject.replace(pos, search.length(), replace);
         pos += replace.length();
    }
    return subject;
}

inline bool compare_nocase(const std::string & str1, const std::string &str2)
{
	return ((str1.size() == str2.size()) && std::equal(str1.begin(), str1.end(), str2.begin(), [](const char & c1, const char & c2){
							return (c1 == c2 || std::toupper(c1) == std::toupper(c2));
								}));
}

inline std::string tolower(std::string s) 
{
    std::transform(s.begin(), s.end(), s.begin(), 
                   [](unsigned char c){ return std::tolower(c); } );
    return s;
}

inline std::string toupper(std::string s) 
{
    std::transform(s.begin(), s.end(), s.begin(), 
                   [](unsigned char c){ return std::toupper(c); } );
    return s;
}

inline void split(std::vector<std::string>&v, const std::string& s, char delim, size_t maxFields)
{
    const size_t size = s.size();
    const char* str = s.c_str();
    uint32_t start = 0;

    v.clear();
    v.reserve(maxFields);

    for(uint32_t i = 0; i < size; i++)
    {
        if(str[i] == delim)
        {
            std::string item = s.substr(start, i - start);
            start = i + 1;
            v.push_back(item);

            if(v.size() == maxFields-1)
            {
                break;
            }
        }
    }

    if(start < size && v.size() == maxFields-1)
    {
        std::string item = s.substr(start);
        v.push_back(item);
    }
}

inline std::wstring utf8towstring(const std::string& utf8)
{
    std::wstring_convert<std::codecvt_utf8<wchar_t>> utf8conv;
    return utf8conv.from_bytes(utf8);
}


inline std::istream& getline(std::istream& is, std::string& t)
{
    t.clear();

    // The characters in the stream are read one-by-one using a std::streambuf.
    // That is faster than reading them one-by-one using the std::istream.
    // Code that uses streambuf this way must be guarded by a sentry object.
    // The sentry object performs various tasks,
    // such as thread synchronization and updating the stream state.

    std::istream::sentry se(is, true);
    std::streambuf* sb = is.rdbuf();

    for(;;) {
        int c = sb->sbumpc();
        switch (c) {
        case '\n':
            return is;
        case '\r':
            if(sb->sgetc() == '\n')
                sb->sbumpc();
            return is;
        case std::streambuf::traits_type::eof():
            // Also handle the case when the last line has no line ending
            if(t.empty())
                is.setstate(std::ios::eofbit);
            return is;
        default:
            t += (char)c;
        }
    }
}

