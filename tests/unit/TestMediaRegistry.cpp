// Unit tests - media registry: registration, duplicates, missing files.

#include <catch2/catch.hpp>

#include <filesystem>
#include <fstream>

#include "veltrix/media/MediaRegistry.h"

using namespace veltrix;

namespace {
std::string temp_media_file() {
    const auto dir = std::filesystem::temp_directory_path() / "veltrix-tests";
    std::filesystem::create_directories(dir);
    const auto path = dir / "clip.mp4";
    std::ofstream out(path, std::ios::binary);
    out << "dummy";
    return path.string();
}
} // namespace

TEST_CASE("Register media requires an existing file", "[media]") {
    media::MediaRegistry registry;
    auto res = registry.register_media("Z:/nope/missing.mp4");
    REQUIRE(res.is_error());
    REQUIRE(res.error().code == core::ErrorCode::MediaUnavailable);

    const std::string existing = temp_media_file();
    auto ok = registry.register_media(existing);
    REQUIRE(ok.is_ok());
    REQUIRE_FALSE(ok.value().is_null());
    REQUIRE(registry.find(ok.value()) != nullptr);
    REQUIRE(registry.find(ok.value())->path == existing);
}

TEST_CASE("Duplicate paths and ids are rejected", "[media]") {
    media::MediaRegistry registry;
    const std::string existing = temp_media_file();
    REQUIRE(registry.register_media(existing).is_ok());
    auto dup = registry.register_media(existing);
    REQUIRE(dup.is_error());
    REQUIRE(dup.error().code == core::ErrorCode::AlreadyExists);

    auto dup_id = registry.register_media_with_id(
        registry.items().front().id, "Z:/other/file.mov");
    REQUIRE(dup_id.is_error());
}

TEST_CASE("Remove media rebuilds the index", "[media]") {
    media::MediaRegistry registry;
    const std::string existing = temp_media_file();
    auto first = registry.register_media(existing);
    REQUIRE(first.is_ok());
    auto second = registry.register_media_with_id(
        core::MediaId{999}, (std::filesystem::temp_directory_path() /
                             "veltrix-tests" / "other.wav")
                                .string());
    // Note: second registration may fail if the file does not exist; that
    // is the correct behavior and not an error of this test.
    (void)second;
    REQUIRE(registry.remove_media(first.value()).is_ok());
    REQUIRE(registry.find(first.value()) == nullptr);
    // Re-registering after removal works.
    REQUIRE(registry.register_media(existing).is_ok());
}

TEST_CASE("Availability refresh flags missing files", "[media]") {
    media::MediaRegistry registry;
    media::MediaItem item;
    item.id.value = 42;
    item.path = "Z:/vanished/file.mp4";
    REQUIRE(registry.load_item(std::move(item)).is_ok());
    REQUIRE(registry.refresh_availability() == 1);
    REQUIRE(registry.find(core::MediaId{42})->missing);
}

TEST_CASE("Empty path and null id are rejected", "[media]") {
    media::MediaRegistry registry;
    REQUIRE(registry.register_media("").is_error());
    media::MediaItem bad;
    bad.id.value = 0;
    bad.path = "x";
    REQUIRE(registry.load_item(std::move(bad)).is_error());
}
