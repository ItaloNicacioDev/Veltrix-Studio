// Unit tests - project persistence (serialize/deserialize/validate).

#include <catch2/catch.hpp>

#include <filesystem>

#include "veltrix/project/Project.h"
#include "veltrix/project/ProjectStore.h"

using namespace veltrix;

TEST_CASE("Project create + validate", "[project]") {
    auto p = project::Project::create_validated("Test", 1920, 1080, 30000, 1001);
    REQUIRE(p.is_ok());
    REQUIRE(p.value().validate().is_ok());
    REQUIRE(p.value().video().fps.num() == 30000);
    REQUIRE(p.value().video().fps.den() == 1001);
}

TEST_CASE("Video settings validation", "[project]") {
    REQUIRE(project::VideoSettings::create(0, 1080, 30, 1).is_error());
    REQUIRE(project::VideoSettings::create(1920, -5, 30, 1).is_error());
    REQUIRE(project::VideoSettings::create(1920, 1080, 0, 1).is_error());
    REQUIRE(project::VideoSettings::create(1920, 1080, 30, 0).is_error());
    REQUIRE(project::AudioSettings::create(0, 2).is_error());
    REQUIRE(project::AudioSettings::create(48000, 0).is_error());
    REQUIRE(project::AudioSettings::create(48000, 2).is_ok());
}

TEST_CASE("Serialize -> deserialize round-trip preserves everything", "[project]") {
    auto p = project::Project::create_validated("RoundTrip", 1280, 720, 24000, 1001);
    REQUIRE(p.is_ok());
    auto& project = p.value();
    auto seq = timeline::Sequence::create("Seq", timeline::Timebase::fps_23_976(), 0);
    REQUIRE(seq.is_ok());
    project.set_sequence(seq.take_value());
    // NOTE: clips need registered media; use the timeline editor through the
    // sequence API and register media afterwards.
    auto track = project.sequence().add_track(timeline::TrackKind::Video, "V1");
    REQUIRE(track.is_ok());

    auto text = project::ProjectStore::serialize(project);
    REQUIRE(text.is_ok());

    auto back = project::ProjectStore::deserialize(text.value());
    REQUIRE(back.is_ok());
    REQUIRE(back.value().name() == "RoundTrip");
    REQUIRE(back.value().id() == project.id());
    REQUIRE(back.value().video().width == 1280);
    REQUIRE(back.value().video().fps == project.video().fps);
    REQUIRE(back.value().sequence().tracks().size() == 1);
    REQUIRE(back.value().sequence().tracks()[0].name == "V1");
    // Round-trip id stability (ids are never regenerated).
    REQUIRE(back.value().sequence().id() == project.sequence().id());
    REQUIRE(back.value().sequence().tracks()[0].id ==
            project.sequence().tracks()[0].id);
}

TEST_CASE("Schema version is enforced", "[project]") {
    const std::string bad_version = R"({"schema_version": 99, "id": "123", "name": "x"})";
    auto res = project::ProjectStore::deserialize(bad_version);
    REQUIRE(res.is_error());
    REQUIRE(res.error().code == core::ErrorCode::VersionUnsupported);
}

TEST_CASE("Corrupted JSON is rejected", "[project]") {
    auto res = project::ProjectStore::deserialize("{ this is not json");
    REQUIRE(res.is_error());
    REQUIRE(res.error().code == core::ErrorCode::ParseError);
}

TEST_CASE("Missing structural fields are rejected", "[project]") {
    auto res = project::ProjectStore::deserialize(R"({"schema_version": 1})");
    REQUIRE(res.is_error());
}

TEST_CASE("Atomic save writes the .veltrix extension and loads back", "[project]") {
    auto p = project::Project::create_validated("FileTest", 1920, 1080, 25, 1);
    REQUIRE(p.is_ok());
    const auto dir = std::filesystem::temp_directory_path() / "veltrix-tests";
    std::filesystem::create_directories(dir);
    const std::string path = (dir / "created").string();
    auto save = project::ProjectStore::save_to_file(p.value(), path);
    REQUIRE(save.is_ok());
    REQUIRE(std::filesystem::exists(dir / "created.veltrix"));

    auto load = project::ProjectStore::load_from_file((dir / "created.veltrix").string());
    REQUIRE(load.is_ok());
    REQUIRE(load.value().name() == "FileTest");

    std::filesystem::remove(dir / "created.veltrix");
}

TEST_CASE("Atomic save does not leave temp files on success", "[project]") {
    auto p = project::Project::create_validated("TempTest", 1920, 1080, 25, 1);
    const auto dir = std::filesystem::temp_directory_path() / "veltrix-tests";
    std::filesystem::create_directories(dir);
    const auto file = dir / "tempcheck.veltrix";
    REQUIRE(project::ProjectStore::save_to_file(p.value(), file.string()).is_ok());
    REQUIRE(std::filesystem::exists(file));
    REQUIRE(!std::filesystem::exists(dir / "tempcheck.veltrix.tmp"));
    std::filesystem::remove(file);
}

TEST_CASE("Load of a nonexistent file fails cleanly", "[project]") {
    auto res = project::ProjectStore::load_from_file(
        "Z:/definitely/not/there.veltrix");
    REQUIRE(res.is_error());
    REQUIRE(res.error().code == core::ErrorCode::NotFound);
}

TEST_CASE("Missing media is loaded but flagged", "[project]") {
    // Build a project that references a nonexistent media file by hand.
    auto p = project::Project::create_validated("Offline", 1920, 1080, 25, 1);
    REQUIRE(p.is_ok());
    media::MediaItem item;
    item.id.value = 12345;
    item.path = "Z:/definitely/missing_clip.mp4";
    item.kind = media::MediaKind::Video;
    REQUIRE(p.value().media().load_item(std::move(item)).is_ok());
    const std::size_t missing = p.value().media().refresh_availability();
    REQUIRE(missing == 1);
    REQUIRE(p.value().media().find(core::MediaId{12345})->missing);
}

TEST_CASE("Clip referencing unregistered media fails validation", "[project]") {
    auto p = project::Project::create_validated("Dangling", 1920, 1080, 25, 1);
    REQUIRE(p.is_ok());
    auto seq = timeline::Sequence::create("s", timeline::Timebase::fps_25(), 0);
    REQUIRE(seq.is_ok());
    p.value().set_sequence(seq.take_value());
    auto track = p.value().sequence().add_track(timeline::TrackKind::Video, "V1");
    REQUIRE(track.is_ok());
    timeline::Clip clip;
    clip.id.value = 777;
    clip.media.value = 424242; // NOT registered
    clip.start = 0;
    clip.duration = 10;
    REQUIRE(p.value().sequence().insert_clip(track.value(), std::move(clip)).is_ok());
    REQUIRE(p.value().validate().is_error());
    REQUIRE(p.value().validate().error().code == core::ErrorCode::ValidationFailed);
}

TEST_CASE("Duplicate media ids are rejected on load", "[project]") {
    auto p = project::Project::create_validated("Dup", 1920, 1080, 25, 1);
    media::MediaItem a;
    a.id.value = 1;
    a.path = "C:/one.mov";
    media::MediaItem b;
    b.id.value = 1; // same id
    b.path = "C:/two.mov";
    REQUIRE(p.value().media().load_item(std::move(a)).is_ok());
    REQUIRE(p.value().media().load_item(std::move(b)).is_error());
}
