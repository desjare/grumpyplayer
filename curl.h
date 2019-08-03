
#pragma once


#include <boost/function.hpp>
#include <curl/curl.h>

#include <string>
#include <thread>

#include "result.h"

namespace curl
{
    struct Session;

    typedef boost::function<size_t (Session*, char* ptr, size_t size)> WriteCallback;

    struct Session
    {
        CURL* curl;

        WriteCallback writeCallback;

        std::atomic<bool> done;
        
        std::string url;
        std::thread thread;
    };

    // create a download session
    Result Create(Session*& session, const std::string& url, WriteCallback& writeCallback);
    void   Destroy(Session*);


}
