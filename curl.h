
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

        std::atomic<bool> done;
        CURLcode result;
        
        std::string url;
        std::thread thread;
    };

    // create a download session
    Result Create(Session*& session, const std::string& url);
    size_t Read(Session*, uint8_t* buf, size_t size);
    void   Destroy(Session*);


}
