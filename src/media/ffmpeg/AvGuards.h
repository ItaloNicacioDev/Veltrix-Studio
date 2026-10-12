// Veltrix FFmpeg layer - internal RAII guards. NOT part of the public API:
// nothing in include/ may see these. Every AV* resource is owned by one
// of these guards — leak-free by construction, including on error paths.

#pragma once

#include <string>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavcodec/codec.h>
#include <libavformat/avformat.h>
#include <libswresample/swresample.h>
#include <libavutil/imgutils.h>
#include <libavutil/opt.h>
#include <libavutil/pixdesc.h>
}

namespace veltrix::media::ffmpeg {

// av_err2str cannot be used directly (compound literal in C++); format manually.
inline std::string av_error_string(int code) {
    char buffer[AV_ERROR_MAX_STRING_SIZE] = {0};
    av_strerror(code, buffer, sizeof(buffer));
    return buffer;
}

struct FormatCtxGuard {
    AVFormatContext* ctx = nullptr;
    ~FormatCtxGuard() {
        if (ctx) avformat_close_input(&ctx);
    }
};

struct CodecCtxGuard {
    AVCodecContext* ctx = nullptr;
    ~CodecCtxGuard() {
        if (ctx) avcodec_free_context(&ctx);
    }
};

struct FrameGuard {
    AVFrame* frame = nullptr;
    FrameGuard() : frame(av_frame_alloc()) {}
    ~FrameGuard() {
        if (frame) av_frame_free(&frame);
    }
};

struct PacketGuard {
    AVPacket* packet = nullptr;
    PacketGuard() : packet(av_packet_alloc()) {}
    ~PacketGuard() {
        if (packet) av_packet_free(&packet);
    }
};

struct SwsGuard {
    SwsContext* ctx = nullptr;
    ~SwsGuard() {
        if (ctx) sws_freeContext(ctx);
    }
};

struct SwrGuard {
    SwrContext* ctx = nullptr;
    ~SwrGuard() {
        if (ctx) swr_free(&ctx);
    }
};

} // namespace veltrix::media::ffmpeg
