
#pragma once

#include "mediaformat.h"
#include "result.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavdevice/avdevice.h>
#include <libswresample/swresample.h>
#include <libswscale/swscale.h>
}

#include <boost/lockfree/queue.hpp>
#include <boost/function.hpp>

#include <string>
#include <vector>
#include <thread>

namespace mediadecoder
{
    // forward declaration
    struct Stream;
    struct Producer;

    // types
    typedef boost::function<void (Stream*, Producer*, AVFrame*)> DecoderCallback;

    struct Stream
    {
        AVCodecParameters* codecParameters;
        AVCodec* codec;
        AVCodecContext* codecContext;
        AVStream* stream;
        int32_t streamIndex;

        // processing
        DecoderCallback processCallback;
    };

    struct VideoStream : public Stream
    {
        SwsContext* swsContext;
        // RGB 24 bytes frame
        AVFrame* frame;
        // RGB 24 bytes buffer size
        uint32_t bufferSize;

        uint32_t width;
        uint32_t height;
    };

    struct AudioStream : public Stream
    {
        SampleFormat sampleFormat;
        uint32_t sampleRate;
        uint32_t channels;
    };

    struct VideoFrame
    {
        uint8_t* frame;
        uint32_t bufferSize;
        uint64_t timeUs;
    };

    struct AudioFrame
    {
        uint8_t* samples;
        uint32_t sampleSize;
        uint32_t nbSamples;
        uint32_t channels;
        uint64_t timeUs;
    };

    typedef boost::lockfree::queue<AudioFrame*, boost::lockfree::fixed_sized<true> > AudioQueue;
    typedef boost::lockfree::queue<VideoFrame*, boost::lockfree::fixed_sized<true> > VideoQueue;

    struct Decoder
    {
        AVFormatContext* avFormatContext;
        VideoStream* videoStream;
        AudioStream* audioStream;
        Producer* producer;
    };

    struct Producer
    {
        Decoder* decoder;

        // playback frames
        VideoQueue* videoQueue;
        AudioQueue* audioQueue;

        std::atomic<uint32_t> videoQueueSize;
        std::atomic<uint32_t> audioQueueSize;

        uint32_t videoQueueCapacity;
        uint32_t audioQueueCapacity;

        // frame pools
        VideoQueue* videoFramePool;
        AudioQueue* audioFramePool;

        // media streams owned by the decoder
        std::vector<Stream*> streams;

        std::thread thread;
        std::atomic<bool> quitting;
    };

    Result Init();
    Result Create(Decoder*& decoder);
    Result Open(Decoder*& decoder, const std::string& filename);
    void   Destroy(Decoder*);

    Result Create(Producer*& producer, Decoder*);
    void   Destroy(Producer*);

    bool   Consume(Producer*, VideoFrame*& frame);
    bool   Consume(Producer*, AudioFrame*& frame);

    void   Release(Producer*,VideoFrame*);
    void   Release(Producer*,AudioFrame*);

    void   WaitForPlayback(Producer*);
};

