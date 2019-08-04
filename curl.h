
#pragma once


#include <boost/function.hpp>
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
        CURL* curl;

        std::mutex mutex;
        std::deque<uint8_t> buffer;
        uint64_t offset;
        uint64_t resumeOffset;

        std::atomic<bool> cancel;
        std::atomic<bool> done;

        CURLcode result;
        
        std::string url;
        std::thread thread;
    };

    // create a download session
    Result Create(Session*& session, const std::string& url, uint64_t offset);
    size_t Read(Session*, uint8_t* buf, size_t size);
    size_t Seek(Session*, uint64_t offset);
    void   Destroy(Session*);


}
