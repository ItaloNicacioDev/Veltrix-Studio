// veltrix-benchmarks — micro-benchmarks de escala do core (harness próprio,
// std::chrono; sem dependências externas).
//
// Mede os custos óbvios da fundação:
//   1. edição massiva + snapshots de undo (ADR-003);
//   2. serialização/desserialização de projetos grandes (ADR-004).

#include <chrono>
#include <cstdint>
#include <iostream>
#include <string>

#include "veltrix/media/MediaRegistry.h"
#include "veltrix/project/Project.h"
#include "veltrix/project/ProjectStore.h"
#include "veltrix/timeline/TimelineEditor.h"

using namespace veltrix;
using Clock = std::chrono::steady_clock;

namespace {

double ms_since(Clock::time_point start) {
    return std::chrono::duration<double, std::milli>(Clock::now() - start).count();
}

// Insere clip_count clipes justapostos e mede N undos.
void bench_undo_snapshots(std::int64_t clip_count) {
    auto project = project::Project::create_validated("bench", 1920, 1080, 30, 1);
    auto& p = project.value();

    media::MediaItem item;
    item.id.value = 42;
    item.path = "Z:/bench/fake.mp4"; // ausência é irrelevante p/ o bench
    p.media().load_item(std::move(item));
    const core::MediaId media{42};

    timeline::TimelineEditor editor(p.sequence());
    auto track = editor.add_track(timeline::TrackKind::Video, "V1");

    const auto t0 = Clock::now();
    for (std::int64_t i = 0; i < clip_count; ++i)
        editor.insert_clip(track.value(), media, i * 10, 10, 0);
    const double insert_ms = ms_since(t0);

    const auto t1 = Clock::now();
    std::int64_t undos = 0;
    while (editor.can_undo() && undos < 200) {
        editor.undo();
        ++undos;
    }
    const double undo_avg_ms =
        undos > 0 ? ms_since(t1) / static_cast<double>(undos) : 0.0;
    std::cout << "undo-snapshots  clips=" << clip_count
              << "  insert_total_ms=" << insert_ms
              << "  undo_avg_ms=" << undo_avg_ms << "\n";
}

// Serializa e desserializa um projeto com clip_count clipes.
void bench_project_roundtrip(std::int64_t clip_count) {
    auto project = project::Project::create_validated("bench", 1920, 1080, 30, 1);
    auto& p = project.value();

    media::MediaItem item;
    item.id.value = 42;
    item.path = "Z:/bench/fake.mp4";
    p.media().load_item(std::move(item));
    const core::MediaId media{42};

    timeline::TimelineEditor editor(p.sequence());
    auto track = editor.add_track(timeline::TrackKind::Video, "V1");
    for (std::int64_t i = 0; i < clip_count; ++i)
        editor.insert_clip(track.value(), media, i * 10, 10, 0);
    p.set_sequence(editor.take_sequence());

    const auto t0 = Clock::now();
    auto text = project::ProjectStore::serialize(p);
    const double ser_ms = ms_since(t0);
    const auto t1 = Clock::now();
    auto back = project::ProjectStore::deserialize(text.value());
    const double deser_ms = ms_since(t1);
    std::cout << "roundtrip       clips=" << clip_count
              << "  serialize_ms=" << ser_ms
              << "  deserialize_ms=" << deser_ms
              << "  bytes=" << text.value().size()
              << "  ok=" << (back.is_ok() ? "yes" : "NO") << "\n";
}

} // namespace

int main() {
    std::cout << "veltrix core benchmarks\n";
    for (std::int64_t n : {1000, 5000, 20000})
        bench_undo_snapshots(n);
    for (std::int64_t n : {1000, 5000, 20000})
        bench_project_roundtrip(n);
    return 0;
}
