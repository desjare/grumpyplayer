
#include "curl.h"
#include "logger.h"

namespace {

    void DownloadThread(curl::Session* session)
    {
        CURLcode res = curl_easy_perform(session->curl);
        session->done = true;
        session->result = res;

        logger::Info("Curl Session Ended %s", curl_easy_strerror(res));
    }

    size_t WriteCallback(char *ptr, size_t, size_t nmemb, void *userdata)
    {
        curl::Session* session = static_cast<curl::Session*>(userdata);
        uint8_t* data = reinterpret_cast<uint8_t*>(ptr);

        std::lock_guard<std::mutex> guard(session->mutex);
        std::deque<uint8_t>& buffer = session->buffer;
        buffer.insert(buffer.end(), data, data + nmemb); 
        return nmemb;
    }
 
}

namespace curl
{
    Result Create(Session*& session, const std::string& url)
    {
        Result result;
        session = new Session;
        session->url = url;

        session->curl = curl_easy_init();
        curl_easy_setopt(session->curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(session->curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(session->curl, CURLOPT_WRITEDATA, session);
        
        logger::Info("Curl Session Started %s", url.c_str());

        session->done = false;
        session->result = CURLE_OK;
        session->thread = std::thread(DownloadThread, session);

        return result;
    }

    size_t Read(Session* session, uint8_t* readbuf, size_t size)
    {
        std::lock_guard<std::mutex> guard(session->mutex);

        std::deque<uint8_t>& buffer = session->buffer;
        size = std::min(size, buffer.size());
        std::copy_n(buffer.begin(), size, readbuf);
        buffer.erase(buffer.begin(), buffer.begin()+size);

        return size;
    }

    void Destroy(Session* session)
    {
        curl_easy_cleanup(session->curl);
        delete session;
    }

}
