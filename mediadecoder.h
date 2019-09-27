
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
    static const uint32_t MAX_FRAME_RATE = 120;

    // forward declaration
    struct Stream;
    struct Producer;

    // types
    typedef boost::function<void (Stream*, Producer*, AVFrame*, AVPacket*)> DecoderCallback;

    struct Stream
    {
        AVCodecParameters* codecParameters = nullptr;
        AVCodec* codec = nullptr;
        AVCodecContext* codecContext = nullptr;
        AVStream* stream = nullptr;
        int32_t streamIndex = -1;

        // processing
        DecoderCallback processCallback;
    };

    struct VideoStream : public Stream
    {
        // video stream output format
        VideoFormat outputFormat = VF_INVALID;

        // sws_scale context if required
        SwsContext* swsContext = nullptr;
        
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

        AudioChannelList channelMapping;
        std::map<uint32_t, uint32_t> channelInputToOutputMapping;
    };

    struct SubtitleStream : public Stream
    {
        subtitle::SubStationAlphaHeader* subtitleHeader = nullptr;
    };

    struct SubtitleSubRip
    {
        std::shared_ptr<subtitle::SubRip> subs;
        subtitle::SubRipDialogueList::const_iterator posIt;
    };

    struct VideoFrame
    {
        uint8_t* buffers[NUM_FRAME_DATA_POINTERS] = { nullptr, nullptr, nullptr, nullptr};
        int32_t lineSize[NUM_FRAME_DATA_POINTERS] = { 0, 0, 0, 0 };
        uint32_t width = 0;
        uint32_t height = 0;
        uint64_t timeUs = 0;
    };

    struct AudioFrame
    {
        uint8_t* samples = nullptr;
        uint32_t sampleSize = 0;
        uint32_t nbSamples = 0;
        uint32_t channels = 0;
        uint64_t timeUs = 0;
        std::atomic<bool> inUse = false;
    };

    struct Subtitle
    {
        // ass subtitle
        subtitle::SubStationAlphaHeader* header = nullptr;
        subtitle::SubStationAlphaDialogue* dialogue = nullptr;
        
        // text subtitle
        std::vector<std::string> text;
        uint64_t startTimeUs = 0;
        uint64_t endTimeUs = 0;

        std::string fontName = "Arial";
        uint32_t fontSize = 16;
        glm::vec3 color = {1.0f, 1.0f, 1.0f};

        float x = 0.0f;
        float y = 0.0f;
    };

    template<typename T>
    using FrameQueue = boost::lockfree::queue<T, boost::lockfree::fixed_sized<true> >;
    typedef FrameQueue<AudioFrame*> AudioQueue;
    typedef FrameQueue<VideoFrame*> VideoQueue;
    typedef FrameQueue<Subtitle*>   SubtitleQueue;

    struct Decoder
    {
        AVFormatContext* avFormatContext = nullptr;

        VideoStream* videoStream = nullptr;
        AudioStream* audioStream = nullptr;
        
        std::vector<SubtitleStream*> subtitleStreams;

        std::vector<int32_t> subtitleIndexes = {-1};
        uint32_t subtitleIndex = 0;
        uint32_t nextSubtitleIndex = 0;

        std::map<uint32_t, SubtitleSubRip> subRips;
        
        Producer* producer = nullptr;
        curl::Session* curl = nullptr;
    };

    struct Producer
    {
        Decoder* decoder = nullptr;

        // playback frames
        VideoQueue* videoQueue = nullptr;
        AudioQueue* audioQueue = nullptr;

        std::atomic<uint32_t> videoQueueSize;
        std::atomic<uint32_t> audioQueueSize;

        uint32_t videoQueueCapacity = 0;
        uint32_t audioQueueCapacity = 0;

        // frame pools
        VideoQueue* videoFramePool = nullptr;
        AudioQueue* audioFramePool = nullptr;

        // subtitle
        SubtitleQueue* subtitleQueue = nullptr;

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

    // video format
    VideoFormat GetOutputFormat(Decoder*);
    uint32_t    GetVideoWidth(Decoder* decoder);
    uint32_t    GetVideoHeight(Decoder* decoder);
    uint32_t    GetFramesPerSecond(Decoder* decoder);

    // audio format
    uint32_t    GetAudioNumChannels(Decoder*);
    uint32_t    GetAudioSampleRate(Decoder*);
    SampleFormat GetAudioSampleFormat(Decoder*);

    uint64_t GetDuration(Decoder* decoder);

    bool GetHaveAudio(Decoder* decoder);
    bool GetHaveVideo(Decoder* decoder);

    // subtitle
    void AddSubtitleTrack(Decoder*, std::shared_ptr<subtitle::SubRip> track);
    void ToggleSubtitleTrack(Decoder*);

    void Destroy(Decoder*&);

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

