// Integration tests - full workflow: create project -> register media ->
// edit timeline -> save -> load -> verify state survived.

#include <catch2/catch.hpp>

#include <filesystem>
#include <fstream>

#include "veltrix/project/Project.h"
#include "veltrix/project/ProjectStore.h"
#include "veltrix/timeline/TimelineEditor.h"

using namespace veltrix;

namespace {
std::string make_media_file(const std::string& name) {
    const auto dir = std::filesystem::temp_directory_path() / "veltrix-integration";
    std::filesystem::create_directories(dir);
    const auto path = dir / name;
    std::ofstream out(path, std::ios::binary);
    out << "integration-fixture";
    return path.string();
}
} // namespace

TEST_CASE("Project save/load round-trips an edited timeline", "[integration]") {
    // 1. Create project + sequence + tracks.
    auto project = project::Project::create_validated("Integration", 1920, 1080, 30, 1);
    REQUIRE(project.is_ok());
    auto seq = timeline::Sequence::create("Main", timeline::Timebase::fps_30(), 0);
    REQUIRE(seq.is_ok());
    project.value().set_sequence(seq.take_value());

    // 2. Register real media (temp files).
    const std::string clip_a_path = make_media_file("a.mp4");
    const std::string clip_b_path = make_media_file("b.wav");
    auto media_a = project.value().media().register_media(clip_a_path);
    auto media_b = project.value().media().register_media(clip_b_path);
    REQUIRE(media_a.is_ok());
    REQUIRE(media_b.is_ok());

    // 3. Edit through the TimelineEditor (validated ops). The editor
    // copies the sequence out of the project and gives it back at the end.
    timeline::TimelineEditor editor(project.value().sequence());

    auto v1 = editor.add_track(timeline::TrackKind::Video, "V1");
    REQUIRE(v1.is_ok());
    auto c1 = editor.insert_clip(v1.value(), media_a.value(), 0, 100, 0);
    REQUIRE(c1.is_ok());
    auto c2 = editor.insert_clip(v1.value(), media_a.value(), 100, 50, 20);
    REQUIRE(c2.is_ok());
    auto split = editor.split_clip(v1.value(), c1.value(), 50);
    REQUIRE(split.is_ok());
    REQUIRE(editor.move_clip(v1.value(), c2.value(), 200).is_ok());
    auto a1 = editor.add_track(timeline::TrackKind::Audio, "A1");
    REQUIRE(a1.is_ok());
    auto c3 = editor.insert_clip(a1.value(), media_b.value(), 10, 90, 0);
    REQUIRE(c3.is_ok());

    // Undo/redo the move of c2. The move is 3 operations back (insert c3,
    // add_track A1, move c2), so undo three times, verify, redo three times.
    REQUIRE(editor.undo().is_ok()); // undo insert c3 (audio)
    REQUIRE(editor.undo().is_ok()); // undo add_track A1
    REQUIRE(editor.undo().is_ok()); // undo move c2 -> back to 100
    const auto* c2_after_undo = editor.sequence().find_clip(v1.value(), c2.value());
    REQUIRE(c2_after_undo != nullptr);
    REQUIRE(c2_after_undo->start == 100);
    REQUIRE(editor.redo().is_ok()); // redo move
    REQUIRE(editor.redo().is_ok()); // redo add_track A1
    REQUIRE(editor.redo().is_ok()); // redo insert c3
    const auto* c2_after_redo = editor.sequence().find_clip(v1.value(), c2.value());
    REQUIRE(c2_after_redo != nullptr);
    REQUIRE(c2_after_redo->start == 200);

    project.value().set_sequence(editor.take_sequence());
    REQUIRE(project.value().validate().is_ok());

    // 4. Save, load, compare.
    const auto dir = std::filesystem::temp_directory_path() / "veltrix-integration";
    const std::string file = (dir / "integration.veltrix").string();
    auto save = project::ProjectStore::save_to_file(project.value(), file);
    REQUIRE(save.is_ok());

    auto loaded = project::ProjectStore::load_from_file(file);
    REQUIRE(loaded.is_ok());
    const auto& lp = loaded.value();

    REQUIRE(lp.name() == "Integration");
    REQUIRE(lp.id() == project.value().id());
    REQUIRE(lp.media().size() == 2);
    REQUIRE(lp.sequence().tracks().size() == 2);
    REQUIRE(lp.sequence().tracks()[0].clips.size() == 3); // split added one
    REQUIRE(lp.sequence().tracks()[1].clips.size() == 1);
    // c2 moved to 200 and is the last clip in sorted order.
    REQUIRE(lp.sequence().tracks()[0].clips[2].start == 200);
    // Clip ids survived the round-trip.
    REQUIRE(lp.sequence().tracks()[0].clips[0].id == c1.value());
    REQUIRE(lp.sequence().tracks()[1].clips[0].media == media_b.value());
    // Media paths survived.
    REQUIRE(lp.media().find(media_a.value())->path == clip_a_path);

    std::filesystem::remove(dir / "integration.veltrix");
}

TEST_CASE("CLI-equivalent workflow with missing media keeps loading", "[integration]") {
    // A project whose media is unavailable must STILL load (offline edit).
    auto project = project::Project::create_validated("Offline", 1920, 1080, 25, 1);
    REQUIRE(project.is_ok());
    media::MediaItem item;
    item.id.value = 7;
    item.path = "Z:/nowhere/clip.mp4";
    item.kind = media::MediaKind::Video;
    REQUIRE(project.value().media().load_item(std::move(item)).is_ok());

    const std::string json = project::ProjectStore::serialize(project.value()).value();
    auto back = project::ProjectStore::deserialize(json);
    REQUIRE(back.is_ok());
    REQUIRE(back.value().media().size() == 1);
    const std::size_t missing = back.value().media().refresh_availability();
    REQUIRE(missing == 1);
}
