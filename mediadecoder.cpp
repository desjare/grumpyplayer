
#include "mediadecoder.h"

#include "mediaformat.h"
#include "result.h"
#include "chrono.h"
#include "profiler.h"
#include "logger.h"

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

#include <stdlib.h>
#include <malloc.h>



namespace {
    const uint32_t NB_SECONDS_READ_AHEAD = 5;
    const uint32_t QUEUE_FULL_SLEEP_TIME_MS = 200;
    const uint32_t WAIT_PLAYBACK_SLEEP_TIME_MS = 100;
    const uint32_t NB_BUFFER_FOR_PLAYBACK = 60;

    SampleFormat AVFormatToSampleFormat(AVSampleFormat f)
    {
        SampleFormat sf = SF_FMT_INVALID;

        switch(f)
        {
            case AV_SAMPLE_FMT_U8:
                sf = SF_FMT_U8;
                break;
            case AV_SAMPLE_FMT_S16:
                sf = SF_FMT_S16;
                break;
            case AV_SAMPLE_FMT_S32:
                sf = SF_FMT_S32;
                break;
            case AV_SAMPLE_FMT_FLT:
                sf = SF_FMT_FLOAT;
                break;
            case AV_SAMPLE_FMT_DBL:
                sf = SF_FMT_DOUBLE;
                break;
            case AV_SAMPLE_FMT_FLTP:
                sf = SF_FMT_FLOAT;
            default:
                break;
        }

        return sf;
    }

    std::string ErrorToString(int errnum)
    {
        char buf[BUFSIZ];
        av_strerror(errnum, buf, sizeof(buf));
        return std::string(buf);
    }

    Result Create(mediadecoder::Producer* producer, mediadecoder::VideoFrame*& frame, uint32_t size)
    {
        Result result;
        if( producer->videoFramePool->pop(frame) )
        {
            if(frame->bufferSize < size)
            {
                delete [] frame->frame;
                frame->frame = new uint8_t[size];
                frame->bufferSize = size;
            }
        }
        else
        {
            frame = new mediadecoder::VideoFrame();
            frame->frame = new uint8_t[size];
            frame->bufferSize = size;
        }
        return result;
    }

    Result Create(mediadecoder::Producer* producer, mediadecoder::AudioFrame*& frame, uint32_t nbSamples, uint32_t sampleSize, uint32_t channels)
    {
        const uint32_t requestedBufferSize = nbSamples * sampleSize * channels;
        Result result;

        if( producer->audioFramePool->pop(frame) )
        {
            const uint32_t frameBufferSize = frame->nbSamples * frame->sampleSize * frame->channels;
            if(requestedBufferSize > frameBufferSize)
            {
                delete [] frame->samples;
                frame->samples = new uint8_t[requestedBufferSize];
                frame->sampleSize = sampleSize;
                frame->nbSamples = nbSamples;
                frame->channels = channels;
            }
        }
        else
        {
            frame = new mediadecoder::AudioFrame();
            frame->samples = new uint8_t[requestedBufferSize];
            frame->sampleSize = sampleSize;
            frame->nbSamples = nbSamples;
            frame->channels = channels;
        }
        return result;

    }

    bool PushFrame(mediadecoder::Producer* producer, mediadecoder::AudioFrame* audioFrame)
    {
        bool success = producer->audioQueue->push(audioFrame);
        if( success )
        {
            producer->audioQueueSize++;
        }
        else
        {
            logger::Warn("AudioDecoderCallback queue full. Waiting.");
            std::this_thread::sleep_for(std::chrono::milliseconds(QUEUE_FULL_SLEEP_TIME_MS));
        }
        return success;
    }

    bool PushFrame(mediadecoder::Producer* producer, mediadecoder::VideoFrame* videoFrame)
    {
        bool success = producer->videoQueue->push(videoFrame);
        if( success )
        {
            producer->videoQueueSize++;
        }
        else
        {
            logger::Warn("AudioDecoderCallback queue full. Waiting.");
            std::this_thread::sleep_for(std::chrono::milliseconds(QUEUE_FULL_SLEEP_TIME_MS));
        }
        return success;
    }

    void PrintStream(mediadecoder::Stream& stream)
    {
        logger::Info("Stream %s ID %d bit_rate %ld", stream.codec->long_name, 
                      stream.codec->id, stream.codecParameters->bit_rate);

        if(stream.codecParameters->codec_type == AVMEDIA_TYPE_VIDEO)
        {
            logger::Info("Video Codec: resolution %d x %d", stream.codecParameters->width,
                         stream.codecParameters->height);
        }
        else
        {
            logger::Info("Audio Codec: %d channels, sample rate %d", stream.codecParameters->channels,
                         stream.codecParameters->sample_rate);
        }
    }

    void AudioDecoderCallback(mediadecoder::Stream* stream, mediadecoder::Producer* producer, AVFrame* frame)
    {
        profiler::ScopeProfiler profiler(profiler::PROFILER_PROCESS_AUDIO_FRAME);
        const uint32_t sampleSize = av_get_bytes_per_sample(stream->codecContext->sample_fmt);
        const uint32_t channels = stream->codecContext->channels;
        const uint32_t nbSamples = frame->nb_samples;
        const AVRational& timeBase = stream->stream->time_base;
        const double timeSeconds = static_cast<double>(frame->pts) * static_cast<double>(timeBase.num) / static_cast<double>(timeBase.den);
        const uint64_t timeUs = chrono::Microseconds(timeSeconds);

        mediadecoder::AudioFrame* audioFrame;
        Result result;

        result = Create(producer, audioFrame, frame->nb_samples, sampleSize, channels);
        assert(result);
        if(!result)
        {
            logger::Error("AudioDecoderCallback cannot create audio frame: %s", result.getError().c_str());
            return;
        }

        audioFrame->timeUs = timeUs;
        uint8_t* samples = audioFrame->samples;
        uint32_t pos = 0;

        for(uint32_t i = 0; i < nbSamples; i++ )
        {
            for( uint32_t ch = 0; ch < channels; ch++)
            {
                uint8_t* data  = (uint8_t*)(frame->data[ch] + sampleSize*i);

                for( uint32_t j = 0; j < sampleSize; j++ )
                {
                    samples[pos++] = data[j];
                }
            }
        }

        bool success = false;
        do
        {
            success = PushFrame(producer, audioFrame);

        } while( !success && !producer->quitting );
    }

    void VideoDecoderCallback(mediadecoder::Stream* stream, mediadecoder::Producer* producer, AVFrame* frame)
    {
        profiler::ScopeProfiler profiler(profiler::PROFILER_PROCESS_VIDEO_FRAME);

        mediadecoder::VideoStream* videoStream 
                       = reinterpret_cast<mediadecoder::VideoStream*>(stream);

        const AVRational& timeBase = stream->stream->time_base;
        const uint32_t bufferSize = videoStream->bufferSize;
        const double timeSeconds = static_cast<double>(frame->pts) * static_cast<double>(timeBase.num) / static_cast<double>(timeBase.den);
        const uint64_t timeUs = chrono::Microseconds(timeSeconds);

        mediadecoder::VideoFrame* videoFrame;
        Result result;

        result = Create(producer, videoFrame, bufferSize);
        assert(result);
        if(!result)
        {
            logger::Error("VideoDecoderCallback cannot create audio frame: %s", result.getError().c_str());
            return;
        }

        videoFrame->timeUs = timeUs;

        sws_scale(videoStream->swsContext, frame->data, frame->linesize, 0, videoStream->codecContext->height,
                  videoStream->frame->data, videoStream->frame->linesize );

        memcpy(videoFrame->frame, videoStream->frame->data[0], bufferSize);

        bool success = false;
        do
        {
            success = PushFrame(producer, videoFrame);

        } while( !success && !producer->quitting );
    }

}

namespace mediadecoder
{
    Result Init()
    {
        Result result;
        return result;
    }

    Result Create(Decoder*& decoder)
    {
        Result result;
        decoder = new Decoder();
        memset(decoder, 0, sizeof(Decoder));
        decoder->avFormatContext = avformat_alloc_context();
        return result;
    }

    Result Open(Decoder*& data, const std::string& filename)
    {
        Result result;

        int outcome = avformat_open_input(&data->avFormatContext, filename.c_str(), NULL, NULL);
        if(outcome != 0)
        {
            std::string error = ErrorToString(outcome);
            return Result(false, "avformat_open_input error %s\n", error.c_str());
        }
        data->avFormatContext->debug = 1;
        data->avFormatContext->flags |= AVFMT_FLAG_GENPTS;

        outcome = avformat_find_stream_info(data->avFormatContext, NULL);
        if(outcome < 0)
        {
            std::string error = ErrorToString(outcome);
            return Result(false,"avformat_find_stream_info error %s\n", error.c_str());
        }

        av_dump_format(data->avFormatContext, 0, filename.c_str(), 0);

        int index = av_find_best_stream(data->avFormatContext, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
        if(index >= 0)
        {
            AVStream* stream = data->avFormatContext->streams[index];
            AVCodecParameters* codecParameters = data->avFormatContext->streams[index]->codecpar;
            AVCodec* codec = avcodec_find_decoder(codecParameters->codec_id);
            if(!codec)
            {
                return Result(false, "Cannot find decoder %s", "video");
            }

            AVDictionary* opts = NULL;
            av_dict_set(&opts, "refcounted_frames", "0", 0);

            AVCodecContext* codecContext = avcodec_alloc_context3(codec);
            avcodec_parameters_to_context(codecContext, codecParameters);
            outcome = avcodec_open2(codecContext, codec, &opts);
            if(outcome < 0)
            {
                std::string error = ErrorToString(outcome);
                return Result(false, "avcodec_open2 error %s\n", error.c_str());
            }

            codecContext->thread_count = 8;
            codecContext->thread_type = FF_THREAD_FRAME;
            
            data->videoStream = new VideoStream();
            data->videoStream->codecParameters = codecParameters;
            data->videoStream->codec = codec;
            data->videoStream->codecContext = codecContext;
            data->videoStream->stream = stream;
            data->videoStream->streamIndex = index;
            data->videoStream->frame = av_frame_alloc();
            data->videoStream->bufferSize = avpicture_get_size(AV_PIX_FMT_RGB24, codecContext->width, codecContext->height);
            data->videoStream->swsContext = sws_getContext(codecContext->width, codecContext->height,
                                                          codecContext->pix_fmt, codecContext->width, codecContext->height,
                                                          AV_PIX_FMT_RGB24, SWS_BICUBIC, NULL, NULL, NULL);

            uint8_t *buffer = static_cast<uint8_t *>(av_malloc(data->videoStream->bufferSize * sizeof(uint8_t)));
            avpicture_fill((AVPicture *)data->videoStream->frame, buffer, AV_PIX_FMT_RGB24, codecContext->width, codecContext->height);

            data->videoStream->processCallback = VideoDecoderCallback;
            data->videoStream->width = codecContext->width;
            data->videoStream->height = codecContext->height;
            PrintStream(*data->videoStream);
        }

        index = av_find_best_stream(data->avFormatContext, AVMEDIA_TYPE_AUDIO, -1, -1, NULL, 0);
        if(index >= 0)
        {
            AVStream* stream = data->avFormatContext->streams[index];
            AVCodecParameters* codecParameters = data->avFormatContext->streams[index]->codecpar;
            AVCodec* codec = avcodec_find_decoder(codecParameters->codec_id);
            if(!codec)
            {
                return Result(false, "Cannot find decoder %s", "audio");
            }

            AVDictionary* opts = NULL;
            av_dict_set(&opts, "refcounted_frames", "0", 0);

            AVCodecContext* codecContext = avcodec_alloc_context3(codec);
            avcodec_parameters_to_context(codecContext, codecParameters);
            outcome = avcodec_open2(codecContext, codec, &opts);
            if(outcome < 0)
            {
                std::string error = ErrorToString(outcome);
                return Result(false, "avcodec_open2 error %s\n", error.c_str());
            }

            data->audioStream = new AudioStream();
            data->audioStream->codecParameters = codecParameters;
            data->audioStream->codec = codec;
            data->audioStream->codecContext = codecContext;
            data->audioStream->stream = stream;
            data->audioStream->streamIndex = index;
            data->audioStream->sampleFormat = AVFormatToSampleFormat(codecContext->sample_fmt);
            data->audioStream->sampleRate = codecParameters->sample_rate;
            data->audioStream->channels = codecParameters->channels;
            data->audioStream->processCallback = AudioDecoderCallback;

            PrintStream(*data->audioStream);
        }

        return result;
    }

    void Destroy(Decoder* decoder)
    {  
        av_frame_free(&decoder->videoStream->frame);
        sws_freeContext(decoder->videoStream->swsContext);

        avcodec_close(decoder->videoStream->codecContext);
        avcodec_close(decoder->audioStream->codecContext);
        avcodec_free_context(&decoder->videoStream->codecContext);
        avcodec_free_context(&decoder->audioStream->codecContext);
        avformat_free_context(decoder->avFormatContext);

        delete decoder;
    }

    bool ContinueDecoding(Producer* producer)
    {
        const bool videoFull = producer->videoQueueSize >= producer->videoQueueCapacity;
        const bool audioFull = producer->audioQueueSize >= producer->audioQueueCapacity;
        const bool continueDecoding = !(videoFull || audioFull);

        if(!continueDecoding)
        {
            logger::Debug("ContinueDecoding queue full video %d audio %d", producer->videoQueueSize.load(),  producer->audioQueueSize.load());
        }

        return continueDecoding;
    }

    void DecoderThread(Producer* producer)
    {
        while( !producer->quitting )
        {
            while( !ContinueDecoding(producer) && !producer->quitting )
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(QUEUE_FULL_SLEEP_TIME_MS));
            }

            if( producer->quitting )
            {
                break;
            }

            
            AVPacket* packet = av_packet_alloc();
            AVFrame* frame = av_frame_alloc();

            av_init_packet(packet);

            int32_t outcome = av_read_frame(producer->decoder->avFormatContext, packet);
            if(outcome < 0 )
            {
                std::string error = ErrorToString(outcome);
                logger::Error("av_read_frame error %s", error.c_str());
                continue;
            }

            Stream* stream = producer->streams[packet->stream_index];
            AVMediaType type = stream->codec->type;

            const profiler::Point profilePoint 
                         = type == AVMEDIA_TYPE_VIDEO ? profiler::PROFILER_DECODE_VIDEO_FRAME
                                                      : profiler::PROFILER_DECODE_AUDIO_FRAME;
            profiler::StartBlock(profilePoint);

            outcome = avcodec_send_packet(stream->codecContext, packet);
            if(outcome < 0 )
            {
                std::string error = ErrorToString(outcome);
                logger::Error("accodec_send_packet error %s", error.c_str());
            }

            while( outcome >= 0 )
            {
                outcome = avcodec_receive_frame(stream->codecContext, frame);
                
                if (outcome == AVERROR(EAGAIN) || outcome == AVERROR_EOF)
                {
                    break;
                }
                else if(outcome < 0 )
                {
                     profiler::StopBlock(profilePoint);
                     std::string error = ErrorToString(outcome);
                     logger::Error("avcodec_receive_frame error %s", error.c_str());
                     return;
                }

                profiler::StopBlock(profilePoint);
                stream->processCallback(stream, producer, frame);
            }

            av_packet_free(&packet);
            av_frame_free(&frame);
        }

    }

    Result Create(Producer*& producer, Decoder* decoder)
    {
        Result result;
        const uint32_t videoQueueSize =  300u;
        const uint32_t audioQueueSize = 300u;

        if( decoder->producer != NULL )
        {
            return Result(false, "Decoder has already a producer.");
        }

        producer = new Producer();
        producer->decoder = decoder;
        producer->videoQueue = new VideoQueue(videoQueueSize);
        producer->audioQueue = new AudioQueue(audioQueueSize);
        producer->videoFramePool = new VideoQueue(videoQueueSize);
        producer->audioFramePool = new AudioQueue(audioQueueSize);
        producer->videoQueueCapacity = videoQueueSize;
        producer->audioQueueCapacity = audioQueueSize;

        uint32_t streamArraySize 
                     = std::max(decoder->videoStream->streamIndex, decoder->audioStream->streamIndex) + 1;
        producer->streams.resize(streamArraySize);
        producer->streams[decoder->videoStream->streamIndex] = decoder->videoStream;
        producer->streams[decoder->audioStream->streamIndex] = decoder->audioStream;
        producer->quitting = false;

        producer->thread = std::thread(DecoderThread, producer);

        return result;
    }

    void Destroy(Producer* producer)
    {
        producer->quitting = true;
        producer->thread.join();

        delete producer->videoQueue;
        delete producer->audioQueue;
        delete producer->videoFramePool;
        delete producer->audioFramePool;
 
        delete producer;
    }

    void Release(Producer* producer, VideoFrame* frame)
    {
        const bool outcome
                    = producer->videoFramePool->push(frame);

        if(!outcome)
        {
            delete frame->frame;
            delete frame;
        }
    }

    void Release(Producer* producer, AudioFrame* frame)
    {
        const bool outcome
                    = producer->audioFramePool->push(frame);
        if(!outcome)
        {
            delete [] frame->samples;
            delete frame;
        }
    }

    
    bool Consume(Producer* producer, VideoFrame*& videoFrame)
    {
         videoFrame = NULL;
         if( producer->videoQueue->pop(videoFrame) )
         {
             producer->videoQueueSize--;
         }
         return videoFrame != NULL;
    }

    bool Consume(Producer* producer,AudioFrame*& audioFrame)
    {
        audioFrame = NULL;
        if( producer->audioQueue->pop(audioFrame) )
        {
            producer->audioQueueSize--;
            return true;
        }
        return false;
    }

    void WaitForPlayback(Producer* producer)
    {
        bool haveVideo = producer->videoQueueSize > NB_BUFFER_FOR_PLAYBACK;
        bool haveAudio = producer->audioQueueSize > NB_BUFFER_FOR_PLAYBACK;

        while( !(haveVideo && haveAudio) )
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(WAIT_PLAYBACK_SLEEP_TIME_MS));
            
            haveVideo = producer->videoQueueSize > NB_BUFFER_FOR_PLAYBACK;
            haveAudio = producer->audioQueueSize > NB_BUFFER_FOR_PLAYBACK;
        }
    }
}




