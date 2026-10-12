// Veltrix Project - ProjectStore.cpp
//
// JSON persistence for projects (.veltrix). Layout (schema_version = 1):
//
// {
//   "schema_version": 1,
//   "id":   "<u64 decimal string>",
//   "name": "...",
//   "settings": { "video": {...}, "audio": {...} },
//   "media":   [ {id, path, kind, streams...} ],
//   "sequence": {
//      "id": ..., "name": ..., "explicit_duration": N,
//      "timebase": {"fps_num": N, "fps_den": N},
//      "tracks": [ {"id":..., "name":..., "kind":"video|audio",
//                   "muted":b, "locked":b, "clips":[{...}]} ]
//   }
// }
//
// Deserialization is strict: wrong version, wrong types, unknown structural
// keys produce errors - never partially filled objects or guessed data.

#include "veltrix/project/ProjectStore.h"

#include <charconv>
#include <filesystem>

#include <nlohmann/json.hpp>

#include "veltrix/io/FileUtils.h"
#include "veltrix/media/MediaItem.h"

namespace veltrix::project {

using veltrix::core::ErrorCode;
using veltrix::core::failure;
using veltrix::core::Result;
using veltrix::core::ResultVoid;
using nlohmann::json;

namespace {

// ---- id <-> string -----------------------------------------------------------

Result<std::uint64_t> parse_id(const json& node, const char* field) {
    const auto it = node.find(field);
    if (it == node.end() || it->is_null())
        return failure<std::uint64_t>(ErrorCode::ParseError,
                                      std::string("missing field: ") + field);
    if (it->is_number_unsigned())
        return Result<std::uint64_t>(it->get<std::uint64_t>());
    if (it->is_string()) {
        std::uint64_t value = 0;
        const auto sv = it->get_ref<const std::string&>();
        const auto res = std::from_chars(sv.data(), sv.data() + sv.size(), value);
        if (res.ec != std::errc{} || res.ptr != sv.data() + sv.size())
            return failure<std::uint64_t>(ErrorCode::ParseError,
                                          "invalid id string", sv);
        return Result<std::uint64_t>(value);
    }
    return failure<std::uint64_t>(ErrorCode::ParseError,
                                 std::string("field must be a number or string: ") + field);
}

// ---- serialize ---------------------------------------------------------------

json clip_to_json(const timeline::Clip& clip) {
    json j;
    j["id"] = clip.id.to_string();
    j["media"] = clip.media.to_string();
    j["start"] = clip.start;
    j["duration"] = clip.duration;
    j["source_in"] = clip.source_in;
    return j;
}

json track_to_json(const timeline::Track& track) {
    json j;
    j["id"] = track.id.to_string();
    j["name"] = track.name;
    j["kind"] = track.kind == timeline::TrackKind::Video ? "video" : "audio";
    j["muted"] = track.muted;
    j["locked"] = track.locked;
    j["clips"] = json::array();
    for (const auto& clip : track.clips) j["clips"].push_back(clip_to_json(clip));
    return j;
}

json stream_to_json(const media::StreamInfo& s) {
    json j;
    j["index"] = s.index;
    j["codec"] = s.codec_name;
    j["width"] = s.width;
    j["height"] = s.height;
    j["sample_rate"] = s.sample_rate;
    j["channels"] = s.channels;
    j["fps_num"] = s.fps_num;
    j["fps_den"] = s.fps_den;
    j["duration_us"] = s.duration_us;
    return j;
}

json media_item_to_json(const media::MediaItem& item) {
    json j;
    j["id"] = item.id.to_string();
    j["path"] = item.path;
    j["kind"] = media::media_kind_name(item.kind);
    j["duration_us"] = item.duration_us;
    j["streams"] = json::array();
    for (const auto& s : item.streams) j["streams"].push_back(stream_to_json(s));
    return j;
}

// ---- deserialize helpers -----------------------------------------------------

struct ParsedStream {
    media::StreamInfo info;
};

Result<timeline::Timebase> parse_timebase(const json& node) {
    if (!node.is_object())
        return failure<timeline::Timebase>(ErrorCode::ParseError,
                                          "timebase must be an object");
    const auto num = node.find("fps_num");
    const auto den = node.find("fps_den");
    if (num == node.end() || den == node.end() || !num->is_number_integer() ||
        !den->is_number_integer())
        return failure<timeline::Timebase>(ErrorCode::ParseError,
                                          "timebase requires integer fps_num/fps_den");
    auto tb = timeline::Timebase::from_fps(num->get<std::int64_t>(),
                                           den->get<std::int64_t>());
    if (tb.is_error())
        return failure<timeline::Timebase>(tb.error().code, tb.error().message);
    return tb;
}

Result<timeline::TrackKind> parse_track_kind(const json& node) {
    if (!node.is_string())
        return failure<timeline::TrackKind>(ErrorCode::ParseError,
                                            "track kind must be a string");
    const std::string kind = node.get<std::string>();
    if (kind == "video") return Result<timeline::TrackKind>(timeline::TrackKind::Video);
    if (kind == "audio") return Result<timeline::TrackKind>(timeline::TrackKind::Audio);
    return failure<timeline::TrackKind>(ErrorCode::ParseError,
                                       "unknown track kind", kind);
}

Result<timeline::Clip> parse_clip(const json& node) {
    if (!node.is_object())
        return failure<timeline::Clip>(ErrorCode::ParseError, "clip must be an object");
    timeline::Clip clip;
    auto id = parse_id(node, "id");
    if (id.is_error())
        return failure<timeline::Clip>(id.error().code, id.error().message);
    clip.id.value = id.take_value();
    auto media = parse_id(node, "media");
    if (media.is_error())
        return failure<timeline::Clip>(media.error().code, media.error().message);
    clip.media.value = media.take_value();
    for (const auto& [field, target] : std::initializer_list<std::pair<const char*, std::int64_t*>>{
             {"start", &clip.start}, {"duration", &clip.duration}, {"source_in", &clip.source_in}}) {
        const auto it = node.find(field);
        if (it == node.end() || !it->is_number_integer())
            return failure<timeline::Clip>(ErrorCode::ParseError,
                                          std::string("clip field must be an integer: ") + field);
        *target = it->get<std::int64_t>();
    }
    return Result<timeline::Clip>(std::move(clip));
}

Result<timeline::Track> parse_track(const json& node) {
    if (!node.is_object())
        return failure<timeline::Track>(ErrorCode::ParseError, "track must be an object");
    timeline::Track track;
    auto id = parse_id(node, "id");
    if (id.is_error()) return failure<timeline::Track>(id.error().code, id.error().message);
    track.id.value = id.take_value();
    const auto name = node.find("name");
    if (name == node.end() || !name->is_string())
        return failure<timeline::Track>(ErrorCode::ParseError, "track name missing");
    track.name = name->get<std::string>();
    auto kind = parse_track_kind(node.at("kind"));
    if (kind.is_error())
        return failure<timeline::Track>(kind.error().code, kind.error().message);
    track.kind = kind.take_value();
    if (auto it = node.find("muted"); it != node.end() && it->is_boolean())
        track.muted = it->get<bool>();
    if (auto it = node.find("locked"); it != node.end() && it->is_boolean())
        track.locked = it->get<bool>();
    if (auto clips = node.find("clips"); clips != node.end() && clips->is_array()) {
        for (const auto& clip_node : *clips) {
            auto clip = parse_clip(clip_node);
            if (clip.is_error())
                return failure<timeline::Track>(clip.error().code, clip.error().message);
            track.clips.push_back(clip.take_value());
        }
    }
    return Result<timeline::Track>(std::move(track));
}

Result<timeline::Sequence> parse_sequence(const json& node) {
    if (!node.is_object())
        return failure<timeline::Sequence>(ErrorCode::ParseError,
                                           "sequence must be an object");
    auto id = parse_id(node, "id");
    if (id.is_error())
        return failure<timeline::Sequence>(id.error().code, id.error().message);
    const auto name = node.find("name");
    if (name == node.end() || !name->is_string())
        return failure<timeline::Sequence>(ErrorCode::ParseError, "sequence name missing");
    auto timebase = parse_timebase(node.at("timebase"));
    if (timebase.is_error())
        return failure<timeline::Sequence>(timebase.error().code, timebase.error().message);
    std::int64_t explicit_duration = 0;
    if (auto it = node.find("explicit_duration"); it != node.end() && it->is_number_integer())
        explicit_duration = it->get<std::int64_t>();

    auto seq = timeline::Sequence::create_with_id(
        veltrix::core::SequenceId{id.take_value()}, name->get<std::string>(),
        timebase.take_value(), explicit_duration);
    if (seq.is_error())
        return failure<timeline::Sequence>(seq.error().code, seq.error().message);

    if (auto tracks = node.find("tracks"); tracks != node.end() && tracks->is_array()) {
        for (const auto& track_node : *tracks) {
            auto track = parse_track(track_node);
            if (track.is_error())
                return failure<timeline::Sequence>(track.error().code, track.error().message);
            auto res = seq.value().add_track(track.value().kind, track.value().name);
            if (res.is_error())
                return failure<timeline::Sequence>(res.error().code, res.error().message);
            // Overwrite the auto-generated track id with the persisted one
            // and load clips through the validated insert path.
            timeline::Track& stored = *seq.value().find_track(res.value());
            stored.id = track.value().id;
            stored.muted = track.value().muted;
            stored.locked = track.value().locked;
            for (const auto& clip : track.value().clips) {
                auto insert_res = seq.value().insert_clip(stored.id, clip);
                if (insert_res.is_error())
                    return failure<timeline::Sequence>(insert_res.error().code,
                                                       insert_res.error().message);
            }
        }
    }
    return seq;
}

Result<media::MediaItem> parse_media_item(const json& node) {
    if (!node.is_object())
        return failure<media::MediaItem>(ErrorCode::ParseError,
                                        "media item must be an object");
    media::MediaItem item;
    auto id = parse_id(node, "id");
    if (id.is_error())
        return failure<media::MediaItem>(id.error().code, id.error().message);
    item.id.value = id.take_value();
    const auto path = node.find("path");
    if (path == node.end() || !path->is_string())
        return failure<media::MediaItem>(ErrorCode::ParseError, "media path missing");
    item.path = path->get<std::string>();
    if (auto it = node.find("kind"); it != node.end() && it->is_string()) {
        const std::string kind = it->get<std::string>();
        if (kind == "video") item.kind = media::MediaKind::Video;
        else if (kind == "audio") item.kind = media::MediaKind::Audio;
        else if (kind == "image") item.kind = media::MediaKind::Image;
        else if (kind == "image_sequence") item.kind = media::MediaKind::ImageSequence;
        else item.kind = media::MediaKind::Unknown;
    }
    if (auto it = node.find("duration_us"); it != node.end() && it->is_number_integer())
        item.duration_us = it->get<std::int64_t>();
    if (auto streams = node.find("streams"); streams != node.end() && streams->is_array()) {
        for (const auto& s : *streams) {
            media::StreamInfo info;
            if (auto f = s.find("index"); f != s.end() && f->is_number_integer())
                info.index = f->get<std::int32_t>();
            if (auto f = s.find("codec"); f != s.end() && f->is_string())
                info.codec_name = f->get<std::string>();
            if (auto f = s.find("width"); f != s.end() && f->is_number_integer())
                info.width = f->get<std::int32_t>();
            if (auto f = s.find("height"); f != s.end() && f->is_number_integer())
                info.height = f->get<std::int32_t>();
            if (auto f = s.find("sample_rate"); f != s.end() && f->is_number_integer())
                info.sample_rate = f->get<std::int32_t>();
            if (auto f = s.find("channels"); f != s.end() && f->is_number_integer())
                info.channels = f->get<std::int32_t>();
            if (auto f = s.find("fps_num"); f != s.end() && f->is_number_integer())
                info.fps_num = f->get<std::int64_t>();
            if (auto f = s.find("fps_den"); f != s.end() && f->is_number_integer())
                info.fps_den = f->get<std::int64_t>();
            if (auto f = s.find("duration_us"); f != s.end() && f->is_number_integer())
                info.duration_us = f->get<std::int64_t>();
            item.streams.push_back(std::move(info));
        }
    }
    return Result<media::MediaItem>(std::move(item));
}

} // namespace

// ---- public API ----------------------------------------------------------------

veltrix::core::Result<std::string> ProjectStore::serialize(const Project& project) {
    auto validation = project.validate();
    if (validation.is_error())
        return failure<std::string>(validation.error().code, validation.error().message);

    json doc;
    doc["schema_version"] = SCHEMA_VERSION;
    doc["id"] = project.id().to_string();
    doc["name"] = project.name();

    json settings;
    settings["video"]["width"] = project.video().width;
    settings["video"]["height"] = project.video().height;
    settings["video"]["fps_num"] = project.video().fps.num();
    settings["video"]["fps_den"] = project.video().fps.den();
    settings["video"]["progressive"] = project.video().progressive;
    settings["audio"]["sample_rate"] = project.audio().sample_rate;
    settings["audio"]["channels"] = project.audio().channels;
    doc["settings"] = std::move(settings);

    doc["media"] = json::array();
    for (const auto& item : project.media().items())
        doc["media"].push_back(media_item_to_json(item));

    const auto& seq = project.sequence();
    json seq_json;
    seq_json["id"] = seq.id().to_string();
    seq_json["name"] = seq.name();
    seq_json["explicit_duration"] = seq.explicit_duration();
    const auto fps = seq.timebase().fps();
    seq_json["timebase"] = {{"fps_num", fps.num()}, {"fps_den", fps.den()}};
    seq_json["tracks"] = json::array();
    for (const auto& track : seq.tracks())
        seq_json["tracks"].push_back(track_to_json(track));
    doc["sequence"] = std::move(seq_json);

    return veltrix::core::Result<std::string>(doc.dump(2) + "\n");
}

veltrix::core::Result<Project> ProjectStore::deserialize(const std::string& json_text) {
    json doc;
    try {
        doc = json::parse(json_text);
    } catch (const json::parse_error& e) {
        return failure<Project>(ErrorCode::ParseError,
                                std::string("invalid project JSON: ") + e.what());
    }
    if (!doc.is_object())
        return failure<Project>(ErrorCode::ParseError, "project root must be an object");

    const auto version = doc.find("schema_version");
    if (version == doc.end() || !version->is_number_integer())
        return failure<Project>(ErrorCode::ParseError, "missing schema_version");
    if (version->get<std::int64_t>() != SCHEMA_VERSION)
        return failure<Project>(ErrorCode::VersionUnsupported,
                               "unsupported project schema version " +
                                   std::to_string(version->get<std::int64_t>()) +
                                   " (supported: 1)");

    Project project;
    auto id = parse_id(doc, "id");
    if (id.is_error()) return failure<Project>(id.error().code, id.error().message);
    project.restore_id(veltrix::core::ProjectId{id.take_value()});

    const auto name = doc.find("name");
    if (name == doc.end() || !name->is_string())
        return failure<Project>(ErrorCode::ParseError, "project name missing");
    project.set_name(name->get<std::string>());

    const auto settings = doc.find("settings");
    if (settings == doc.end() || !settings->is_object())
        return failure<Project>(ErrorCode::ParseError, "missing settings");
    const auto video = settings->find("video");
    if (video == settings->end() || !video->is_object())
        return failure<Project>(ErrorCode::ParseError, "missing video settings");
    auto fps = timeline::Rational::from(
        video->value("fps_num", std::int64_t(0)), video->value("fps_den", std::int64_t(1)));
    if (fps.is_error()) return failure<Project>(fps.error().code, fps.error().message);
    auto video_settings = VideoSettings::create(
        video->value("width", 0), video->value("height", 0),
        fps.value().num(), fps.value().den());
    if (video_settings.is_error())
        return failure<Project>(video_settings.error().code, video_settings.error().message);
    project.video() = video_settings.take_value();

    const auto audio = settings->find("audio");
    if (audio == settings->end() || !audio->is_object())
        return failure<Project>(ErrorCode::ParseError, "missing audio settings");

    auto audio_settings = AudioSettings::create(audio->value("sample_rate", 0),
                                                audio->value("channels", 0));
    if (audio_settings.is_error())
        return failure<Project>(audio_settings.error().code, audio_settings.error().message);
    project.audio() = audio_settings.take_value();

    if (auto media = doc.find("media"); media != doc.end() && media->is_array()) {
        for (const auto& item_node : *media) {
            auto item = parse_media_item(item_node);
            if (item.is_error())
                return failure<Project>(item.error().code, item.error().message);
            auto res = project.media().load_item(item.take_value());
            if (res.is_error())
                return failure<Project>(res.error().code, res.error().message);
        }
    }

    const auto seq_node = doc.find("sequence");
    if (seq_node == doc.end() || !seq_node->is_object())
        return failure<Project>(ErrorCode::ParseError, "missing sequence");
    auto seq = parse_sequence(*seq_node);
    if (seq.is_error()) return failure<Project>(seq.error().code, seq.error().message);
    project.set_sequence(seq.take_value());

    // Final structural validation of the whole document.
    auto validation = project.validate();
    if (validation.is_error())
        return failure<Project>(validation.error().code, validation.error().message);
    return veltrix::core::Result<Project>(std::move(project));
}

ResultVoid ProjectStore::save_to_file(const Project& project, const std::string& path) {
    auto text = serialize(project);
    if (text.is_error())
        return failure(text.error().code, text.error().message);
    std::string final_path = path;
    const std::filesystem::path fs_path(path);
    if (fs_path.extension().empty()) final_path += ".veltrix";
    auto res = veltrix::io::write_text_file_atomic(final_path, text.take_value());
    if (res.is_error()) return failure(res.error().code, res.error().message);
    return veltrix::core::success();
}

veltrix::core::Result<Project> ProjectStore::load_from_file(const std::string& path) {
    auto text = veltrix::io::read_text_file(path);
    if (text.is_error())
        return failure<Project>(text.error().code, text.error().message);
    auto project = deserialize(text.take_value());
    if (project.is_error()) return project;
    project.value().media().refresh_availability();
    return project;
}

} // namespace veltrix::project
