
#pragma once

#include "mediaformat.h"
#include "curl.h"
#include "result.h"
#include "subtitle.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
}


#ifdef WIN32
#pragma warning( push )
#pragma warning( disable : 26495) // uninitialized variable
#endif

#include <boost/lockfree/queue.hpp>
#include <boost/function.hpp>

#ifdef WIN32
#pragma warning( pop ) 
#endif

#include <string>
#include <vector>
#include <thread>

namespace mediadecoder
{
    static const uint32_t NUM_FRAME_DATA_POINTERS = 4;
    static const uint32_t DEFAULT_SUBTITLE_DURATION_SEC = 4;
    
    // forward declaration
    struct Stream;
    struct Producer;

    // types
    typedef boost::function<void (Stream*, Producer*, AVFrame*, AVPacket*)> DecoderCallback;

    struct Stream
    {
        AVCodecParameters* codecParameters = NULL;
        AVCodec* codec = NULL;
        AVCodecContext* codecContext = NULL;
        AVStream* stream = NULL;
        int32_t streamIndex = -1;

        // processing
        DecoderCallback processCallback;
    };

    struct VideoStream : public Stream
    {
        // video stream output format
        VideoFormat outputFormat = VF_INVALID;

        // sws_scale context if required
        SwsContext* swsContext = NULL;
        
        // destination format
        AVPixelFormat dstFormat = AV_PIX_FMT_NONE;

        // reformat buffer size if reformat is required
        uint32_t reformatBufferSize = 0;

        uint32_t width = 0;
        uint32_t height = 0;

        uint32_t framesPerSecond = 0;
    };

    struct AudioStream : public Stream
    {
        SampleFormat sampleFormat = SF_FMT_INVALID;
        uint32_t sampleRate = 0;
        uint32_t channels = 0;
        std::vector<AudioChannel> channelMapping;
    };

    struct SubtitleStream : public Stream
    {
        subtitle::SubStationAlphaHeader* subtitleHeader = NULL;
    };

    struct VideoFrame
    {
        uint8_t* buffers[NUM_FRAME_DATA_POINTERS] = { NULL, NULL, NULL, NULL};
        int32_t lineSize[NUM_FRAME_DATA_POINTERS] = { 0, 0, 0, 0 };
        uint32_t width = 0;
        uint32_t height = 0;
        uint64_t timeUs = 0;
    };

    struct AudioFrame
    {
        uint8_t* samples = NULL;
        uint32_t sampleSize = 0;
        uint32_t nbSamples = 0;
        uint32_t channels = 0;
        uint64_t timeUs = 0;
        std::atomic<bool> inUse = false;
    };

    struct Subtitle
    {
        std::string text;
        uint64_t startTimeUs = 0;
        uint64_t endTimeUs = 0;
    };

    template<typename T>
    using FrameQueue = boost::lockfree::queue<T, boost::lockfree::fixed_sized<true> >;
    typedef FrameQueue<AudioFrame*> AudioQueue;
    typedef FrameQueue<VideoFrame*> VideoQueue;
    typedef FrameQueue<Subtitle*>   SubtitleQueue;

    struct Decoder
    {
        AVFormatContext* avFormatContext;

        VideoStream* videoStream;
        AudioStream* audioStream;
        
        std::vector<SubtitleStream*> subtitleStreams;
        std::vector<int32_t> subtitleIndexes = {-1 };
        uint32_t subtitleIndex = 0;
        
        Producer* producer;
        curl::Session* curl;
    };

    struct Producer
    {
        Decoder* decoder = NULL;

        // playback frames
        VideoQueue* videoQueue = NULL;
        AudioQueue* audioQueue = NULL;

        std::atomic<uint32_t> videoQueueSize;
        std::atomic<uint32_t> audioQueueSize;

        uint32_t videoQueueCapacity = 0;
        uint32_t audioQueueCapacity = 0;

        // frame pools
        VideoQueue* videoFramePool = NULL;
        AudioQueue* audioFramePool = NULL;

        // subtitle
        SubtitleQueue* subtitleQueue = NULL;

        // media streams owned by the decoder
        std::vector<Stream*> streams;

        std::thread thread;
        std::atomic<bool> quitting = false;;

        uint64_t currentDecodingTimeUs = 0;

        // seeking
        std::atomic<bool> seeking;
        uint64_t seekTime = 0;

        // eof
        std::atomic<bool> done = false;;
    };

    Result   Init();
    void     SetOutputFormat(const VideoFormatList&);

    // decoder
    Result   Create(Decoder*& decoder);
    Result   Open(Decoder*& decoder, const std::string& filename);

    VideoFormat GetOutputFormat(Decoder*);
    uint64_t    GetDuration(Decoder* decoder);
    uint32_t    GetFramesPerSecond(Decoder* decoder);
    void        ToggleSubtitle(Decoder*);

    void     Destroy(Decoder*&);

    // producer / consumer
    Result Create(Producer*& producer, Decoder*);
    void   Destroy(Producer*&);

    void   Seek(Producer*,uint64_t timeUs);
    bool   IsSeeking(Producer*);
    bool   Consume(Producer*, VideoFrame*& frame);
    bool   Consume(Producer*, AudioFrame*& frame);
    bool   Consume(Producer*, Subtitle*& sub);

    void   Release(Producer*,VideoFrame*);
    void   Release(Producer*,AudioFrame*);
    void   Release(Producer*,Subtitle*);

    void   WaitForPlayback(Producer*);
};

