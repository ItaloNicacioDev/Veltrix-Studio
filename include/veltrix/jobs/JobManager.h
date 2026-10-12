// Veltrix Jobs - JobManager.h
//
// Single-worker job executor (foundation scale, per ADR-005: explicit
// concurrency rules before a pool). Rules:
//   * exactly one worker thread executes jobs FIFO;
//   * submit() returns a handle (shared_ptr<Job>); the manager never
//     destroys a running job;
//   * cancel(job) marks cancellation; the worker observes the token;
//   * wait_all() joins the worker after draining the queue - call once at
//     shutdown, from the owner thread.

#pragma once

#include <condition_variable>
#include <deque>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

#include "veltrix/core/Result.h"
#include "veltrix/jobs/Job.h"

namespace veltrix::jobs {

class JobManager {
public:
    JobManager();
    ~JobManager();

    JobManager(const JobManager&) = delete;
    JobManager& operator=(const JobManager&) = delete;

    // Queues the job. Rejects null jobs and jobs not in Queued state.
    veltrix::core::ResultVoid submit(std::shared_ptr<Job> job);

    // Requests cancellation of a queued or running job. Idempotent.
    void cancel(const veltrix::core::JobId& id);

    // Asks the worker to stop; queued jobs become Cancelled. Call from the
    // owner thread; the destructor also performs a safe shutdown.
    void shutdown();

    // Blocks until the queue is empty AND no job is running.
    void wait_all();

    // Snapshot of the states of all tracked jobs (for diagnostics).
    std::vector<std::pair<veltrix::core::JobId, JobState>> states() const;

private:
    void worker_loop();
    void notify_change() { cv_.notify_all(); }

    mutable std::mutex mutex_;
    std::condition_variable cv_;
    std::deque<std::shared_ptr<Job>> queue_;
    std::vector<std::shared_ptr<Job>> all_;
    std::thread worker_;
    bool shutdown_requested_ = false;
    bool worker_idle_ = true;
};

} // namespace veltrix::jobs
