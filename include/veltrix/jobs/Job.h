// Veltrix Jobs - Job.h
//
// Long-running asynchronous tasks (import, probe, thumbnail, decode,
// render, export). Jobs are explicit objects with lifecycle, progress,
// cancellation and error propagation - not fire-and-forget callbacks.
//
// Ownership rule: a Job owns its payload; the JobManager owns Job state;
// results are moved out exactly once.

#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>

#include "veltrix/core/CancellationToken.h"
#include "veltrix/core/Id.h"
#include "veltrix/core/Result.h"

namespace veltrix::jobs {

using veltrix::core::CancellationToken;

enum class JobKind : std::uint8_t {
    Import = 0,
    Probe,
    Thumbnail,
    Decode,
    Render,
    Export,
    Custom,
};

enum class JobState : std::uint8_t {
    Queued = 0,   // accepted, not started
    Running,      // worker executing
    Succeeded,    // completed without error
    Failed,       // completed with error
    Cancelled,    // cancel() won before completion
};

inline const char* job_state_name(JobState s) {
    switch (s) {
        case JobState::Queued:    return "queued";
        case JobState::Running:   return "running";
        case JobState::Succeeded: return "succeeded";
        case JobState::Failed:    return "failed";
        case JobState::Cancelled: return "cancelled";
    }
    return "?";
}

// Progress in [0.0, 1.0]; -1 = indeterminate. set_progress() clamps.
class JobProgress {
public:
    void set(float fraction) { value_.store(clamp(fraction), std::memory_order_relaxed); }
    float get() const { return value_.load(std::memory_order_relaxed); }

private:
    static float clamp(float f) { return f < 0.0f ? 0.0f : (f > 1.0f ? 1.0f : f); }
    std::atomic<float> value_{-1.0f};
};

// Abstract long task. Implementations run on a worker thread; they MUST
// poll the token and return ResultVoid errors, never throw.
class Job {
public:
    explicit Job(std::string name, JobKind kind = JobKind::Custom)
        : name_(std::move(name)), kind_(kind) {}
    virtual ~Job() = default;

    const veltrix::core::JobId& id() const { return id_; }
    const std::string& name() const { return name_; }
    JobKind kind() const { return kind_; }
    JobState state() const { return state_.load(std::memory_order_acquire); }
    const JobProgress& progress() const { return progress_; }
    JobProgress& progress() { return progress_; }
    CancellationToken& token() { return token_; }

    // Marks the job as started; called by the manager on the worker.
    void mark_running() { state_.store(JobState::Running, std::memory_order_release); }
    // Final states are latches: once set they never change back.
    void mark_succeeded() {
        JobState expected = JobState::Running;
        state_.compare_exchange_strong(expected, JobState::Succeeded);
    }
    void mark_failed() {
        JobState expected = JobState::Running;
        state_.compare_exchange_strong(expected, JobState::Failed);
    }
    void mark_cancelled() {
        // Latch from whichever non-final state the job is in.
        JobState expected = JobState::Running;
        if (state_.compare_exchange_strong(expected, JobState::Cancelled)) {
            token_.cancel();
            return;
        }
        expected = JobState::Queued;
        if (state_.compare_exchange_strong(expected, JobState::Cancelled)) {
            token_.cancel();
        }
        // Any other state is already final: cancellation is a no-op.
    }

    // The work. Runs on a worker thread. Return failure(Cancelled,...) when
    // the token fired mid-run.
    virtual veltrix::core::ResultVoid run() = 0;

private:
    veltrix::core::JobId id_ = veltrix::core::JobId::generate();
    std::string name_;
    JobKind kind_;
    std::atomic<JobState> state_{JobState::Queued};
    JobProgress progress_;
    CancellationToken token_;
};

// Concrete job wrapping a function: ResultVoid(CancellationToken&).
class FunctionJob final : public Job {
public:
    using Fn = std::function<veltrix::core::ResultVoid(CancellationToken&)>;
    FunctionJob(std::string name, JobKind kind, Fn fn)
        : Job(std::move(name), kind), fn_(std::move(fn)) {}
    veltrix::core::ResultVoid run() override {
        if (!fn_) return veltrix::core::failure(veltrix::core::ErrorCode::InvalidArgument,
                                                "job function missing");
        return fn_(token());
    }

private:
    Fn fn_;
};

} // namespace veltrix::jobs
