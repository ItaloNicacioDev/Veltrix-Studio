// Unit tests - plugin manifest parsing and compatibility.

#include <catch2/catch.hpp>

#include "veltrix/plugins/PluginManifest.h"

using namespace veltrix;

TEST_CASE("Valid manifest parses with all fields", "[plugins]") {
    const std::string text = R"({
        "id": "com.vendor.coolblur",
        "name": "Cool Blur",
        "vendor": "Vendor Inc",
        "version": "1.2.3",
        "api_version": 1,
        "type": "effect",
        "entry_library": "lib/coolblur.dll"
    })";
    auto m = plugins::PluginManifest::from_json(text);
    REQUIRE(m.is_ok());
    REQUIRE(m.value().id == "com.vendor.coolblur");
    REQUIRE(m.value().name == "Cool Blur");
    REQUIRE(m.value().vendor == "Vendor Inc");
    REQUIRE(m.value().version_major == 1);
    REQUIRE(m.value().version_minor == 2);
    REQUIRE(m.value().version_patch == 3);
    REQUIRE(m.value().api_version == 1);
    REQUIRE(m.value().type == plugins::PluginType::Effect);
    REQUIRE(m.value().entry_library == "lib/coolblur.dll");
    REQUIRE(m.value().is_compatible());
}

TEST_CASE("Missing required fields are rejected", "[plugins]") {
    REQUIRE(plugins::PluginManifest::from_json("{}").is_error());
    REQUIRE(plugins::PluginManifest::from_json(
                R"({"id":"x","name":"y","version":"1.0.0"})")
                .is_error()); // no api_version
    REQUIRE(plugins::PluginManifest::from_json(
                R"({"id":"x","version":"1.0.0","api_version":1})")
                .is_error()); // no name
    REQUIRE(plugins::PluginManifest::from_json("not json").is_error());
}

TEST_CASE("Newer plugin API is refused, not guessed", "[plugins]") {
    const std::string text = R"({
        "id": "com.vendor.future",
        "name": "Future",
        "version": "0.1.0",
        "api_version": 999
    })";
    auto m = plugins::PluginManifest::from_json(text);
    REQUIRE(m.is_error());
    REQUIRE(m.error().code == core::ErrorCode::PluginIncompatible);
}

TEST_CASE("Malformed version strings are rejected", "[plugins]") {
    const std::string text = R"({
        "id": "x", "name": "y", "api_version": 1, "version": "1.2"
    })";
    REQUIRE(plugins::PluginManifest::from_json(text).is_error());
    const std::string text2 = R"({
        "id": "x", "name": "y", "api_version": 1, "version": "abc"
    })";
    REQUIRE(plugins::PluginManifest::from_json(text2).is_error());
}

TEST_CASE("Audio plugin type parses", "[plugins]") {
    const std::string text = R"({
        "id": "com.vendor.comp", "name": "Comp", "version": "2.0.0",
        "api_version": 1, "type": "audio"
    })";
    auto m = plugins::PluginManifest::from_json(text);
    REQUIRE(m.is_ok());
    REQUIRE(m.value().type == plugins::PluginType::AudioProcessor);
}

TEST_CASE("Unknown optional type string is an error", "[plugins]") {
    const std::string text = R"({
        "id": "x", "name": "y", "version": "1.0.0", "api_version": 1,
        "type": "mystery"
    })";
    REQUIRE(plugins::PluginManifest::from_json(text).is_error());
}

TEST_CASE("Older api stays compatible", "[plugins]") {
    const std::string text = R"({
        "id": "x", "name": "y", "version": "1.0.0", "api_version": 1
    })";
    auto m = plugins::PluginManifest::from_json(text);
    REQUIRE(m.is_ok());
    REQUIRE(m.value().is_compatible());
}
