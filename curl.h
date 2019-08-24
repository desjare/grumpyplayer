
#pragma once

#ifdef WIN32
#pragma warning( push )
#pragma warning( disable : 26495) // uninitialized variable
#endif
#include <boost/function.hpp>
#ifdef WIN32
#pragma warning( pop ) 
#endif


#include <curl/curl.h>

#include <deque>
#include <string>
#include <thread>
#include <mutex>
#include <atomic>

#include "result.h"

namespace curl
{
    struct Session
    {
        CURL* curl = NULL;

        std::mutex mutex;
        std::deque<uint8_t> buffer;
        std::atomic<uint64_t> offset;

        std::atomic<uint64_t> totalBytes;

        std::atomic<bool> cancel;
        std::atomic<bool> done;

        CURLcode result = CURLE_OK;
        
        std::string url;
        std::thread thread;
    };

    // create a download session
    Result Create(Session*& session, const std::string& url, uint64_t offset);
    size_t Read(Session*, uint8_t* buf, size_t size);
    size_t Seek(Session*, uint64_t offset);
    void   Destroy(Session*);


}
