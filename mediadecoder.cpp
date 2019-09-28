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

    // output channel mapping 
    const AudioChannelList ChannelMap2ChannelsDefault = {AC_CH_FRONT_LEFT, AC_CH_FRONT_RIGHT};
    const AudioChannelList ChannelMap3ChannelsDefault = {AC_CH_FRONT_LEFT, AC_CH_FRONT_RIGHT, AC_CH_LOW_FREQUENCY};
    const AudioChannelList ChannelMap4ChannelsDefault = {AC_CH_FRONT_LEFT, AC_CH_FRONT_RIGHT, AC_CH_BACK_LEFT, AC_CH_BACK_RIGHT};

#ifdef WIN32
    // https://docs.microsoft.com/en-us/windows/win32/xaudio2/xaudio2-default-channel-mapping
    const AudioChannelList ChannelMap5ChannelsRear = {AC_CH_FRONT_LEFT, AC_CH_FRONT_RIGHT, AC_CH_FRONT_CENTER, AC_CH_BACK_LEFT, AC_CH_BACK_RIGHT};
    const AudioChannelList ChannelMap5ChannelsSide = {AC_CH_FRONT_LEFT, AC_CH_FRONT_RIGHT, AC_CH_FRONT_CENTER, AC_CH_SIDE_LEFT, AC_CH_SIDE_RIGHT};

    const AudioChannelList ChannelMap6ChannelsRear = {AC_CH_FRONT_LEFT, AC_CH_FRONT_RIGHT, AC_CH_FRONT_CENTER, AC_CH_LOW_FREQUENCY, AC_CH_BACK_LEFT, AC_CH_BACK_RIGHT};
    const AudioChannelList ChannelMap6ChannelsSide = {AC_CH_FRONT_LEFT, AC_CH_FRONT_RIGHT, AC_CH_FRONT_CENTER, AC_CH_LOW_FREQUENCY, AC_CH_SIDE_LEFT, AC_CH_SIDE_RIGHT};

    const AudioChannelList ChannelMap8ChannelsDefault = {AC_CH_FRONT_LEFT, AC_CH_FRONT_RIGHT, AC_CH_FRONT_CENTER, AC_CH_LOW_FREQUENCY, AC_CH_SIDE_LEFT, AC_CH_SIDE_RIGHT};
#else
    // pulse audio (found no doc)
    const AudioChannelList ChannelMap5ChannelsRear = {AC_CH_FRONT_LEFT, AC_CH_FRONT_RIGHT, AC_CH_BACK_LEFT, AC_CH_BACK_RIGHT, AC_CH_FRONT_CENTER};
    const AudioChannelList ChannelMap5ChannelsSide = {AC_CH_FRONT_LEFT, AC_CH_FRONT_RIGHT, AC_CH_SIDE_LEFT, AC_CH_SIDE_RIGHT, AC_CH_FRONT_CENTER};

    const AudioChannelList ChannelMap6ChannelsRear = {AC_CH_FRONT_LEFT, AC_CH_FRONT_RIGHT, AC_CH_BACK_LEFT, AC_CH_BACK_RIGHT, AC_CH_FRONT_CENTER, AC_CH_LOW_FREQUENCY};
    const AudioChannelList ChannelMap6ChannelsSide = {AC_CH_FRONT_LEFT, AC_CH_FRONT_RIGHT, AC_CH_SIDE_LEFT, AC_CH_SIDE_RIGHT, AC_CH_FRONT_CENTER, AC_CH_LOW_FREQUENCY};

    // assuming this one
    const AudioChannelList ChannelMap8ChannelsDefault = {AC_CH_FRONT_LEFT, AC_CH_FRONT_RIGHT, AC_CH_SIDE_LEFT, AC_CH_SIDE_RIGHT, AC_CH_BACK_LEFT, AC_CH_BACK_RIGHT, AC_CH_FRONT_CENTER, AC_CH_LOW_FREQUENCY};
#endif

    const std::vector<AudioChannelList> ChannelMap2Channels = {ChannelMap2ChannelsDefault};
    const std::vector<AudioChannelList> ChannelMap3Channels = {ChannelMap3ChannelsDefault};
    const std::vector<AudioChannelList> ChannelMap4Channels = {ChannelMap4ChannelsDefault};
    const std::vector<AudioChannelList> ChannelMap5Channels = {ChannelMap5ChannelsRear, ChannelMap5ChannelsSide} ;
    const std::vector<AudioChannelList> ChannelMap6Channels = {ChannelMap6ChannelsRear, ChannelMap6ChannelsSide} ;
    const std::vector<AudioChannelList> ChannelMap8Channels = {ChannelMap8ChannelsDefault};

    std::map<uint32_t, const std::vector<AudioChannelList>> ChannelsToChanelMaps = { 
        {2,ChannelMap2Channels}, 
        {3,ChannelMap3Channels}, 
        {4,ChannelMap3Channels}, 
        {5, ChannelMap5Channels},
        {6, ChannelMap6Channels},
        {8, ChannelMap8Channels}
    };

    void GetChannelInputToOutputMap(const AudioChannelList& input, std::map<uint32_t, uint32_t>& outputMap)
    {
        const size_t channels = input.size();
        outputMap.clear();

        // mono
        if(channels == 0)
        {
            outputMap[0] = 0;
            return;
        }

        auto it = ChannelsToChanelMaps.find(static_cast<uint32_t>(input.size()));
        if( it != ChannelsToChanelMaps.end() )
        {
             const std::vector<AudioChannelList>& channelSetups = it->second;

             for(auto setup:channelSetups)
             {
                 for(uint32_t i = 0; i < channels; i++)
                 {
                     for(uint32_t j = 0; j < channels; j++)
                     {
                         if(input[i] == setup[j])
                         {
                             outputMap[i] = j;
                             break;
                         }
                     }
                 }

                 // we are done, all are mapped properly
                 if(outputMap.size() == input.size())
                 {
                      return;
                 }
                 else
                 {
                     // missing channels in setup
                     outputMap.clear();
                 }

             }
        }
        
        // use no mapping
        logger::Warn("GetChannelInputToOutputMap: no mapping found.");
        for(uint32_t i = 0; i < channels; i++)
        {
            outputMap[i] = i;
        }

    }

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

            for(uint32_t i = 0; i < mediadecoder::NUM_FRAME_DATA_POINTERS; i++)
            {
                if(data[i] != nullptr && linesize[i] != 0)
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

    void AudioDecoderCallback(mediadecoder::Stream* stream, mediadecoder::Producer* producer, AVFrame* frame, AVPacket* packet)
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

        mediadecoder::AudioStream* audioStream 
                       = reinterpret_cast<mediadecoder::AudioStream*>(stream);

        const std::map<uint32_t, uint32_t>& inputToOutputMapping = audioStream->channelInputToOutputMapping;
        audioFrame->timeUs = timeUs;
        uint8_t* samples = audioFrame->samples;
        uint32_t pos = 0;

        for(uint32_t i = 0; i < nbSamples; i++ )
        {
            for( uint32_t ch = 0; ch < channels; ch++)
            {
                std::map<uint32_t, uint32_t>::const_iterator it = inputToOutputMapping.find(ch);
                assert(it != inputToOutputMapping.end());

                const uint32_t outputChannel = it->second;
                uint8_t* data  = (uint8_t*)(frame->data[outputChannel] + static_cast<ptrdiff_t>(static_cast<size_t>(sampleSize)*static_cast<size_t>(i)));

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

    void VideoDecoderCallback(mediadecoder::Stream* stream, mediadecoder::Producer* producer, AVFrame* frame, AVPacket*)
    {
        profiler::ScopeProfiler profiler(profiler::PROFILER_PROCESS_VIDEO_FRAME);

        mediadecoder::VideoStream* videoStream 
                       = reinterpret_cast<mediadecoder::VideoStream*>(stream);

        const AVRational& timeBase = stream->stream->time_base;
        const uint32_t reformatBufferSize = videoStream->reformatBufferSize;
        const bool convertFrame = videoStream->swsContext != nullptr;
        const double timeSeconds = static_cast<double>(frame->pts) * static_cast<double>(timeBase.num) / static_cast<double>(timeBase.den);
        const uint64_t timeUs = chrono::Microseconds(timeSeconds);
        producer->currentDecodingTimeUs = timeUs;

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

    void SubtitleDecoderCallback(mediadecoder::Stream* stream, mediadecoder::Producer* producer, AVFrame* frame, AVPacket* packet)
    {
        mediadecoder::SubtitleStream* subStream = reinterpret_cast<mediadecoder::SubtitleStream*>(stream);

        AVSubtitle avSub;
        int32_t gotSub = 1;

        int32_t outcome = avcodec_decode_subtitle2(stream->codecContext, &avSub, &gotSub, packet);
        if(outcome < 0)
        {
            std::string error = ErrorToString(outcome);
            logger::Error("avcodec_decode_subtitle2 error %s", error.c_str());
        }

        mediadecoder::Subtitle* sub = new mediadecoder::Subtitle();

        // by default, use current decoding time
        sub->startTimeUs = producer->currentDecodingTimeUs;
        sub->endTimeUs = sub->startTimeUs + chrono::Microseconds(mediadecoder::DEFAULT_SUBTITLE_DURATION_SEC);

        // if we have a valid pts, use it
        const AVRational& timeBase = stream->stream->time_base;
        const double timeSeconds = static_cast<double>(avSub.pts) * static_cast<double>(timeBase.num) / static_cast<double>(timeBase.den);
        if(timeSeconds > 0.0)
        {
            const uint64_t timeUs = chrono::Microseconds(timeSeconds);
            sub->startTimeUs = timeUs;
        }

        // start_display_time & end_display_time in ms
        if(avSub.start_display_time != 0)
        {
            sub->startTimeUs = sub->startTimeUs + static_cast<uint64_t>(avSub.start_display_time) * 1000L;
        }
        if(avSub.end_display_time != 0)
        {
            sub->endTimeUs = sub->startTimeUs + static_cast<uint64_t>(avSub.end_display_time) * 1000;
        }

        for(uint32_t i = 0; i < avSub.num_rects; i++)
        {
            AVSubtitleRect* rect = avSub.rects[i];

            if((rect->type == SUBTITLE_TEXT || rect->type == SUBTITLE_ASS) && avSub.num_rects > 1)
            {
                logger::Warn("Decoding subtitle has %d rects", avSub.num_rects);
            }

            if(rect->type == SUBTITLE_TEXT)
            {
                logger::Debug("Text Sub %s", rect->text);
                sub->text.push_back(rect->text);
            }
            else if(rect->type == SUBTITLE_ASS)
            {
                subtitle::SubStationAlphaDialogue* dialogue = nullptr;
                subtitle::SubStationAlphaHeader* header = subStream->subtitleHeader;

                Result result = subtitle::Parse(rect->ass, subStream->subtitleHeader, dialogue);
                if(!result)
                {
                    logger::Error("Error parsing dialogue: %s", result.getError().c_str());
                    delete dialogue;
                    continue;
                }

                sub->text.push_back(dialogue->text);
                sub->header = header;
                sub->dialogue = dialogue;
            }
            else
            {
                logger::Warn("Unsupported subtitle type %d", rect->type);
            }
        }

        producer->subtitleQueue->push(sub);
        producer->subtitleQueueSize++;

        avsubtitle_free(&avSub);
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

        // seek subtitles
        for(auto it = producer->decoder->subRips.begin(); it != producer->decoder->subRips.end(); ++it)
        {
            mediadecoder::SubtitleSubRip& srt = it->second;

            for( srt.posIt = srt.subs->diags.begin() ; srt.posIt != srt.subs->diags.end() && producer->seekTime != 0; ++srt.posIt)
            {
                const subtitle::SubRipDialogue& d = *srt.posIt;
                if(d.startTimeUs > producer->seekTime)
                {
                    logger::Info("Done Seek subtitle at %f seconds", chrono::Seconds(d.startTimeUs));
                    break;
                }
            }
        }

        ::Release(producer, producer->videoQueue);
        ::Release(producer, producer->audioQueue);
        ::Release(producer, producer->subtitleQueue);
        producer->videoQueueSize = 0;
        producer->audioQueueSize = 0;
        producer->subtitleQueueSize = 0;

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
            curl::Session* session = nullptr;
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

        int outcome = avformat_open_input(&data->avFormatContext, path.c_str(), nullptr, nullptr);
        if(outcome != 0)
        {
            std::string error = ErrorToString(outcome);
            return Result(false, "avformat_open_input error %s\n", error.c_str());
        }
        data->avFormatContext->debug = 1;
        data->avFormatContext->flags |= AVFMT_FLAG_GENPTS;

        outcome = avformat_find_stream_info(data->avFormatContext, nullptr);
        if(outcome < 0)
        {
            std::string error = ErrorToString(outcome);
            return Result(false,"avformat_find_stream_info error %s\n", error.c_str());
        }

        av_dump_format(data->avFormatContext, 0, filename.c_str(), 0);

        int index = av_find_best_stream(data->avFormatContext, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
        if(index >= 0)
        {
            AVStream* stream = data->avFormatContext->streams[index];
            AVCodecParameters* codecParameters = data->avFormatContext->streams[index]->codecpar;
            AVCodec* codec = avcodec_find_decoder(codecParameters->codec_id);
            if(!codec)
            {
                return Result(false, "Cannot find decoder %s", "video");
            }

            AVDictionary* opts = nullptr;
            av_dict_set(&opts, "refcounted_frames", "0", 0);

            AVCodecContext* codecContext = avcodec_alloc_context3(codec);
            avcodec_parameters_to_context(codecContext, codecParameters);
            outcome = avcodec_open2(codecContext, codec, &opts);
            if(outcome < 0)
            {
                std::string error = ErrorToString(outcome);
                return Result(false, "avcodec_open2 error %s\n", error.c_str());
            }

            uint32_t framesPerSecond = GetStreamFrameRate(codecContext, stream);
            if(framesPerSecond <= MAX_FRAME_RATE)
            {
                codecContext->thread_count = 8;
                codecContext->thread_type = FF_THREAD_FRAME;
                
                data->videoStream = new VideoStream();
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
                                                                  outputPixelFormat, SWS_BICUBIC, nullptr, nullptr, nullptr);
                }

                data->videoStream->processCallback = VideoDecoderCallback;
                data->videoStream->width = codecContext->width;
                data->videoStream->height = codecContext->height;
                data->videoStream->framesPerSecond = framesPerSecond;

                PrintStream(*data->videoStream);
            }
            else
            {
                avcodec_close(codecContext);
                avcodec_free_context(&codecContext);
                logger::Warn("Decoder skipping video stream frame rate %d", framesPerSecond);
            }
        }

        index = av_find_best_stream(data->avFormatContext, AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0);
        if(index >= 0)
        {
            AVStream* stream = data->avFormatContext->streams[index];
            AVCodecParameters* codecParameters = data->avFormatContext->streams[index]->codecpar;
            AVCodec* codec = avcodec_find_decoder(codecParameters->codec_id);
            if(!codec)
            {
                return Result(false, "Cannot find decoder %s", "audio");
            }

            AVDictionary* opts = nullptr;
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
            data->audioStream->channelMapping.resize(codecParameters->channels);

            // channel layouts
            const uint64_t channelLayout = codecContext->channel_layout;
            std::map<uint32_t, AudioChannel> AVAudioChannelMap;
            AVAudioChannelMap[AV_CH_FRONT_LEFT] = AC_CH_FRONT_LEFT;
            AVAudioChannelMap[AV_CH_FRONT_RIGHT] = AC_CH_FRONT_RIGHT;
            AVAudioChannelMap[AV_CH_FRONT_CENTER] = AC_CH_FRONT_CENTER;
            AVAudioChannelMap[AV_CH_LOW_FREQUENCY] = AC_CH_LOW_FREQUENCY;
            AVAudioChannelMap[AV_CH_BACK_LEFT] = AC_CH_BACK_LEFT;
            AVAudioChannelMap[AV_CH_BACK_RIGHT] = AC_CH_BACK_RIGHT;
            AVAudioChannelMap[AV_CH_FRONT_LEFT_OF_CENTER] = AC_CH_FRONT_LEFT_OF_CENTER;
            AVAudioChannelMap[AV_CH_FRONT_RIGHT_OF_CENTER] = AC_CH_FRONT_RIGHT_OF_CENTER;
            AVAudioChannelMap[AV_CH_BACK_CENTER] = AC_CH_BACK_CENTER;
            AVAudioChannelMap[AV_CH_SIDE_LEFT] = AC_CH_SIDE_LEFT;
            AVAudioChannelMap[AV_CH_SIDE_RIGHT] = AC_CH_SIDE_RIGHT;
            AVAudioChannelMap[AV_CH_TOP_CENTER] = AC_CH_TOP_CENTER;
            AVAudioChannelMap[AV_CH_TOP_FRONT_LEFT] = AC_CH_TOP_FRONT_LEFT;
            AVAudioChannelMap[AV_CH_TOP_FRONT_CENTER] = AC_CH_TOP_FRONT_CENTER;
            AVAudioChannelMap[AV_CH_TOP_FRONT_RIGHT] = AC_CH_TOP_FRONT_RIGHT;
            AVAudioChannelMap[AV_CH_TOP_BACK_LEFT] = AC_CH_TOP_BACK_LEFT;
            AVAudioChannelMap[AV_CH_TOP_BACK_CENTER] = AC_CH_TOP_BACK_CENTER;
            AVAudioChannelMap[AV_CH_TOP_BACK_RIGHT] = AC_CH_TOP_BACK_RIGHT;

            for(auto it = AVAudioChannelMap.begin(); it != AVAudioChannelMap.end(); ++it)
            {
                int index = av_get_channel_layout_channel_index(channelLayout, it->first);
                if(index >= 0)
                {
                    data->audioStream->channelMapping[index] = it->second;
                    logger::Info("Channel %s at %d", av_get_channel_name(it->first), index);
                }
            }

            for(auto it = data->audioStream->channelMapping.begin(); it !=  data->audioStream->channelMapping.end(); ++it)
            {
                assert(*it != AC_CH_INVALID);
            }

            // input to output mapping
            const AudioChannelList& channels = data->audioStream->channelMapping;
            GetChannelInputToOutputMap(channels, data->audioStream->channelInputToOutputMapping);

            PrintStream(*data->audioStream);
        }

        // subtitle streams
        for(uint32_t index = 0; index < data->avFormatContext->nb_streams; index++)
        {
            AVStream* stream = data->avFormatContext->streams[index];
            AVCodecParameters* codecParameters = data->avFormatContext->streams[index]->codecpar;
            AVCodec* codec = avcodec_find_decoder(codecParameters->codec_id);
            if(!codec)
            {
                return Result(false, "Cannot find decoder %s", "audio");
            }

            if(codec->type == AVMEDIA_TYPE_SUBTITLE)
            {
                AVCodecContext* codecContext = avcodec_alloc_context3(codec);
                avcodec_parameters_to_context(codecContext, codecParameters);
                outcome = avcodec_open2(codecContext, codec, nullptr);
                if(outcome < 0)
                {
                    std::string error = ErrorToString(outcome);
                    return Result(false, "avcodec_open2 error %s\n", error.c_str());
                }

                SubtitleStream* subtitleStream = new SubtitleStream();
                subtitleStream->codecParameters = codecParameters;
                subtitleStream->codec = codec;
                subtitleStream->codecContext = codecContext;
                subtitleStream->stream = stream;
                subtitleStream->streamIndex = index;
                subtitleStream->processCallback = SubtitleDecoderCallback;

                // ssa / ass
                if(codecContext->subtitle_header)
                {
                    std::string ssa(reinterpret_cast<char*>(codecContext->subtitle_header), codecContext->subtitle_header_size);
                    subtitle::SubStationAlphaHeader* subtitleHeader = nullptr;

                    Result subtitleResult = subtitle::Parse(ssa, subtitleHeader);
                    if(!subtitleResult)
                    {
                        logger::Error("Could not parse ass subtitle header %s", subtitleResult.getError());
                        delete subtitleStream; subtitleStream = nullptr;
                        continue;
                    }
                    else
                    {
                        subtitleStream->subtitleHeader = subtitleHeader;
                    }
                }

                data->subtitleIndexes.push_back(index);
                data->subtitleStreams.push_back(subtitleStream);
            }
        }
        data->nextSubtitleIndex = data->avFormatContext->nb_streams + 1;

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

    uint32_t GetVideoWidth(Decoder* decoder)
    {
        if(!decoder || !decoder->videoStream)
        {
            return 0;
        }
        return decoder->videoStream->width;
    }

    uint32_t GetVideoHeight(Decoder* decoder)
    {
        if(!decoder || !decoder->videoStream)
        {
            return 0;
        }
        return decoder->videoStream->height;
    }

    uint32_t GetAudioNumChannels(Decoder* decoder)
    {
        if(!decoder || !decoder->audioStream)
        {
            return 0;
        }
        return decoder->audioStream->channels;
    }

    uint32_t GetAudioSampleRate(Decoder* decoder)
    {
        if(!decoder || !decoder->audioStream)
        {
            return 0;
        }
        return decoder->audioStream->sampleRate;
    }

    SampleFormat GetAudioSampleFormat(Decoder* decoder)
    {
        if(!decoder || !decoder->audioStream)
        {
            return SF_FMT_INVALID;
        }
        return decoder->audioStream->sampleFormat;
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

    bool GetHaveAudio(Decoder* decoder)
    {
        if(!decoder || !decoder->audioStream)
        {
            return false;
        }
        return true;
    }

    bool GetHaveVideo(Decoder* decoder)
    {
        if(!decoder || !decoder->videoStream)
        {
            return false;
        }
        return true;
    }


    void ToggleSubtitleTrack(Decoder* decoder)
    {
        if(!decoder || !decoder->videoStream)
        {
            return;
        }
        decoder->subtitleIndex = (decoder->subtitleIndex + 1) % static_cast<int32_t>(decoder->subtitleIndexes.size());
        logger::Info("Subtitle set to stream index %d", decoder->subtitleIndexes[decoder->subtitleIndex]);
    }

    void AddSubtitleTrack(Decoder* decoder, std::shared_ptr<subtitle::SubRip> track)
    {
        if(!decoder || !decoder->videoStream)
        {
            return;
        }
        
        SubtitleSubRip subRip;
        subRip.subs = track;
        subRip.posIt = track->diags.begin();

        decoder->subtitleIndexes.push_back(decoder->nextSubtitleIndex);
        decoder->subRips[decoder->nextSubtitleIndex++] = subRip;
    }

    void Destroy(Decoder*& decoder)
    {  
        if(!decoder)
        {
            return;
        }

        curl::Destroy(decoder->curl);

        if(decoder->videoStream)
        {
            sws_freeContext(decoder->videoStream->swsContext);

            avcodec_close(decoder->videoStream->codecContext);
            avcodec_free_context(&decoder->videoStream->codecContext);
        }
        if(decoder->audioStream)
        {
            avcodec_close(decoder->audioStream->codecContext);
            avcodec_free_context(&decoder->audioStream->codecContext);
        }
        avformat_free_context(decoder->avFormatContext);

        delete decoder;
        decoder = nullptr;
    }

    bool ContinueDecoding(Producer* producer)
    {
        const bool videoFull = producer->videoQueueSize >= producer->videoQueueCapacity && GetHaveVideo(producer->decoder);
        const bool audioFull = producer->audioQueueSize >= producer->audioQueueCapacity && GetHaveAudio(producer->decoder);
        const bool continueDecoding = !(videoFull || audioFull);

        if(!continueDecoding)
        {
            logger::Trace("ContinueDecoding queue full video %d audio %d", producer->videoQueueSize.load(),  producer->audioQueueSize.load());
        }

        return continueDecoding;
    }

    void ProcessSrt(Producer* producer)
    {
        const uint32_t MAX_SUBTITLE_QUEUE_SIZE = 100;

        Decoder* decoder = producer->decoder;

        if(producer->subtitleQueueSize >= MAX_SUBTITLE_QUEUE_SIZE)
        {
            return;
        }

        int32_t subtitleStreamIndex = decoder->subtitleIndexes[decoder->subtitleIndex];
        auto it = decoder->subRips.find(subtitleStreamIndex);
        if(it != decoder->subRips.end())
        {
            SubtitleSubRip& srt = it->second;

            if(srt.posIt != srt.subs->diags.end())
            {
               
                const subtitle::SubRipDialogue& d = *srt.posIt;
                mediadecoder::Subtitle* sub = new mediadecoder::Subtitle();
                for(auto it = d.lines.begin(); it != d.lines.end(); ++it)
                {
                    sub->text.push_back((*it).text);
                }
                sub->startTimeUs = d.startTimeUs;
                sub->endTimeUs = d.endTimeUs;

                producer->subtitleQueue->push(sub);
                producer->subtitleQueueSize++;
                srt.posIt++;
            }
        }
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

            ProcessSrt(producer);

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

            Stream* stream = static_cast<size_t>(packet->stream_index) < producer->streams.size() ? 
                                                          producer->streams[packet->stream_index] : nullptr;
            if(!stream)
            {
                continue;
            }

            const AVMediaType type = stream->codec->type;

            if(type == AVMEDIA_TYPE_VIDEO || type == AVMEDIA_TYPE_AUDIO)
            {
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
                    stream->processCallback(stream, producer, frame, packet);
                }
            }
            else if(type == AVMEDIA_TYPE_SUBTITLE)
            {
                int32_t subtitleStreamIndex = producer->decoder->subtitleIndexes[producer->decoder->subtitleIndex];
                if(packet->stream_index == subtitleStreamIndex )
                {
                    logger::Debug("Got subtitle decoder index %d index %d", producer->decoder->subtitleIndex, subtitleStreamIndex);
                    stream->processCallback(stream, producer, frame, packet);
                }
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
        const uint32_t subtitleQueueSize = 32768u;

        assert(!decoder->producer);

        if( decoder->producer != nullptr )
        {
            return Result(false, "Decoder has already a producer.");
        }

        producer = new Producer();
        producer->decoder = decoder;

        if(decoder->videoStream != nullptr)
        {
            producer->videoQueue = new VideoQueue(videoQueueSize);
            producer->videoFramePool = new VideoQueue(videoQueueSize);
            producer->videoQueueCapacity = videoQueueSize;
        }
        if(decoder->audioStream != nullptr)
        {
            producer->audioQueue = new AudioQueue(audioQueueSize);
            producer->audioFramePool = new AudioQueue(audioQueueSize);
            producer->audioQueueCapacity = audioQueueSize;
        }

        producer->subtitleQueue = new SubtitleQueue(subtitleQueueSize);
        producer->subtitleQueueCapacity = subtitleQueueSize;
        producer->streams.resize(decoder->avFormatContext->nb_streams);
        
        if(decoder->videoStream != nullptr)
        {
            producer->streams[decoder->videoStream->streamIndex] = decoder->videoStream;
        }
        if(decoder->audioStream != nullptr)
        {
            producer->streams[decoder->audioStream->streamIndex] = decoder->audioStream;
        }

        for(auto streamIt = decoder->subtitleStreams.begin(); streamIt != decoder->subtitleStreams.end(); ++streamIt)
        {
            producer->streams[(*streamIt)->streamIndex] = *streamIt;
        }

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
        producer = nullptr;
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

    void Release(Producer* producer, Subtitle* subtitle)
    {
        delete subtitle->dialogue;
        delete subtitle;
    }
    
    bool Consume(Producer* producer, VideoFrame*& videoFrame)
    {
        videoFrame = nullptr;
        if(!GetHaveVideo(producer->decoder))
        {
            return false;
        }

        if( producer->videoQueue->pop(videoFrame) )
        {
            producer->videoQueueSize--;
        }
        return videoFrame != nullptr;
    }

    bool Consume(Producer* producer,AudioFrame*& audioFrame)
    {
        audioFrame = nullptr;
        if(!GetHaveAudio(producer->decoder))
        {
            return false;
        }

        if( producer->audioQueue->pop(audioFrame) )
        {
            producer->audioQueueSize--;
            return true;
        }
        return false;
    }

    bool Consume(Producer* producer, Subtitle*& subtitle)
    {
         subtitle = nullptr;
         if( producer->subtitleQueue->pop(subtitle) )
         {
             producer->subtitleQueueSize--;
         }

         return subtitle != nullptr;
    }

    void WaitForPlayback(Producer* producer)
    {
        if(!GetHaveVideo(producer->decoder))
        {
            return;
        }

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




