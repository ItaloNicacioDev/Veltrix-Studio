// tests - FrameRequestor: media-id -> frame pipeline (Fase 2 item 6).
// Covers: stale-request rejection, cancellation, resolver errors,
// real decode through the request path (when ffmpeg fixtures exist).

#include <catch2/catch.hpp>

#include <filesystem>
#include <thread>

#include "veltrix/core/CancellationToken.h"
#include "veltrix/media/MediaPathResolver.h"
#include "veltrix/media/MediaRegistry.h"
#include "veltrix/playback/FrameRequestor.h"

#include "media/ffmpeg/AvDecoderFactory.h"

using namespace veltrix;
using veltrix::media::ffmpeg::AvDecoderFactory;
using veltrix::playback::FrameRequestor;
using veltrix::playback::VideoFrame;

namespace {
// Reuses the same synthetic fixtures as the decode tests.
std::filesystem::path fixture_av() {
    const auto dir = std::filesystem::temp_directory_path() / "veltrix-decode-tests";
    const auto file = dir / "av_3s.mp4";
    std::error_code ec;
    return std::filesystem::exists(file, ec) ? file : std::filesystem::path{};
}
} // namespace

TEST_CASE("FrameRequestor: stale request is rejected without decoding",
          "[requestor][ffmpeg]") {
    AvDecoderFactory factory;
    media::MediaRegistry registry;
    media::MediaPathResolver resolver(registry);

    FrameRequestor requestor(factory, resolver);
    const auto media = core::MediaId{12345}; // not registered

    auto first = requestor.request_frame(media, 0);
    auto second = requestor.request_frame(media, 10); // supersedes

    auto stale = requestor.await_frame(first);
    REQUIRE(stale.is_error());
    REQUIRE(stale.error().code == core::ErrorCode::Conflict);
    REQUIRE(stale.error().message.find("superseded") != std::string::npos);
    REQUIRE(requestor.stale_rejects() == 1);

    // The superseding request still runs (and fails on the unregistered
    // media id — resolver error, not crash).
    auto fresh = requestor.await_frame(second);
    REQUIRE(fresh.is_error());
    REQUIRE(fresh.error().code == core::ErrorCode::NotFound);
}

TEST_CASE("FrameRequestor: cancellation before decode", "[requestor][ffmpeg]") {
    AvDecoderFactory factory;
    media::MediaRegistry registry;
    media::MediaPathResolver resolver(registry);
    FrameRequestor requestor(factory, resolver);

    auto handle = requestor.request_frame(core::MediaId{1}, 0);
    handle.token->cancel();
    auto result = requestor.await_frame(handle);
    REQUIRE(result.is_error());
    REQUIRE(result.error().code == core::ErrorCode::Cancelled);
}

TEST_CASE("FrameRequestor: new request cancels the previous token",
          "[requestor][ffmpeg]") {
    AvDecoderFactory factory;
    media::MediaRegistry registry;
    media::MediaPathResolver resolver(registry);
    FrameRequestor requestor(factory, resolver);

    auto first = requestor.request_frame(core::MediaId{1}, 0);
    auto second = requestor.request_frame(core::MediaId{1}, 5);
    REQUIRE(first.token->is_cancelled());  // auto-cancelled by supersession
    REQUIRE_FALSE(second.token->is_cancelled());
    REQUIRE(requestor.total_requests() == 2);
}

TEST_CASE("FrameRequestor: missing media file reports MediaUnavailable",
          "[requestor][ffmpeg]") {
    AvDecoderFactory factory;
    media::MediaRegistry registry;
    media::MediaPathResolver resolver(registry);
    FrameRequestor requestor(factory, resolver);

    media::MediaItem item;
    item.id.value = 7;
    item.path = "Z:/nowhere/gone.mp4";
    REQUIRE(registry.load_item(std::move(item)).is_ok());
    registry.refresh_availability(); // flags it missing

    auto handle = requestor.request_frame(core::MediaId{7}, 0);
    auto result = requestor.await_frame(handle);
    REQUIRE(result.is_error());
    REQUIRE(result.error().code == core::ErrorCode::MediaUnavailable);
}

TEST_CASE("FrameRequestor: real decode through the request path",
          "[requestor][ffmpeg][integration]") {
    const auto file = fixture_av();
    if (file.empty()) {
        WARN("SKIP: fixture not generated (ffmpeg binary missing)");
        return;
    }
    AvDecoderFactory factory;
    media::MediaRegistry registry;
    auto media = registry.register_media(file.string());
    REQUIRE(media.is_ok());
    media::MediaPathResolver resolver(registry);
    FrameRequestor requestor(factory, resolver);

    // Frame 0.
    auto h0 = requestor.request_frame(media.value(), 0);
    auto f0 = requestor.await_frame(h0);
    REQUIRE(f0.is_ok());
    REQUIRE(f0.value().width == 320);
    REQUIRE(f0.value().format == veltrix::playback::PixelFormat::R8G8B8A8_Unorm);
    REQUIRE(f0.value().pts_ticks >= 0);

    // Mid-file frame (1.5 s @ 30fps: stream tb is 1/15360 -> 1.5s = 23040).
    const std::int64_t mid_ticks = 23040;
    auto h1 = requestor.request_frame(media.value(), mid_ticks);
    auto f1 = requestor.await_frame(h1);
    REQUIRE(f1.is_ok());
    REQUIRE(f1.value().pts_ticks >= mid_ticks);
    REQUIRE(f1.value().pts_ticks < mid_ticks + 2000); // within ~1/8 s

    // Superseding while "in flight": the old handle yields Conflict.
    auto old = requestor.request_frame(media.value(), 0);
    auto h2 = requestor.request_frame(media.value(), 500);
    auto old_result = requestor.await_frame(old);
    REQUIRE(old_result.is_error());
    REQUIRE(old_result.error().code == core::ErrorCode::Conflict);

    auto f2 = requestor.await_frame(h2);
    REQUIRE(f2.is_ok());
}
