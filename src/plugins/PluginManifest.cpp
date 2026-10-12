// Veltrix Plugins - PluginManifest.cpp

#include "veltrix/plugins/PluginManifest.h"

#include <cstdio>

#include <nlohmann/json.hpp>

namespace veltrix::plugins {

using veltrix::core::ErrorCode;
using veltrix::core::failure;

veltrix::core::Result<PluginManifest> PluginManifest::from_json(const std::string& text) {
    nlohmann::json doc;
    try {
        doc = nlohmann::json::parse(text);
    } catch (const nlohmann::json::parse_error& e) {
        return failure<PluginManifest>(ErrorCode::ParseError,
                                       std::string("invalid manifest JSON: ") + e.what());
    }
    if (!doc.is_object())
        return failure<PluginManifest>(ErrorCode::ParseError,
                                       "manifest root must be an object");

    PluginManifest m;

    auto read_string = [&](const char* key, std::string& out,
                           bool required) -> bool {
        const auto it = doc.find(key);
        if (it == doc.end()) return !required;
        if (!it->is_string()) return false;
        out = it->get<std::string>();
        return true;
    };

    if (!read_string("id", m.id, true) || m.id.empty())
        return failure<PluginManifest>(ErrorCode::ParseError, "manifest id missing/invalid");
    if (!read_string("name", m.name, true) || m.name.empty())
        return failure<PluginManifest>(ErrorCode::ParseError,
                                       "manifest name missing/invalid");
    read_string("vendor", m.vendor, false);
    read_string("entry_library", m.entry_library, false);

    const auto version = doc.find("version");
    if (version == doc.end() || !version->is_string())
        return failure<PluginManifest>(ErrorCode::ParseError,
                                       "manifest version must be a string");
    {
        const std::string v = version->get<std::string>();
        if (std::sscanf(v.c_str(), "%u.%u.%u", &m.version_major, &m.version_minor,
                        &m.version_patch) != 3)
            return failure<PluginManifest>(ErrorCode::ParseError,
                                           "manifest version must be major.minor.patch",
                                           v);
    }

    const auto api = doc.find("api_version");
    if (api == doc.end() || !api->is_number_unsigned())
        return failure<PluginManifest>(ErrorCode::ParseError,
                                       "manifest api_version missing/invalid");
    m.api_version = api->get<std::uint32_t>();

    const auto type = doc.find("type");
    if (type != doc.end() && type->is_string()) {
        const std::string t = type->get<std::string>();
        if (t == "effect") m.type = PluginType::Effect;
        else if (t == "audio") m.type = PluginType::AudioProcessor;
        else if (!t.empty())
            return failure<PluginManifest>(ErrorCode::ParseError,
                                           "unknown plugin type", t);
    }

    if (!m.is_compatible())
        return failure<PluginManifest>(ErrorCode::PluginIncompatible,
                                       "plugin targets newer plugin API " +
                                           std::to_string(m.api_version) +
                                           " (build supports " +
                                           std::to_string(kCurrentPluginApi) + ")");
    return veltrix::core::Result<PluginManifest>(std::move(m));
}

} // namespace veltrix::plugins
