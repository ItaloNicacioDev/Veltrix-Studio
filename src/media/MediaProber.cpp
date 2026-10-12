// Veltrix Media - MediaProber.cpp
//
// ffprobe is located at RUNTIME (explicit path > VELTRIX_FFPROBE > PATH).
// The core does not link libav* (ADR-006). The media path is passed as the
// final argument. POSIX: files whose names start with '-' are a documented
// limitation (prefix "./"). ffprobe is always invoked with "-i" so option
// injection through the file name is not possible.

#include "veltrix/media/MediaProber.h"

#include <cerrno>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <sstream>
#include <string>
#include <vector>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#else
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

#include <nlohmann/json.hpp>

namespace veltrix::media {

using veltrix::core::ErrorCode;
using veltrix::core::failure;
using veltrix::core::ResultVoid;

namespace {

struct CaptureResult {
    std::string out;
    int exit_code = -1;
    bool timed_out = false;
};

#ifdef _WIN32

CaptureResult capture_windows(const std::string& command_line, int timeout_seconds) {
    SECURITY_ATTRIBUTES sa{sizeof(sa), nullptr, TRUE};
    HANDLE read_end = nullptr, write_end = nullptr;
    if (!CreatePipe(&read_end, &write_end, &sa, 0)) return {};
    SetHandleInformation(read_end, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOA si{};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    si.hStdOutput = write_end;
    si.hStdError = GetStdHandle(STD_ERROR_HANDLE);
    si.hStdInput = nullptr;

    PROCESS_INFORMATION pi{};
    std::vector<char> mutable_cmd(command_line.begin(), command_line.end());
    mutable_cmd.push_back('\0');
    if (!CreateProcessA(nullptr, mutable_cmd.data(), nullptr, nullptr, TRUE,
                        CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) {
        CloseHandle(read_end);
        CloseHandle(write_end);
        return {};
    }
    CloseHandle(pi.hThread);
    CloseHandle(write_end); // the child owns its inherited copy

    std::string out;
    char buffer[4096];
    const auto deadline = std::chrono::steady_clock::now() +
                          std::chrono::seconds(timeout_seconds > 0 ? timeout_seconds
                                                                   : 86400);
    bool timed_out = false;

    for (;;) {
        DWORD avail = 0;
        if (!PeekNamedPipe(read_end, nullptr, 0, nullptr, &avail, nullptr)) break;
        if (avail > 0) {
            DWORD got = 0;
            if (!ReadFile(read_end, buffer, sizeof(buffer), &got, nullptr) || got == 0) break;
            out.append(buffer, got);
            continue;
        }
        DWORD code = 0;
        if (GetExitCodeProcess(pi.hProcess, &code) && code != STILL_ACTIVE) break;
        if (WaitForSingleObject(pi.hProcess, 50) == WAIT_OBJECT_0) break;
        if (timeout_seconds > 0 && std::chrono::steady_clock::now() > deadline) {
            timed_out = true;
            break;
        }
    }
    // Drain whatever is still buffered in the pipe.
    for (;;) {
        DWORD avail = 0;
        if (!PeekNamedPipe(read_end, nullptr, 0, nullptr, &avail, nullptr) || avail == 0)
            break;
        DWORD got = 0;
        if (!ReadFile(read_end, buffer, sizeof(buffer), &got, nullptr) || got == 0) break;
        out.append(buffer, got);
    }

    DWORD exit_code = 0;
    GetExitCodeProcess(pi.hProcess, &exit_code);
    if (timed_out) {
        TerminateProcess(pi.hProcess, 1);
        WaitForSingleObject(pi.hProcess, 5000);
    }
    CloseHandle(pi.hProcess);
    CloseHandle(read_end);
    return {std::move(out), static_cast<int>(exit_code), timed_out};
}

#else // POSIX

CaptureResult capture_posix(const std::vector<std::string>& args, int timeout_seconds) {
    int fds[2];
    if (pipe(fds) != 0) return {};
    const pid_t pid = fork();
    if (pid < 0) { close(fds[0]); close(fds[1]); return {}; }
    if (pid == 0) {
        close(fds[0]);
        dup2(fds[1], STDOUT_FILENO);
        const int devnull = open("/dev/null", O_WRONLY);
        if (devnull >= 0) { dup2(devnull, STDERR_FILENO); close(devnull); }
        close(fds[1]);
        std::vector<char*> argv;
        argv.reserve(args.size() + 1);
        for (const auto& a : args) argv.push_back(const_cast<char*>(a.c_str()));
        argv.push_back(nullptr);
        execv(args[0].c_str(), argv.data());
        _exit(127); // exec failed
    }
    close(fds[1]);

    std::string out;
    char buffer[4096];
    const auto deadline = std::chrono::steady_clock::now() +
                          std::chrono::seconds(timeout_seconds > 0 ? timeout_seconds
                                                                   : 86400);
    bool timed_out = false;

    struct pollfd pfd{fds[0], POLLIN, 0};
    for (;;) {
        const int ready = poll(&pfd, 1, 50);
        if (ready > 0) {
            const ssize_t n = read(fds[0], buffer, sizeof(buffer));
            if (n <= 0) break;
            out.append(buffer, static_cast<std::size_t>(n));
            continue;
        }
        int status = 0;
        if (waitpid(pid, &status, WNOHANG) == pid) {
            for (;;) { // exited: drain the rest
                const ssize_t n = read(fds[0], buffer, sizeof(buffer));
                if (n <= 0) break;
                out.append(buffer, static_cast<std::size_t>(n));
            }
            close(fds[0]);
            if (WIFEXITED(status)) return {std::move(out), WEXITSTATUS(status), false};
            return {std::move(out), -1, false};
        }
        if (timeout_seconds > 0 && std::chrono::steady_clock::now() > deadline) {
            timed_out = true;
            break;
        }
    }
    if (timed_out) {
        kill(pid, SIGKILL);
        int status = 0;
        waitpid(pid, &status, 0);
    }
    close(fds[0]);
    return {std::move(out), -1, timed_out};
}

#endif

bool parse_fraction(const std::string& text, std::int64_t& out_num, std::int64_t& out_den) {
    out_num = 0;
    out_den = 1;
    const auto slash = text.find('/');
    if (slash == std::string::npos) {
        errno = 0;
        char* end = nullptr;
        const long long v = std::strtoll(text.c_str(), &end, 10);
        if (errno != 0 || end == text.c_str()) return false;
        out_num = static_cast<std::int64_t>(v);
        return true;
    }
    errno = 0;
    char* end = nullptr;
    const long long num = std::strtoll(text.substr(0, slash).c_str(), &end, 10);
    if (errno != 0 || end == text.c_str()) return false;
    errno = 0;
    const long long den = std::strtoll(text.substr(slash + 1).c_str(), &end, 10);
    if (errno != 0 || den == 0) return false;
    out_num = static_cast<std::int64_t>(num);
    out_den = static_cast<std::int64_t>(den);
    return true;
}

// Decimal seconds string ("12.040000") -> integer microseconds (rounded).
bool seconds_to_us(const std::string& text, std::int64_t& out_us) {
    errno = 0;
    char* end = nullptr;
    const double seconds = std::strtod(text.c_str(), &end);
    if (end == text.c_str() || seconds < 0.0) return false;
    if (seconds > 9.2e12) return false; // > ~292k years: reject nonsense
    out_us = static_cast<std::int64_t>(seconds * 1.0e6 + 0.5);
    return true;
}

} // namespace

veltrix::core::Result<std::string> MediaProber::locate_ffprobe(const std::string& explicit_path) {
    auto exists = [](const std::string& candidate) {
        std::error_code ec;
        return !candidate.empty() &&
               std::filesystem::exists(std::filesystem::path(candidate), ec);
    };

    if (!explicit_path.empty()) {
        if (exists(explicit_path))
            return veltrix::core::Result<std::string>(explicit_path);
#ifdef _WIN32
        if (exists(explicit_path + ".exe"))
            return veltrix::core::Result<std::string>(explicit_path + ".exe");
#endif
        return failure<std::string>(ErrorCode::NotFound,
                                    "explicit ffprobe path does not exist", explicit_path);
    }

    if (const char* env = std::getenv("VELTRIX_FFPROBE")) {
        const std::string env_path(env);
        if (exists(env_path))
            return veltrix::core::Result<std::string>(env_path);
#ifdef _WIN32
        if (exists(env_path + ".exe"))
            return veltrix::core::Result<std::string>(env_path + ".exe");
#endif
    }

#ifdef _WIN32
    char buffer[MAX_PATH] = {};
    const DWORD length = SearchPathA(nullptr, "ffprobe.exe", nullptr, MAX_PATH, buffer, nullptr);
    if (length > 0 && length < MAX_PATH)
        return veltrix::core::Result<std::string>(std::string(buffer, length));
#else
    if (const char* path_env = std::getenv("PATH")) {
        std::istringstream stream(path_env);
        std::string entry;
        while (std::getline(stream, entry, ':')) {
            if (entry.empty()) continue;
            const std::string candidate = entry + "/ffprobe";
            struct stat st{};
            if (stat(candidate.c_str(), &st) == 0 && (st.st_mode & S_IXUSR))
                return veltrix::core::Result<std::string>(candidate);
        }
    }
#endif
    return failure<std::string>(
        ErrorCode::NotFound,
        "ffprobe not found: set VELTRIX_FFPROBE, pass an explicit path, or extend PATH");
}

ResultVoid MediaProber::probe(MediaItem& item, const std::string& ffprobe_path,
                               int timeout_seconds) {
    if (item.path.empty())
        return failure(ErrorCode::InvalidArgument, "media path is empty");
    if (ffprobe_path.empty())
        return failure(ErrorCode::InvalidArgument, "ffprobe path is empty");

    CaptureResult captured;
#ifdef _WIN32
    const std::string command_line = "\"" + ffprobe_path +
        "\" -v error -print_format json -show_streams -show_format -i \"" + item.path + "\"";
    captured = capture_windows(command_line, timeout_seconds);
#else
    const std::vector<std::string> args{
        ffprobe_path, "-v", "error", "-print_format", "json",
        "-show_streams", "-show_format", "-i", item.path};
    captured = capture_posix(args, timeout_seconds);
#endif

    if (captured.timed_out)
        return failure(ErrorCode::ProbeFailed, "ffprobe timed out", item.path);
    if (captured.exit_code != 0)
        return failure(ErrorCode::ProbeFailed,
                       "ffprobe failed with exit code " + std::to_string(captured.exit_code),
                       item.path);
    if (captured.out.empty())
        return failure(ErrorCode::ProbeFailed, "ffprobe produced no output", item.path);

    nlohmann::json doc;
    try {
        doc = nlohmann::json::parse(captured.out);
    } catch (const nlohmann::json::parse_error&) {
        return failure(ErrorCode::ProbeFailed, "ffprobe output is not valid JSON", item.path);
    }

    bool has_video = false;
    bool has_audio = false;

    if (auto streams = doc.find("streams"); streams != doc.end() && streams->is_array()) {
        for (const auto& s : *streams) {
            StreamInfo info;
            if (auto f = s.find("index"); f != s.end() && f->is_number())
                info.index = f->get<std::int32_t>();
            if (auto f = s.find("codec_name"); f != s.end() && f->is_string())
                info.codec_name = f->get<std::string>();
            std::string codec_type;
            if (auto f = s.find("codec_type"); f != s.end() && f->is_string())
                codec_type = f->get<std::string>();
            if (codec_type == "video") {
                has_video = true;
                if (auto f = s.find("width"); f != s.end() && f->is_number())
                    info.width = f->get<std::int32_t>();
                if (auto f = s.find("height"); f != s.end() && f->is_number())
                    info.height = f->get<std::int32_t>();
                std::int64_t num = 0, den = 1;
                if (auto f = s.find("r_frame_rate"); f != s.end() && f->is_string() &&
                    parse_fraction(f->get<std::string>(), num, den)) {
                    info.fps_num = num;
                    info.fps_den = den;
                }
                if (auto f = s.find("duration"); f != s.end() && f->is_string())
                    seconds_to_us(f->get<std::string>(), info.duration_us);
            } else if (codec_type == "audio") {
                has_audio = true;
                if (auto f = s.find("sample_rate"); f != s.end() && f->is_string()) {
                    std::int64_t num = 0, den = 1;
                    if (parse_fraction(f->get<std::string>(), num, den))
                        info.sample_rate = static_cast<std::int32_t>(num);
                }
                if (auto f = s.find("channels"); f != s.end() && f->is_number())
                    info.channels = f->get<std::int32_t>();
                if (auto f = s.find("duration"); f != s.end() && f->is_string())
                    seconds_to_us(f->get<std::string>(), info.duration_us);
            } else {
                continue; // subtitles/data/attachments: not editable media
            }
            item.streams.push_back(std::move(info));
        }
    }

    if (auto format = doc.find("format"); format != doc.end() && format->is_object()) {
        if (auto f = format->find("duration"); f != format->end() && f->is_string())
            seconds_to_us(f->get<std::string>(), item.duration_us);
    }

    if (item.kind == MediaKind::Unknown) {
        if (has_video) item.kind = MediaKind::Video;
        else if (has_audio) item.kind = MediaKind::Audio;
    }
    if (item.streams.empty())
        return failure(ErrorCode::Unsupported,
                       "no decodable video/audio streams found", item.path);
    return veltrix::core::success();
}

} // namespace veltrix::media
