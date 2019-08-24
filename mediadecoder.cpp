#include "precomp.h"
#include "mediadecoder.h"

#include "mediaformat.h"
#include "result.h"
#include "chrono.h"
#include "profiler.h"
#include "logger.h"
#include "curl.h"
#include "uri.h"



#include <string>
#include <vector>
#include <thread>
#include <algorithm>

#include <stdlib.h>
#include <malloc.h>

namespace {
    const uint32_t QUEUE_FULL_SLEEP_TIME_MS = 200;
    const uint32_t WAIT_PLAYBACK_SLEEP_TIME_MS = 100;

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

    Result Create(mediadecoder::Producer* producer, mediadecoder::VideoFrame*& frame, uint32_t width, uint32_t height, uint8_t** data, int32_t* linesize)
    {
        Result result;
        if( !producer->videoFramePool->pop(frame) )
        {
            frame = new mediadecoder::VideoFrame();
            memset(frame,0, sizeof(mediadecoder::VideoFrame));

            for(uint32_t i = 0; i < mediadecoder::NUM_FRAME_DATA_POINTERS; i++)
            {
                if(data[i] != NULL && linesize[i] != 0)
                {
                    frame->buffers[i] = reinterpret_cast<uint8_t*>(av_malloc(static_cast<size_t>(linesize[i]) * static_cast<size_t>(height)));
                    frame->lineSize[i] = linesize[i];
                }
            }
            frame->width = width;
            frame->height = height;
        }
        return result;
    }

    Result Create(mediadecoder::Producer* producer, mediadecoder::VideoFrame*& frame, uint32_t width, uint32_t height, uint32_t bufferSize)
    {
        Result result;
        if( !producer->videoFramePool->pop(frame) )
        {
            frame = new mediadecoder::VideoFrame();
            memset(frame,0, sizeof(mediadecoder::VideoFrame));

            frame->buffers[0] = reinterpret_cast<uint8_t*>(av_malloc(bufferSize));
            frame->lineSize[0] = bufferSize / height;
            frame->width = width;
            frame->height = height;
        }
        return result;
    }

    Result Create(mediadecoder::Producer* producer, mediadecoder::AudioFrame*& frame, uint32_t nbSamples, uint32_t sampleSize, uint32_t channels)
    {
        const uint32_t requestedBufferSize = nbSamples * sampleSize * channels;
        Result result;

        const bool haveFrame = producer->audioFramePool->pop(frame);

        if( haveFrame && !frame->inUse )
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
            // frame is in use
            if (haveFrame)
            {
                const bool outcome 
                          = producer->audioFramePool->push(frame);
                // out buffer queue is too small
                assert(outcome);
            }

            frame = new mediadecoder::AudioFrame();
            frame->samples = new uint8_t[requestedBufferSize];
            frame->sampleSize = sampleSize;
            frame->nbSamples = nbSamples;
            frame->channels = channels;
            frame->inUse = false;
        }
        return result;

    }

    void Delete(mediadecoder::VideoFrame* frame)
    {
        for(uint32_t i = 0; i < mediadecoder::NUM_FRAME_DATA_POINTERS; i++)
        {
            if(frame->buffers[i])
            {
                av_free(frame->buffers[i]);
            }
        }
        delete frame;
    }

    void Delete(mediadecoder::AudioFrame* frame)
    {
        delete [] frame->samples;
        delete frame;
    }

    template<typename T>
    void Clear(mediadecoder::FrameQueue<T>* q)
    {
        T item;
        while( q->pop(item) )
        {
            Delete(item);
        }
    }

    template<typename T>
    void Release(mediadecoder::Producer* producer, mediadecoder::FrameQueue<T>* q)
    {
        T item;
        while( q->pop(item) )
        {
            mediadecoder::Release(producer,item);
        }
    }


    bool PushFrame(mediadecoder::Producer* producer, mediadecoder::AudioFrame* audioFrame)
    {
        bool success = producer->audioQueue->push(audioFrame);
        if( success )
        {
            producer->audioQueueSize++;
        }
        else if( producer->seeking )
        {
            Delete(audioFrame);
            success = false;
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
        else if( producer->seeking )
        {
            Delete(videoFrame);
            success = false;
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

    uint32_t GetStreamFrameRate(AVCodecContext* codecContext, AVStream* st)
    {
        double frameRate = 30.0;

        if (codecContext->codec_id == AV_CODEC_ID_H264 )//mp4
        {
            frameRate = st->avg_frame_rate.num / static_cast<double>(st->avg_frame_rate.den);
        }
        else if(codecContext->codec_id == AV_CODEC_ID_MJPEG )
        {
            frameRate = st->r_frame_rate.num / static_cast<double>(st->r_frame_rate.den);
        }
        else if (codecContext->codec_id == AV_CODEC_ID_FLV1 )
        {
            frameRate = st->r_frame_rate.num / static_cast<double>(st->r_frame_rate.den);
        }
        else if (codecContext->codec_id == AV_CODEC_ID_WMV3 )
        {
            frameRate = st->r_frame_rate.num / static_cast<double>(st->r_frame_rate.den);
        }
        else if (codecContext->codec_id == AV_CODEC_ID_MPEG4 )//3gp
        {
            frameRate = st->r_frame_rate.num / static_cast<double>(st->r_frame_rate.den);
        }
        else
        {
            frameRate = st->r_frame_rate.num / static_cast<double>(st->r_frame_rate.den);
        }

        return static_cast<uint32_t>(ceil(frameRate));
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
                uint8_t* data  = (uint8_t*)(frame->data[ch] + static_cast<ptrdiff_t>(static_cast<size_t>(sampleSize)*static_cast<size_t>(i)));

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

        } while( !success && !producer->quitting && !producer->seeking );
    }

    void VideoDecoderCallback(mediadecoder::Stream* stream, mediadecoder::Producer* producer, AVFrame* frame)
    {
        profiler::ScopeProfiler profiler(profiler::PROFILER_PROCESS_VIDEO_FRAME);

        mediadecoder::VideoStream* videoStream 
                       = reinterpret_cast<mediadecoder::VideoStream*>(stream);

        const AVRational& timeBase = stream->stream->time_base;
        const uint32_t reformatBufferSize = videoStream->reformatBufferSize;
        const bool convertFrame = videoStream->swsContext != NULL;
        const double timeSeconds = static_cast<double>(frame->pts) * static_cast<double>(timeBase.num) / static_cast<double>(timeBase.den);
        const uint64_t timeUs = chrono::Microseconds(timeSeconds);

        mediadecoder::VideoFrame* videoFrame;
        Result result;

        result = convertFrame ? Create(producer, videoFrame, frame->width, frame->height, reformatBufferSize)
                              : Create(producer, videoFrame, frame->width, frame->height, frame->data, frame->linesize);

        assert(result);
        if(!result)
        {
            logger::Error("VideoDecoderCallback cannot create audio frame: %s", result.getError().c_str());
            return;
        }

        videoFrame->timeUs = timeUs;

        // Convert the video frame to output format using sws_scale
        if(videoStream->swsContext)
        {
            sws_scale(videoStream->swsContext, frame->data, frame->linesize, 0, videoStream->codecContext->height,
                      videoFrame->buffers, videoFrame->lineSize );
        }
        else
        {
            av_image_copy(videoFrame->buffers, videoFrame->lineSize, (const uint8_t**)frame->data, frame->linesize, 
                          videoStream->codecContext->pix_fmt,  frame->width, frame->height);
        }


        bool success = false;
        do
        {
            success = PushFrame(producer, videoFrame);

        } while( !success && !producer->quitting && !producer->seeking );
    }

    void Seek(mediadecoder::Producer* producer )
    {
        for( auto it = producer->streams.begin(); it != producer->streams.end(); ++it)
        {
            mediadecoder::Stream* stream = *it;
            const AVRational& timeBase = stream->stream->time_base;
            const double timeInTimeBase = chrono::Seconds(producer->seekTime) * static_cast<double>(timeBase.den) / static_cast<double>(timeBase.num);
            int outcome = av_seek_frame(producer->decoder->avFormatContext, stream->streamIndex, static_cast<int64_t>(timeInTimeBase), 0);
            if(outcome < 0 )
            {
                std::string error = ErrorToString(outcome);
                logger::Error("av_seek_frame error %s", error.c_str());
                continue;
            }
        }

        ::Release(producer, producer->videoQueue);
        ::Release(producer, producer->audioQueue);
        producer->videoQueueSize = 0;
        producer->audioQueueSize = 0;

        producer->seekTime = 0;
        producer->seeking = false;
    }

    int ReadPacket(void *opaque, uint8_t *buf, int size)
    {
        curl::Session* session = reinterpret_cast<curl::Session*>(opaque);
        size_t readBytes = 0;
        do
        {
            readBytes = curl::Read(session,buf, size);
            if( readBytes == 0 )
            {
                std::this_thread::yield();
            }

        } while( readBytes == 0 && !session->done && !session->cancel );
 
        return static_cast<int>(readBytes);
    }

    int64_t SeekPacket(void *opaque, int64_t offset, int whence)
    {
        curl::Session* session = reinterpret_cast<curl::Session*>(opaque);
        switch(whence)
        {
            case SEEK_SET:
            {
                int64_t position = curl::Seek(session, offset);

                logger::Info("SeekPacket SEEK_SET %ld Curl pos %ld Offset %ld Buffer size %ld", 
                              offset, position, session->offset.load(), session->buffer.size());

                return position;

            } break;

            case SEEK_CUR:
                logger::Info("SeekPacket SEEK_CUR %ld", offset);
                assert(0);
                return 1;
            break;

            case SEEK_END:
            {
                logger::Info("SeekPacket SEEK_END %ld Buffer size %d", offset, session->buffer.size());
                if(offset < 0)
                {
                    return -1;
                }
                return offset + session->offset + session->buffer.size();

            } break;

            break;

            case AVSEEK_SIZE:
                logger::Info("SeekPacket AVSEEK_SIZE %ld", session->buffer.size());
                if( session->buffer.size() > 0 )
                {
                    return session->buffer.size();
                }
                else
                {
                    return -1;
                }
            break;
        }
        return 1;
    }
}

namespace mediadecoder
{
    VideoFormatList outputFormats;

    VideoFormat GetOutputFormat(AVPixelFormat inputFormat, AVPixelFormat& outputPixelFormat)
    {
        // default values
        VideoFormat f = VF_RGB24;
        AVPixelFormat p = AV_PIX_FMT_RGB24;

        switch(inputFormat)
        {
            case AV_PIX_FMT_YUV420P:
                f = VF_YUV420P;
                p = AV_PIX_FMT_YUV420P;
                break;
            case AV_PIX_FMT_RGB24:
                f = VF_RGB24;
                p = AV_PIX_FMT_RGB24;
                break;
            default:
                break;
        }

        auto it = std::find(outputFormats.begin(), outputFormats.end(), f);
        if( it != outputFormats.end() )
        {
            outputPixelFormat = p;
            return f;
        }

        outputPixelFormat = AV_PIX_FMT_RGB24;
        return VF_RGB24;
    }

    Result Init()
    {
        Result result;
        return result;
    }

    void SetOutputFormat(const VideoFormatList& l)
    {
        outputFormats = l;
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
        std::string path = filename;
        Uri::Uri uri = Uri::Parse(filename);

        if( !Uri::IsLocal( &uri) )
        {
            // network stream
            curl::Session* session = NULL;
            result = curl::Create(session, path, 0);
            if(!result)
            {
                return result;
            }

            data->curl = session;
            const uint32_t avioBufferSize = 32768; // FIXME TBM_desjare: memory leak
            AVIOContext* avio = avio_alloc_context(static_cast<uint8_t*>(av_malloc(avioBufferSize)), avioBufferSize, 0, session, ReadPacket, nullptr, SeekPacket);
            data->avFormatContext->pb = avio;
            path = "curl";
        }

        int outcome = avformat_open_input(&data->avFormatContext, path.c_str(), NULL, NULL);
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
            memset(data->videoStream, 0, sizeof(VideoStream));
            data->videoStream->codecParameters = codecParameters;
            data->videoStream->codec = codec;
            data->videoStream->codecContext = codecContext;
            data->videoStream->stream = stream;
            data->videoStream->streamIndex = index;

            AVPixelFormat outputPixelFormat;
            data->videoStream->outputFormat = GetOutputFormat(codecContext->pix_fmt, outputPixelFormat);
            data->videoStream->dstFormat = outputPixelFormat;

            if(codecContext->pix_fmt != outputPixelFormat)
            {
                data->videoStream->reformatBufferSize = av_image_get_buffer_size(outputPixelFormat, codecContext->width, codecContext->height, 32);
                data->videoStream->swsContext = sws_getContext(codecContext->width, codecContext->height,
                                                              codecContext->pix_fmt, codecContext->width, codecContext->height,
                                                              outputPixelFormat, SWS_BICUBIC, NULL, NULL, NULL);
            }

            data->videoStream->processCallback = VideoDecoderCallback;
            data->videoStream->width = codecContext->width;
            data->videoStream->height = codecContext->height;
            data->videoStream->framesPerSecond = GetStreamFrameRate(codecContext, stream);

            PrintStream(*data->videoStream);
        }
        else
        {
            return Result(false, "Cannot find best video stream");
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

    VideoFormat GetOutputFormat(Decoder* decoder)
    {
        if(!decoder || !decoder->videoStream)
        {
            return VF_INVALID;
        }
        return decoder->videoStream->outputFormat;

    }

    uint64_t GetDuration(Decoder* decoder)
    {
        if(!decoder)
        {
            return 0;
        }
        return decoder->avFormatContext->duration;
    }

    uint32_t GetFramesPerSecond(Decoder* decoder)
    {
        if(!decoder || !decoder->videoStream)
        {
            return 0;
        }
        return decoder->videoStream->framesPerSecond;
    }

    void Destroy(Decoder*& decoder)
    {  
        if(!decoder)
        {
            return;
        }

        curl::Destroy(decoder->curl);
        sws_freeContext(decoder->videoStream->swsContext);

        avcodec_close(decoder->videoStream->codecContext);
        avcodec_close(decoder->audioStream->codecContext);
        avcodec_free_context(&decoder->videoStream->codecContext);
        avcodec_free_context(&decoder->audioStream->codecContext);
        avformat_free_context(decoder->avFormatContext);

        delete decoder;
        decoder = NULL;
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
            while( !ContinueDecoding(producer) && !producer->quitting && !producer->seeking )
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(QUEUE_FULL_SLEEP_TIME_MS));
            }

            if( producer->quitting )
            {
                break;
            }

            if( producer->seeking )
            {
                ::Seek(producer);
            }

            
            AVPacket* packet = av_packet_alloc();
            AVFrame* frame = av_frame_alloc();

            av_init_packet(packet);

            int32_t outcome = av_read_frame(producer->decoder->avFormatContext, packet);
            if(outcome < 0 )
            {
                if(outcome == AVERROR_EOF)
                {
                    producer->done = true;
                    av_packet_free(&packet);
                    av_frame_free(&frame);
                    return;
                }
                else
                {
                    std::string error = ErrorToString(outcome);
                    logger::Error("av_read_frame error %s", error.c_str());
                    continue;
                }
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

        const uint32_t nbSecondsPreBuffer = 10;

        const uint32_t videoQueueSize =  nbSecondsPreBuffer * GetFramesPerSecond(decoder);
        const uint32_t audioQueueSize = 32768u;

        assert(videoQueueSize > 0);
        assert(!decoder->producer);

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

    void Destroy(Producer*& producer)
    {
        if(!producer)
        {
            return;
        }

        producer->quitting = true;
        producer->thread.join();

        Clear(producer->videoQueue);
        Clear(producer->audioQueue);
        Clear(producer->videoFramePool);
        Clear(producer->audioFramePool);
        
        delete producer->videoQueue;
        delete producer->audioQueue;
        delete producer->videoFramePool;
        delete producer->audioFramePool;
 
        delete producer;
        producer = NULL;
    }

    void Seek(Producer* producer, uint64_t timeUs)
    {
        producer->seekTime = timeUs;
        producer->seeking = true;
    }

    bool IsSeeking(Producer* producer)
    {
        return producer->seeking;
    }

    void Release(Producer* producer, VideoFrame* frame)
    {
        const bool outcome
                    = producer->videoFramePool->push(frame);

        if(!outcome)
        {
            Delete(frame);
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
        // Wait for half a second playback before starting to play
        const uint32_t nbBufferForPlayback = GetFramesPerSecond(producer->decoder) / 2;
        bool haveVideo = producer->videoQueueSize > nbBufferForPlayback;

        while( !haveVideo )
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(WAIT_PLAYBACK_SLEEP_TIME_MS));
            haveVideo = producer->videoQueueSize > nbBufferForPlayback;
        }
    }
}




