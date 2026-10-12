// Unit tests - timeline editing (Sequence + TimelineEditor + undo/redo).

#include <catch2/catch.hpp>

#include "veltrix/core/Id.h"
#include "veltrix/timeline/TimelineEditor.h"

using namespace veltrix;
using namespace veltrix::timeline;

namespace {
struct Fixture {
    TimelineEditor editor;
    veltrix::core::TrackId video_track;
    veltrix::core::MediaId media;

    Fixture() {
        auto seq = Sequence::create("Main", Timebase::fps_24(), 0);
        REQUIRE(seq.is_ok());
        editor.load(seq.take_value());
        auto track = editor.add_track(TrackKind::Video, "V1");
        REQUIRE(track.is_ok());
        video_track = track.value();
        media = veltrix::core::MediaId::generate();
    }
};
} // namespace

TEST_CASE_METHOD(Fixture, "Insert validates arguments", "[timeline]") {
    REQUIRE(editor.insert_clip(video_track, media, 0, 0, 0).is_error());
    REQUIRE(editor.insert_clip(video_track, media, -1, 10, 0).is_error());
    REQUIRE(editor.insert_clip(video_track, media, 0, 10, -1).is_error());
    REQUIRE(editor.insert_clip(video_track, veltrix::core::MediaId{}, 0, 10, 0).is_error());
    REQUIRE(editor.insert_clip(veltrix::core::TrackId{}, media, 0, 10, 0).is_error());
}

TEST_CASE_METHOD(Fixture, "Insert rejects overlap and keeps state intact", "[timeline]") {
    auto first = editor.insert_clip(video_track, media, 0, 100, 0);
    REQUIRE(first.is_ok());
    auto overlap = editor.insert_clip(video_track, media, 50, 10, 0);
    REQUIRE(overlap.is_error());
    REQUIRE(overlap.error().code == veltrix::core::ErrorCode::Conflict);
    // Failed op must NOT consume an undo slot. The Fixture constructor's
    // add_track is the first history entry; the successful insert is the
    // second; the failed insert adds none.
    REQUIRE(editor.undo_depth() == 2);
    const Track* t = editor.sequence().find_track(video_track);
    REQUIRE(t->clips.size() == 1);
}

TEST_CASE_METHOD(Fixture, "Adjacent clips are allowed", "[timeline]") {
    auto a = editor.insert_clip(video_track, media, 0, 100, 0);
    auto b = editor.insert_clip(video_track, media, 100, 50, 0);
    REQUIRE(a.is_ok());
    REQUIRE(b.is_ok());
    REQUIRE(editor.sequence().find_track(video_track)->clips.size() == 2);
}

TEST_CASE_METHOD(Fixture, "Move respects overlap rules", "[timeline]") {
    auto a = editor.insert_clip(video_track, media, 0, 100, 0);
    auto b = editor.insert_clip(video_track, media, 200, 100, 0);
    REQUIRE(a.is_ok());
    REQUIRE(b.is_ok());
    // Move b into overlap with a: rejected.
    REQUIRE(editor.move_clip(video_track, b.value(), 50).is_error());
    // Fit exactly in the gap [100, 200): allowed.
    REQUIRE(editor.move_clip(video_track, b.value(), 100).is_ok());
    const Track* t = editor.sequence().find_track(video_track);
    REQUIRE(t->clips[0].start == 0);
    REQUIRE(t->clips[1].start == 100);
}

TEST_CASE_METHOD(Fixture, "Split produces two consistent clips", "[timeline]") {
    auto a = editor.insert_clip(video_track, media, 0, 100, 10);
    REQUIRE(a.is_ok());
    auto right = editor.split_clip(video_track, a.value(), 40);
    REQUIRE(right.is_ok());
    const Track* t = editor.sequence().find_track(video_track);
    REQUIRE(t->clips.size() == 2);
    REQUIRE(t->clips[0].id == a.value());
    REQUIRE(t->clips[0].start == 0);
    REQUIRE(t->clips[0].duration == 40);
    REQUIRE(t->clips[0].source_in == 10);
    REQUIRE(t->clips[1].id == right.value());
    REQUIRE(t->clips[1].start == 40);
    REQUIRE(t->clips[1].duration == 60);
    REQUIRE(t->clips[1].source_in == 50);
    // Split at boundaries is rejected.
    REQUIRE(editor.split_clip(video_track, a.value(), 0).is_error());
    REQUIRE(editor.split_clip(video_track, a.value(), 100).is_error());
    REQUIRE(editor.split_clip(video_track, a.value(), -5).is_error());
}

TEST_CASE_METHOD(Fixture, "Undo and redo round-trip edits", "[timeline]") {
    auto a = editor.insert_clip(video_track, media, 0, 100, 0);
    REQUIRE(a.is_ok());
    // History: add_track (fixture) + insert_clip = 2 entries.
    REQUIRE(editor.undo_depth() == 2);
    REQUIRE(editor.undo().is_ok());
    REQUIRE(editor.sequence().find_track(video_track)->clips.empty());
    REQUIRE(editor.redo().is_ok());
    const Track* t = editor.sequence().find_track(video_track);
    REQUIRE(t->clips.size() == 1);
    REQUIRE(t->clips[0].id == a.value());
    // History: add_track + insert_clip. Two undos drain it; the third errors.
    REQUIRE(editor.undo().is_ok());
    REQUIRE(editor.undo().is_ok());
    REQUIRE(editor.undo().is_error());
}

TEST_CASE_METHOD(Fixture, "New ops clear the redo stack", "[timeline]") {
    auto a = editor.insert_clip(video_track, media, 0, 100, 0);
    REQUIRE(editor.undo().is_ok());
    REQUIRE(editor.redo_depth() == 1);
    auto b = editor.insert_clip(video_track, media, 500, 10, 0);
    REQUIRE(b.is_ok());
    REQUIRE(editor.redo_depth() == 0);
}

TEST_CASE_METHOD(Fixture, "Track operations are undoable", "[timeline]") {
    auto t2 = editor.add_track(TrackKind::Audio, "A1");
    REQUIRE(t2.is_ok());
    REQUIRE(editor.rename_track(t2.value(), "Music").is_ok());
    REQUIRE(editor.sequence().find_track(t2.value())->name == "Music");
    REQUIRE(editor.undo().is_ok()); // undo rename
    REQUIRE(editor.sequence().find_track(t2.value())->name == "A1");
    REQUIRE(editor.undo().is_ok()); // undo add
    REQUIRE(editor.sequence().find_track(t2.value()) == nullptr);
}

TEST_CASE_METHOD(Fixture, "Remove clip then undo restores it", "[timeline]") {
    auto a = editor.insert_clip(video_track, media, 0, 100, 0);
    REQUIRE(editor.remove_clip(video_track, a.value()).is_ok());
    REQUIRE(editor.sequence().find_track(video_track)->clips.empty());
    REQUIRE(editor.undo().is_ok());
    const Track* t = editor.sequence().find_track(video_track);
    REQUIRE(t->clips.size() == 1);
    REQUIRE(t->clips[0].id == a.value());
    REQUIRE(t->clips[0].media == media);
}

TEST_CASE_METHOD(Fixture, "Trim keeps end anchored and validates overlap", "[timeline]") {
    auto a = editor.insert_clip(video_track, media, 100, 100, 0); // [100,200)
    auto b = editor.insert_clip(video_track, media, 200, 50, 0);  // [200,250)
    REQUIRE(a.is_ok());
    REQUIRE(b.is_ok());
    // Extend a into b: rejected.
    REQUIRE(editor.trim_clip(video_track, a.value(), 0, 150).is_error());
    // Trim 10 ticks off the head: start -> 110, duration -> 90.
    REQUIRE(editor.trim_clip(video_track, a.value(), 10, 90).is_ok());
    const Track* t = editor.sequence().find_track(video_track);
    REQUIRE(t->clips[0].start == 110);
    REQUIRE(t->clips[0].duration == 90);
    REQUIRE(t->clips[0].source_in == 10);
}

TEST_CASE_METHOD(Fixture, "Active clips query", "[timeline]") {
    auto a = editor.insert_clip(video_track, media, 0, 100, 0);
    auto b = editor.insert_clip(video_track, media, 200, 50, 0);
    REQUIRE(a.is_ok());
    REQUIRE(b.is_ok());
    auto active = editor.sequence().clips_active_at(50);
    REQUIRE(active.size() == 1);
    REQUIRE(active[0]->id == a.value());
    REQUIRE(editor.sequence().clips_active_at(150).empty());
    REQUIRE(editor.sequence().clips_active_at(200).size() == 1);
}

TEST_CASE_METHOD(Fixture, "Track kinds and metadata round-trip", "[timeline]") {
    auto t2 = editor.add_track(TrackKind::Audio, "A1");
    REQUIRE(t2.is_ok());
    auto track_ptr = editor.sequence().find_track(t2.value());
    REQUIRE(track_ptr->kind == TrackKind::Audio);
    REQUIRE(track_ptr->name == "A1");
}

TEST_CASE("Sequence rejects invalid timebase and negative duration", "[timeline]") {
    REQUIRE(Sequence::create("x", Timebase(), -1).is_error()); // null timebase
    auto tb = Timebase::from_fps(24, 1);
    REQUIRE(tb.is_ok());
    REQUIRE(Sequence::create("x", Timebase(), 100).is_error());
    auto ok = Sequence::create("x", tb.value(), 100);
    REQUIRE(ok.is_ok());
    REQUIRE(ok.value().duration() == 100);
}

TEST_CASE("Sequence explicit duration vs content duration", "[timeline]") {
    auto tb = Timebase::from_fps(24, 1);
    auto seq = Sequence::create("x", tb.value(), 0);
    REQUIRE(seq.is_ok());
    auto seqv = seq.take_value();
    seqv.set_duration(500);
    REQUIRE(seqv.duration() == 500);
    REQUIRE(seqv.content_duration() == 0);
}
