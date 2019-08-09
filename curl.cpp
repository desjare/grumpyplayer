#include "precomp.h"
#include "curl.h"
#include "logger.h"

#ifdef WIN32
#undef min
#endif

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

    int ProgressCallback(void *clientp, curl_off_t dlnow, curl_off_t dltotal, curl_off_t ultotal, curl_off_t ulnow)
    {
        curl::Session* session = static_cast<curl::Session*>(clientp);
        session->totalBytes = static_cast<uint64_t>(dltotal);
        if(session->cancel)
        {
            return 1;
        }
        return 0;
    }

    void Cancel(curl::Session* session)
    {
        session->cancel = true;
        session->thread.join();

        session->buffer.clear();
        session->totalBytes = 0;
        session->offset = 0;

        curl_easy_cleanup(session->curl);
        session->curl = NULL;
        session->cancel = false;
        session->done = false;
        session->result = CURLE_OK;

    }

    void StartSession(curl::Session* session, uint64_t offset)
    {
        session->curl = curl_easy_init();
        curl_easy_setopt(session->curl, CURLOPT_URL, session->url.c_str());
        curl_easy_setopt(session->curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(session->curl, CURLOPT_WRITEDATA, session);
        curl_easy_setopt(session->curl, CURLOPT_NOPROGRESS, 0);
        curl_easy_setopt(session->curl, CURLOPT_PROGRESSFUNCTION, ProgressCallback);
        curl_easy_setopt(session->curl, CURLOPT_PROGRESSDATA, session);
        curl_easy_setopt(session->curl, CURLOPT_RESUME_FROM_LARGE, offset); 

        logger::Info("Curl Session Started %s offset %ld", session->url.c_str(), offset);

        session->totalBytes = 0;
        session->cancel = false;
        session->done = false;
        session->result = CURLE_OK;
        session->offset = 0;
        session->thread = std::thread(DownloadThread, session);
    }
 
}

namespace curl
{
    Result Create(Session*& session, const std::string& url, uint64_t offset)
    {
        Result result;
        session = new Session;
        session->url = url;
        
        StartSession(session, offset);

        return result;
    }

    size_t Read(Session* session, uint8_t* readbuf, size_t size)
    {
        std::lock_guard<std::mutex> guard(session->mutex);

        std::deque<uint8_t>& buffer = session->buffer;
        size = std::min(size, buffer.size());
        std::copy_n(buffer.begin(), size, readbuf);
        buffer.erase(buffer.begin(), buffer.begin()+size);
        session->offset += static_cast<uint64_t>(size);

        return size;
    }

    size_t Seek(Session* session, uint64_t offset)
    {
        std::deque<uint8_t>& buffer = session->buffer;
        size_t position = 0;

        // Can we continue the download session
        if(offset >= session->offset &&  offset < session->offset + buffer.size()  )
        {
            std::lock_guard<std::mutex> guard(session->mutex);
            logger::Info("Curl seek. Buffer already in memory. Size %d", buffer.size());
            size_t offsetBytes = offset - session->offset;
            buffer.erase(buffer.begin(), buffer.begin()+offsetBytes);
            session->offset += offset;
            position = session->offset;
        }
        else
        {
            Cancel(session);
            StartSession(session,offset);
        }

        return position;
    }

    void Destroy(Session* session)
    {
        if(!session)
        {
            return;
        }

        Cancel(session);
        delete session;
    }

}
