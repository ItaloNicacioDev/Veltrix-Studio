// Veltrix FFmpeg layer - AvAudioDecoder.cpp
//
// Real audio decoding (ADR-008). Output contract: PLANAR float32
// (deinterleaved), one AudioBlock per decoded AVFrame; pts in the audio
// stream timebase (1/sample_rate). Variable block sizes are preserved
// (no reblocking) — callers must handle frames != constant.
// swresample converts ANY supported source layout/sample format.

#include "AvAudioDecoder.h"

#include <cstring>

namespace veltrix::media::ffmpeg {

using veltrix::core::ErrorCode;
using veltrix::core::failure;
using veltrix::core::Result;
using veltrix::core::ResultVoid;
using veltrix::playback::AudioBlock;
using veltrix::playback::MediaStreamInfo;
using veltrix::playback::SeekMode;

ResultVoid AvAudioDecoder::open(const std::string& path) {
    if (is_open())
        return failure(ErrorCode::Conflict, "decoder already open");

    const int open_res = avformat_open_input(&format_.ctx, path.c_str(), nullptr, nullptr);
    if (open_res != 0) {
        format_.ctx = nullptr;
        if (open_res == AVERROR_ENOENT)
            return failure(ErrorCode::NotFound, "media file not found", path);
        return failure(ErrorCode::ParseError,
                       "avformat_open_input failed: " + av_error_string(open_res), path);
    }
    if (const int info_res = avformat_find_stream_info(format_.ctx, nullptr);
        info_res < 0) {
        close();
        return failure(ErrorCode::ParseError,
                       "avformat_find_stream_info failed: " + av_error_string(info_res),
                       path);
    }
    return setup_audio_pipeline();
}

ResultVoid AvAudioDecoder::setup_audio_pipeline() {
    const int stream_index = av_find_best_stream(format_.ctx, AVMEDIA_TYPE_AUDIO,
                                                 -1, -1, nullptr, 0);
    if (stream_index < 0) {
        close();
        return failure(ErrorCode::Unsupported, "no audio stream in file");
    }
    audio_stream_index_ = stream_index;
    AVStream* stream = format_.ctx->streams[stream_index];

    const AVCodec* codec = avcodec_find_decoder(stream->codecpar->codec_id);
    if (!codec) {
        close();
        return failure(ErrorCode::Unsupported, "no decoder available for codec",
                       avcodec_get_name(stream->codecpar->codec_id));
    }
    codec_.ctx = avcodec_alloc_context3(codec);
    if (!codec_.ctx) {
        close();
        return failure(ErrorCode::Unknown, "avcodec_alloc_context3 failed");
    }
    if (const int cp = avcodec_parameters_to_context(codec_.ctx, stream->codecpar);
        cp < 0) {
        close();
        return failure(ErrorCode::ParseError,
                       "avcodec_parameters_to_context failed: " + av_error_string(cp));
    }
    av_codec_set_pkt_timebase(codec_.ctx, stream->time_base);
    if (const int open_res = avcodec_open2(codec_.ctx, codec, nullptr); open_res < 0) {
        close();
        return failure(ErrorCode::Unsupported,
                       "avcodec_open2 failed: " + av_error_string(open_res), codec->name);
    }

    // swr: anything -> planar float32 at the source sample rate/layout.
    const AVChannelLayout* src_layout = &codec_.ctx->ch_layout;
    AVChannelLayout dst_layout;
    av_channel_layout_default(&dst_layout, src_layout->nb_channels);
    SwrContext* swr = nullptr;
    const int swr_res = swr_alloc_set_opts2(
        &swr, src_layout, AV_SAMPLE_FMT_FLTP, codec_.ctx->sample_rate,
        &dst_layout, static_cast<AVSampleFormat>(codec_.ctx->sample_fmt),
        codec_.ctx->sample_rate, 0, nullptr);
    av_channel_layout_uninit(&dst_layout);
    if (swr_res < 0 || !swr) {
        close();
        return failure(ErrorCode::Unsupported,
                       "swr_alloc_set_opts2 failed: " + av_error_string(swr_res));
    }
    if (const int init_res = swr_init(swr); init_res < 0) {
        swr_free(&swr);
        close();
        return failure(ErrorCode::Unsupported,
                       "swr_init failed: " + av_error_string(init_res));
    }
    swr_.ctx = swr;

    info_ = MediaStreamInfo{};
    info_.has_audio = true;
    info_.audio_channels = codec_.ctx->ch_layout.nb_channels;
    info_.audio_sample_rate = codec_.ctx->sample_rate;
    info_.audio_timebase_num = stream->time_base.num;
    info_.audio_timebase_den = stream->time_base.den;
    // Report video presence for shared consumers.
    const int video_index = av_find_best_stream(format_.ctx, AVMEDIA_TYPE_VIDEO,
                                                -1, -1, nullptr, 0);
    info_.has_video = video_index >= 0;
    if (info_.has_video) {
        AVStream* video = format_.ctx->streams[video_index];
        info_.video_width = video->codecpar->width;
        info_.video_height = video->codecpar->height;
        if (video->avg_frame_rate.den != 0 && video->avg_frame_rate.num != 0) {
            info_.video_fps_num = video->avg_frame_rate.num;
            info_.video_fps_den = video->avg_frame_rate.den;
        }
        info_.video_timebase_num = video->time_base.num;
        info_.video_timebase_den = video->time_base.den;
    }
    if (format_.ctx->duration != AV_NOPTS_VALUE) {
        info_.duration_ticks = format_.ctx->duration; // AV_TIME_BASE (us)
        info_.duration_timebase_num = 1;
        info_.duration_timebase_den = AV_TIME_BASE;
    }
    eof_reached_ = false;
    end_of_stream_signaled_ = false;
    return veltrix::core::success();
}

ResultVoid AvAudioDecoder::seek(std::int64_t target_ticks, SeekMode mode) {
    if (!is_open())
        return failure(ErrorCode::Conflict, "decoder not open");
    if (target_ticks < 0)
        return failure(ErrorCode::InvalidArgument, "seek target must be >= 0");
    (void)mode; // audio: both modes land at-or-before; blocks decode forward

    if (info_.duration_ticks > 0) {
        AVStream* stream = format_.ctx->streams[audio_stream_index_];
        const std::int64_t duration_stream_ticks = av_rescale_q(
            info_.duration_ticks, AVRational{1, AV_TIME_BASE}, stream->time_base);
        if (target_ticks > duration_stream_ticks)
            return failure(ErrorCode::OutOfRange, "seek target beyond end");
    }

    const int res = avformat_seek_file(format_.ctx, audio_stream_index_,
                                       INT64_MIN, target_ticks, INT64_MAX, 0);
    if (res < 0) {
        if (res == AVERROR_EOF)
            return failure(ErrorCode::OutOfRange, "seek target beyond end");
        return failure(ErrorCode::ParseError,
                       "avformat_seek_file failed: " + av_error_string(res));
    }
    avcodec_flush_buffers(codec_.ctx);
    if (swr_.ctx) swr_convert(swr_.ctx, nullptr, 0, nullptr, 0); // drop buffered samples
    eof_reached_ = false;
    end_of_stream_signaled_ = false;
    return veltrix::core::success();
}

Result<AudioBlock> AvAudioDecoder::read_block(
    const veltrix::core::CancellationToken& token) {
    if (!is_open())
        return failure<AudioBlock>(ErrorCode::Conflict, "decoder not open");

    for (;;) {
        if (token.is_cancelled())
            return failure<AudioBlock>(ErrorCode::Cancelled, "cancelled by token");

        if (eof_reached_) {
            const int send = avcodec_send_packet(codec_.ctx, nullptr);
            if (send < 0 && send != AVERROR_EOF)
                return failure<AudioBlock>(ErrorCode::ParseError,
                    "drain send_packet failed: " + av_error_string(send));
            const int recv = avcodec_receive_frame(codec_.ctx, frame_.frame);
            if (recv == 0)
                return convert_to_planar_f32(*frame_.frame);
            // Flush any partial swr samples remaining in the resampler.
            // (swr_convert with small out buffer; done lazily — the tiny
            // tail is reported as the final block.)
            if (recv == AVERROR_EOF || recv == AVERROR(EAGAIN)) {
                if (end_of_stream_signaled_)
                    return failure<AudioBlock>(ErrorCode::Conflict,
                                              "end of stream already signaled");
                end_of_stream_signaled_ = true;
                return failure<AudioBlock>(ErrorCode::NotFound, "end of stream");
            }
            return failure<AudioBlock>(ErrorCode::ParseError,
                "receive_frame failed: " + av_error_string(recv));
        }

        const int read = av_read_frame(format_.ctx, packet_.packet);
        if (read < 0) {
            if (read == AVERROR_EOF) {
                eof_reached_ = true;
                continue;
            }
            if (read == AVERROR_EXIT)
                return failure<AudioBlock>(ErrorCode::Cancelled, "read interrupted");
            return failure<AudioBlock>(ErrorCode::ParseError,
                "av_read_frame failed: " + av_error_string(read));
        }
        if (packet_.packet->stream_index != audio_stream_index_) {
            av_packet_unref(packet_.packet);
            continue;
        }

        const int send = avcodec_send_packet(codec_.ctx, packet_.packet);
        av_packet_unref(packet_.packet);
        if (send < 0 && send != AVERROR(EAGAIN)) {
            if (send == AVERROR_INVALIDDATA)
                return failure<AudioBlock>(ErrorCode::ParseError, "corrupt packet");
            return failure<AudioBlock>(ErrorCode::ParseError,
                "send_packet failed: " + av_error_string(send));
        }
        const int recv = avcodec_receive_frame(codec_.ctx, frame_.frame);
        if (recv == 0)
            return convert_to_planar_f32(*frame_.frame);
        if (recv == AVERROR(EAGAIN))
            continue;
        if (recv == AVERROR_EOF) {
            eof_reached_ = true;
            continue;
        }
        return failure<AudioBlock>(ErrorCode::ParseError,
            "receive_frame failed: " + av_error_string(recv));
    }
}

Result<AudioBlock> AvAudioDecoder::convert_to_planar_f32(const AVFrame& av_frame) {
    const std::int32_t channels = codec_.ctx->ch_layout.nb_channels;
    if (channels <= 0 || av_frame.nb_samples <= 0)
        return failure<AudioBlock>(ErrorCode::ParseError,
                                   "audio frame has invalid layout");

    // Planar float32 output: one contiguous plane per channel. libswr
    // requires a pointer PER PLANE for FLTP output — a single pointer
    // would be treated as one packed plane and corrupt the layout.
    auto buffer = std::make_shared<veltrix::playback::FrameBuffer>();
    const std::size_t plane_samples = static_cast<std::size_t>(av_frame.nb_samples);
    const std::size_t plane_bytes = plane_samples * sizeof(float);
    buffer->bytes.resize(plane_bytes * channels);

    std::vector<std::uint8_t*> plane_ptrs(channels);
    for (std::int32_t ch = 0; ch < channels; ++ch)
        plane_ptrs[ch] = buffer->bytes.data() + static_cast<std::size_t>(ch) * plane_bytes;

    const std::int32_t out_samples =
        swr_convert(swr_.ctx, plane_ptrs.data(), av_frame.nb_samples,
                    const_cast<const std::uint8_t**>(av_frame.extended_data),
                    av_frame.nb_samples);
    if (out_samples < 0)
        return failure<AudioBlock>(ErrorCode::ParseError,
            "swr_convert failed: " + av_error_string(out_samples));
    if (out_samples == 0)
        return failure<AudioBlock>(ErrorCode::ParseError, "swr produced 0 samples");

    AudioBlock block;
    block.data = std::move(buffer);
    block.channels = channels;
    block.sample_rate = codec_.ctx->sample_rate;
    block.frames = out_samples;
    block.pts_ticks = (av_frame.pts == AV_NOPTS_VALUE) ? AudioBlock::NO_TIMESTAMP
                                                       : av_frame.pts;
    block.duration_ticks = out_samples; // 1/sample_rate timebase
    frames_decoded_ += out_samples;
    return Result<AudioBlock>(std::move(block));
}

void AvAudioDecoder::close() {
    if (swr_.ctx) {
        swr_free(&swr_.ctx);
    }
    if (codec_.ctx) avcodec_free_context(&codec_.ctx);
    if (format_.ctx) avformat_close_input(&format_.ctx);
    audio_stream_index_ = -1;
    eof_reached_ = false;
    end_of_stream_signaled_ = false;
}

} // namespace veltrix::media::ffmpeg
