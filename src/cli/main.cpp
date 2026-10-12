// veltrix-cli — diagnostic tool for the Veltrix core (NOT the product UI).
//
// Subcommands:
//   info                                 build/deps/plugin-api report
//   new  <file.veltrix> [name] [media]   create + save a project; when a
//                                         media file is given it is
//                                         registered, probed and placed on
//                                         a demo video track
//   check <file.veltrix>                 load + validate a project file
//   probe <media-file> [ffprobe-path]    ffprobe-based technical analysis
//
// Exit codes: 0 ok; 1 core error; 2 usage error.

#include <iostream>
#include <string>

#include "veltrix/core/Result.h"
#include "veltrix/diagnostics/Diagnostics.h"
#include "veltrix/io/FileUtils.h"
#include "veltrix/media/MediaItem.h"
#include "veltrix/media/MediaProber.h"
#include "veltrix/project/Project.h"
#include "veltrix/project/ProjectStore.h"
#include "veltrix/timeline/Sequence.h"
#include "veltrix/timeline/TimelineEditor.h"

#ifdef VELTRIX_HAS_FFMPEG
#include "veltrix/playback/Decoder.h"
namespace veltrix::cli {
std::unique_ptr<playback::IDecoderFactory> make_decoder_factory();
}
#endif

namespace {

int fail(const veltrix::core::Error& error) {
    std::cerr << "error: " << error.to_string() << "\n";
    return 1;
}

int cmd_info() {
    std::cout << veltrix::diagnostics::build_report();
    return 0;
}

int cmd_new(const std::string& path, const std::string& name,
            const std::string& media_path) {
    using namespace veltrix;
    auto project = project::Project::create(name.empty() ? "Demo Project" : name);

    if (!media_path.empty()) {
        // Register + probe the media, then place it on a demo video track.
        auto media = project.media().register_media(io::normalize_path(media_path));
        if (media.is_error()) return fail(media.error());
        auto located = media::MediaProber::locate_ffprobe("");
        if (located.is_error()) return fail(located.error());
        auto* item = project.media().find(media.value());
        auto probed = media::MediaProber::probe(*item, located.value(), 30);
        if (probed.is_error()) return fail(probed.error());

        timeline::TimelineEditor editor(project.sequence());
        auto v1 = editor.add_track(timeline::TrackKind::Video, "V1");
        if (v1.is_error()) return fail(v1.error());

        // Clip length: media duration mapped to sequence ticks, with a
        // sane 3-second default when the duration is unknown.
        std::int64_t duration = 90; // 3 s @ 30 fps project default
        auto ticks = project.sequence().timebase().ticks_for_double_seconds(
            static_cast<double>(item->duration_us) / 1.0e6);
        if (ticks.is_ok() && ticks.value() > 0) duration = ticks.value();
        auto clip = editor.insert_clip(v1.value(), media.value(), 0, duration, 0);
        if (clip.is_error()) return fail(clip.error());
        project.set_sequence(editor.take_sequence());
    }

    auto validation = project.validate();
    if (validation.is_error()) return fail(validation.error());
    auto save = project::ProjectStore::save_to_file(project, path);
    if (save.is_error()) return fail(save.error());
    std::cout << "created: " << (path.ends_with(".veltrix") ? path : path + ".veltrix")
              << "\n";
    std::cout << "  project id: " << project.id().to_string() << "\n";
    std::cout << "  schema:     " << project::ProjectStore::SCHEMA_VERSION << "\n";
    return 0;
}

int cmd_check(const std::string& path) {
    auto project = veltrix::project::ProjectStore::load_from_file(path);
    if (project.is_error()) return fail(project.error());
    const auto& p = project.value();
    std::cout << "valid project\n";
    std::cout << "  name:      " << p.name() << "\n";
    std::cout << "  id:        " << p.id().to_string() << "\n";
    std::cout << "  video:     " << p.video().width << "x" << p.video().height
              << " @ " << p.video().fps.to_string() << " fps\n";
    std::cout << "  audio:     " << p.audio().sample_rate << " Hz, "
              << p.audio().channels << " ch\n";
    std::cout << "  media:     " << p.media().size() << " items\n";
    std::cout << "  tracks:    " << p.sequence().tracks().size() << "\n";
    const std::size_t clips = [&] {
        std::size_t n = 0;
        for (const auto& t : p.sequence().tracks()) n += t.clips.size();
        return n;
    }();
    std::cout << "  clips:     " << clips << "\n";
    return 0;
}

int cmd_probe(const std::string& media_path, const std::string& ffprobe_override) {
    veltrix::media::MediaItem item;
    item.path = media_path;
    auto located = veltrix::media::MediaProber::locate_ffprobe(ffprobe_override);
    if (located.is_error()) return fail(located.error());
    std::cout << "ffprobe: " << located.value() << "\n";
    auto probed = veltrix::media::MediaProber::probe(item, located.value(), 30);
    if (probed.is_error()) return fail(probed.error());
    std::cout << "file:    " << item.path << "\n";
    std::cout << "kind:    " << veltrix::media::media_kind_name(item.kind) << "\n";
    std::cout << "streams: " << item.streams.size() << "\n";
    for (const auto& s : item.streams) {
        std::cout << "  [" << s.index << "] " << s.codec_name;
        if (s.width > 0) std::cout << " " << s.width << "x" << s.height;
        if (s.fps_den > 0) std::cout << " " << s.fps_num << "/" << s.fps_den << " fps";
        if (s.sample_rate > 0) std::cout << " " << s.sample_rate << " Hz";
        if (s.channels > 0) std::cout << " " << s.channels << "ch";
        std::cout << "\n";
    }
    std::cout << "duration: " << (item.duration_us / 1000) << " ms\n";
    return 0;
}

} // namespace

#ifdef VELTRIX_HAS_FFMPEG
#include <memory>
#include "veltrix/playback/Decoder.h"
namespace veltrix::cli {
// Implemented in cli/DecodeCommands.cpp (compiled only with veltrix-ffmpeg).
int cmd_decode(const std::string& path, int max_frames, double seek_seconds);
}
#endif

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "usage: veltrix-cli <info|new|check|probe> [args]\n"
                  << "  info\n"
                  << "  new <file.veltrix> [name] [media-file]\n"
                  << "  check <file.veltrix>\n"
                  << "  probe <media-file> [ffprobe-path]\n";
        return 2;
    }
    const std::string command = argv[1];
    if (command == "info")
        return cmd_info();
    if (command == "new") {
        if (argc < 3) { std::cerr << "error: new requires a file path\n"; return 2; }
        // new <file> [name] [media] — name has no extension; media has one.
        std::string name, media;
        if (argc >= 4) {
            const std::string first = argv[3];
            if (first.find('.') == std::string::npos) name = first;
            else media = first;
        }
        if (argc >= 5) media = argv[4];
        return cmd_new(argv[2], name, media);
    }
    if (command == "check") {
        if (argc < 3) { std::cerr << "error: check requires a file path\n"; return 2; }
        return cmd_check(argv[2]);
    }
    if (command == "probe") {
        if (argc < 3) { std::cerr << "error: probe requires a media path\n"; return 2; }
        return cmd_probe(argv[2], argc > 3 ? argv[3] : "");
    }
#ifdef VELTRIX_HAS_FFMPEG
    if (command == "decode") {
        if (argc < 3) { std::cerr << "error: decode requires a media path\n"; return 2; }
        const int max_frames = argc > 3 ? std::atoi(argv[3]) : 10;
        const double seek_s = argc > 4 ? std::atof(argv[4]) : 0.0;
        return veltrix::cli::cmd_decode(argv[2], max_frames, seek_s);
    }
#else
    if (command == "decode") {
        std::cerr << "error: this build has no decode engine "
                     "(configure with an FFmpeg SDK: VELTRIX_FFMPEG_DIR)\n";
        return 1;
    }
#endif
    std::cerr << "error: unknown command '" << command << "'\n";
    return 2;
}
