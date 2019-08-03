
#include "curl.h"

namespace {

    void DownloadThread(curl::Session* session)
    {
        CURLcode res = curl_easy_perform(session->curl);
        session->done = true;
    }

    size_t WriteCallback(char *ptr, size_t size, size_t, void *userdata)
    {
        curl::Session* session = static_cast<curl::Session*>(userdata);
        session->writeCallback(session,ptr,size);
    }
 
}

namespace curl
{
    Result Create(Session*& session, const std::string& url, WriteCallback& writeCallback)
    {
        session = new Session;
        session->writeCallback = writeCallback;
        session->url = url;

        session->curl = curl_easy_init();
        curl_easy_setopt(session->curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(session->curl, CURLOPT_WRITEFUNCTION, ::WriteCallback);
        curl_easy_setopt(session->curl, CURLOPT_WRITEDATA, session);

        session->thread = std::thread(session);
    }

    void Destroy(Session* session)
    {
        curl_easy_cleanup(session->curl);
        delete session;
    }

}
