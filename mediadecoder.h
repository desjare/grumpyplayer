
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
    namespace producer 
    {
        struct Producer;
    }

    // types
    typedef boost::function<void (Stream*, producer::Producer*, AVFrame*)> DecoderCallback;

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

    struct Decoder
    {
        AVFormatContext* avFormatContext;
        VideoStream* videoStream;
        AudioStream* audioStream;
    };

    Result Init();
    Result Create(Decoder*& decoder);
    Result Open(Decoder*& decoder, const std::string& filename);

    namespace producer
    {
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

            std::vector<Stream*> streams;

            std::thread thread;
            std::atomic<bool> quitting;
        };

        Result Create(Producer*& producer, Decoder*);

        Result Create(Producer*, VideoFrame*&, uint32_t size);
        Result Create(Producer*, AudioFrame*&, uint32_t nbSamples, uint32_t sampleSize, uint32_t channels);

        void   Destroy(Producer*,VideoFrame*);
        void   Destroy(Producer*,AudioFrame*);

        void   WaitForPlayback(Producer*);

    };

};

