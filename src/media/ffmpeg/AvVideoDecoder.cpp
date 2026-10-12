// Veltrix FFmpeg layer - AvVideoDecoder.cpp
//
// Real video decoding via libav (ADR-008). Key behaviors:
//   * open(): avformat_open_input + find_stream_info + best video stream
//     + codec open + sws setup (source pix fmt -> packed RGBA8).
//   * seek(): avformat_seek_file with the stream timebase; Exact flushes
//     and decodes forward to the target; Keyframe accepts the previous
//     keyframe landing. av_read_frame errors map to explicit Results.
//   * read_frame(): demux/decode loop with token polls between packets
//     and between decodes; missing PTS is surfaced as NO_TIMESTAMP
//     (never guessed); end of stream = failure(NotFound, "end of stream").
//   * Frames are converted with sws_scale into a packed RGBA8 buffer
//     (stride == width*4). Buffer ownership: shared_ptr<FrameBuffer>.
//   * Every resource is behind a guard: partial-open failures leak
//     nothing (verified by repeated open/close tests + AV leaks checked
//     in the benchmark harness with av_log quiet).

#include "AvVideoDecoder.h"

#include <cerrno>
#include <cstring>

namespace veltrix::media::ffmpeg {

using veltrix::core::ErrorCode;
using veltrix::core::failure;
using veltrix::core::Result;
using veltrix::core::ResultVoid;
using veltrix::playback::MediaStreamInfo;
using veltrix::playback::PixelFormat;
using veltrix::playback::SeekMode;
using veltrix::playback::VideoFrame;

ResultVoid AvVideoDecoder::open(const std::string& path) {
    if (is_open())
        return failure(ErrorCode::Conflict, "decoder already open");

    const AVInputFormat* forced_format = nullptr;
    const int open_res = avformat_open_input(&format_.ctx, path.c_str(),
                                             forced_format, nullptr);
    if (open_res != 0) {
        format_.ctx = nullptr; // avformat frees on failure
        if (open_res == AVERROR(ENOENT))
            return failure(ErrorCode::NotFound, "media file not found", path);
        if (open_res == AVERROR_INVALIDDATA)
            return failure(ErrorCode::ParseError, "invalid or empty media file", path);
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

    return setup_video_pipeline();
}

ResultVoid AvVideoDecoder::setup_video_pipeline() {
    const int stream_index = av_find_best_stream(format_.ctx, AVMEDIA_TYPE_VIDEO,
                                                  -1, -1, nullptr, 0);
    if (stream_index < 0) {
        const int err = stream_index;
        close();
        if (err == AVERROR_STREAM_NOT_FOUND || err == AVERROR_DECODER_NOT_FOUND)
            return failure(ErrorCode::Unsupported,
                           "no decodable video stream in file");
        return failure(ErrorCode::ParseError,
                       "video stream discovery failed: " + av_error_string(err));
    }
    video_stream_index_ = stream_index;
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
    if (const int cp_res = avcodec_parameters_to_context(codec_.ctx, stream->codecpar);
        cp_res < 0) {
        close();
        return failure(ErrorCode::ParseError,
                       "avcodec_parameters_to_context failed: " + av_error_string(cp_res));
    }
    // Multimedia timing comes from the STREAM timebase; the codec context
    // inherits it so pts values match the stream.
    codec_.ctx->pkt_timebase = stream->time_base;
    if (const int open_res = avcodec_open2(codec_.ctx, codec, nullptr); open_res < 0) {
        close();
        return failure(ErrorCode::Unsupported,
                       "avcodec_open2 failed: " + av_error_string(open_res),
                       codec->name);
    }

    // ---- stream info (public, AV-free) --------------------------------------
    info_ = MediaStreamInfo{};
    info_.has_video = true;
    info_.video_width = codec_.ctx->width ? codec_.ctx->width : stream->codecpar->width;
    info_.video_height = codec_.ctx->height ? codec_.ctx->height : stream->codecpar->height;
    if (stream->avg_frame_rate.den != 0 && stream->avg_frame_rate.num != 0) {
        info_.video_fps_num = stream->avg_frame_rate.num;
        info_.video_fps_den = stream->avg_frame_rate.den;
    }
    info_.video_timebase_num = stream->time_base.num;
    info_.video_timebase_den = stream->time_base.den;
    // Audio presence is reported too (stream_info is shared by both
    // decoders' open(); harmless here).
    const int audio_index = av_find_best_stream(format_.ctx, AVMEDIA_TYPE_AUDIO,
                                                -1, -1, nullptr, 0);
    info_.has_audio = audio_index >= 0;
    if (info_.has_audio) {
        AVStream* audio = format_.ctx->streams[audio_index];
        info_.audio_channels = audio->codecpar->ch_layout.nb_channels;
        info_.audio_sample_rate = audio->codecpar->sample_rate;
        info_.audio_timebase_num = audio->time_base.num;
        info_.audio_timebase_den = audio->time_base.den;
    }
    if (format_.ctx->duration != AV_NOPTS_VALUE) {
        info_.duration_ticks = format_.ctx->duration; // in AV_TIME_BASE (us)
        info_.duration_timebase_num = 1;
        info_.duration_timebase_den = AV_TIME_BASE;
    }
    eof_reached_ = false;
    end_of_stream_signaled_ = false;
    return veltrix::core::success();
}

ResultVoid AvVideoDecoder::seek(std::int64_t target_ticks, SeekMode mode) {
    if (!is_open())
        return failure(ErrorCode::Conflict, "decoder not open");
    if (target_ticks < 0)
        return failure(ErrorCode::InvalidArgument, "seek target must be >= 0");

    AVStream* stream = format_.ctx->streams[video_stream_index_];

    // Position is given in the video stream timebase already. For Exact
    // we pass the target: libavformat lands at-or-before it and we decode
    // forward. For Keyframe we also pass the target; the demuxer itself
    // lands on the preceding keyframe (backward seek flags = 0 ts range).
    const std::int64_t target = target_ticks;
    if (mode == SeekMode::Exact && info_.duration_ticks > 0) {
        const std::int64_t duration_stream_ticks = av_rescale_q(
            info_.duration_ticks, AVRational{1, AV_TIME_BASE}, stream->time_base);
        if (target > duration_stream_ticks)
            return failure(ErrorCode::OutOfRange, "seek target beyond end");
    }

    const int res = avformat_seek_file(format_.ctx, video_stream_index_,
                                       INT64_MIN, target, INT64_MAX, 0);
    if (res < 0) {
        // Some demuxers return AVERROR_EOF for seeks past the end.
        if (res == AVERROR_EOF && mode == SeekMode::Exact && info_.duration_ticks > 0)
            return failure(ErrorCode::OutOfRange, "seek target beyond end");
        return failure(ErrorCode::ParseError,
                       "avformat_seek_file failed: " + av_error_string(res));
    }

    avcodec_flush_buffers(codec_.ctx);
    eof_reached_ = false;
    end_of_stream_signaled_ = false;
    return veltrix::core::success();
}

Result<VideoFrame> AvVideoDecoder::read_frame(
    const veltrix::core::CancellationToken& token) {
    if (!is_open())
        return failure<VideoFrame>(ErrorCode::Conflict, "decoder not open");

    // Outer loop: feed packets until one frame comes out or EOS.
    for (;;) {
        if (token.is_cancelled())
            return failure<VideoFrame>(ErrorCode::Cancelled, "cancelled by token");

        if (eof_reached_) {
            // Drain the codec: frames may still be buffered.
            const int send_res = avcodec_send_packet(codec_.ctx, nullptr);
            if (send_res < 0 && send_res != AVERROR_EOF)
                return failure<VideoFrame>(ErrorCode::ParseError,
                    "drain send_packet failed: " + av_error_string(send_res));
            const int recv_res = avcodec_receive_frame(codec_.ctx, frame_.frame);
            if (recv_res == 0) {
                ++frames_decoded_;
                return convert_to_rgba(*frame_.frame);
            }
            if (recv_res == AVERROR_EOF || recv_res == AVERROR(EAGAIN)) {
                // Fully drained: signal EOS exactly once.
                if (end_of_stream_signaled_)
                    return failure<VideoFrame>(ErrorCode::Conflict,
                                              "end of stream already signaled");
                end_of_stream_signaled_ = true;
                return failure<VideoFrame>(ErrorCode::NotFound, "end of stream");
            }
            return failure<VideoFrame>(ErrorCode::ParseError,
                "receive_frame failed: " + av_error_string(recv_res));
        }

        const int read_res = av_read_frame(format_.ctx, packet_.packet);
        if (read_res < 0) {
            if (read_res == AVERROR_EOF) {
                eof_reached_ = true; // enter drain path on next iteration
                continue;
            }
            if (read_res == AVERROR_EXIT)
                return failure<VideoFrame>(ErrorCode::Cancelled, "read interrupted");
            return failure<VideoFrame>(ErrorCode::ParseError,
                "av_read_frame failed: " + av_error_string(read_res));
        }

        // Non-video packets are skipped (audio has its own decoder).
        if (packet_.packet->stream_index != video_stream_index_) {
            av_packet_unref(packet_.packet);
            continue;
        }

        const int send_res = avcodec_send_packet(codec_.ctx, packet_.packet);
        av_packet_unref(packet_.packet);
        if (send_res < 0 && send_res != AVERROR(EAGAIN)) {
            if (send_res == AVERROR_INVALIDDATA)
                return failure<VideoFrame>(ErrorCode::ParseError,
                                           "corrupt packet (invalid data)");
            return failure<VideoFrame>(ErrorCode::ParseError,
                "send_packet failed: " + av_error_string(send_res));
        }

        const int recv_res = avcodec_receive_frame(codec_.ctx, frame_.frame);
        if (recv_res == 0) {
            ++frames_decoded_;
            return convert_to_rgba(*frame_.frame);
        }
        if (recv_res == AVERROR(EAGAIN))
            continue; // need more packets
        if (recv_res == AVERROR_EOF) {
            eof_reached_ = true;
            continue; // drain path
        }
        return failure<VideoFrame>(ErrorCode::ParseError,
            "receive_frame failed: " + av_error_string(recv_res));
    }
}

Result<VideoFrame> AvVideoDecoder::convert_to_rgba(const AVFrame& av_frame) {
    const std::int32_t width = av_frame.width;
    const std::int32_t height = av_frame.height;
    if (width <= 0 || height <= 0)
        return failure<VideoFrame>(ErrorCode::ParseError, "frame has invalid size");

    // Rebuild sws when the source format/size changes (mid-stream size
    // changes are legal in some containers).
    const AVPixelFormat src_format = static_cast<AVPixelFormat>(av_frame.format);
    if (!sws_.ctx || sws_src_width_ != width || sws_src_height_ != height ||
        sws_src_format_ != src_format) {
        sws_.ctx = sws_getContext(width, height, src_format,
                                  width, height, AV_PIX_FMT_RGBA,
                                  SWS_BILINEAR, nullptr, nullptr, nullptr);
        if (!sws_.ctx)
            return failure<VideoFrame>(ErrorCode::Unsupported,
                                       "cannot convert pixel format",
                                       std::to_string(static_cast<int>(src_format)));
        sws_src_width_ = width;
        sws_src_height_ = height;
        sws_src_format_ = src_format;
    }

    auto buffer = std::make_shared<veltrix::playback::FrameBuffer>();
    const std::size_t bytes = static_cast<std::size_t>(width) * height * 4;
    buffer->bytes.resize(bytes);

    // Destination: packed RGBA, stride = width*4, single plane.
    std::uint8_t* dst = buffer->bytes.data();
    const int dst_stride = width * 4;
    sws_scale(sws_.ctx, av_frame.data, av_frame.linesize, 0, height,
              &dst, &dst_stride);

    VideoFrame frame;
    frame.data = std::move(buffer);
    frame.width = width;
    frame.height = height;
    frame.format = PixelFormat::R8G8B8A8_Unorm;
    frame.pts_ticks = (av_frame.pts == AV_NOPTS_VALUE)
                          ? VideoFrame::NO_TIMESTAMP
                          : av_frame.pts;
    frame.duration_ticks = av_frame.duration > 0 ? av_frame.duration : 0;
    frame.keyframe = (av_frame.flags & AV_FRAME_FLAG_KEY) != 0;
    return Result<VideoFrame>(std::move(frame));
}

void AvVideoDecoder::close() {
    if (sws_.ctx) {
        sws_freeContext(sws_.ctx);
        sws_.ctx = nullptr;
    }
    sws_src_width_ = sws_src_height_ = 0;
    sws_src_format_ = AV_PIX_FMT_NONE;
    if (codec_.ctx) avcodec_free_context(&codec_.ctx);
    if (format_.ctx) avformat_close_input(&format_.ctx);
    video_stream_index_ = -1;
    eof_reached_ = false;
    end_of_stream_signaled_ = false;
}

} // namespace veltrix::media::ffmpeg
