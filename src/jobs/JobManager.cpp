// Veltrix Jobs - JobManager.cpp

#include "veltrix/jobs/JobManager.h"

namespace veltrix::jobs {

using veltrix::core::ErrorCode;
using veltrix::core::failure;
using veltrix::core::ResultVoid;

JobManager::JobManager() : worker_(&JobManager::worker_loop, this) {}

JobManager::~JobManager() {
    shutdown();
    if (worker_.joinable()) worker_.join();
}

ResultVoid JobManager::submit(std::shared_ptr<Job> job) {
    if (!job)
        return failure(ErrorCode::InvalidArgument, "job is null");
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (shutdown_requested_)
            return failure(ErrorCode::Conflict, "JobManager is shut down");
        if (job->state() != JobState::Queued)
            return failure(ErrorCode::Conflict, "job is not in Queued state",
                           job->name());
        queue_.push_back(job);
        all_.push_back(std::move(job));
    }
    notify_change();
    return veltrix::core::success();
}

void JobManager::cancel(const veltrix::core::JobId& id) {
    std::shared_ptr<Job> target;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        for (auto& j : all_) {
            if (j->id() == id) { target = j; break; }
        }
    }
    if (!target) return;
    target->mark_cancelled(); // latches state + fires token
    notify_change();
}

void JobManager::shutdown() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        shutdown_requested_ = true;
        for (auto& j : queue_) j->mark_cancelled();
        queue_.clear();
    }
    notify_change();
}

void JobManager::wait_all() {
    std::unique_lock<std::mutex> lock(mutex_);
    cv_.wait(lock, [&] { return queue_.empty() && worker_idle_; });
}

std::vector<std::pair<veltrix::core::JobId, JobState>> JobManager::states() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::pair<veltrix::core::JobId, JobState>> out;
    out.reserve(all_.size());
    for (const auto& j : all_) out.emplace_back(j->id(), j->state());
    return out;
}

void JobManager::worker_loop() {
    std::unique_lock<std::mutex> lock(mutex_);
    for (;;) {
        cv_.wait(lock, [&] { return shutdown_requested_ || !queue_.empty(); });
        if (shutdown_requested_ && queue_.empty()) return;

        std::shared_ptr<Job> job = queue_.front();
        queue_.pop_front();
        worker_idle_ = false;
        lock.unlock();

        if (job->state() == JobState::Cancelled) {
            // already cancelled while queued
        } else {
            job->mark_running();
            auto result = job->run();
            if (job->state() == JobState::Cancelled) {
                // cancel() won the race; keep Cancelled
            } else if (result.is_error()) {
                job->mark_failed();
            } else {
                job->mark_succeeded();
            }
        }

        lock.lock();
        worker_idle_ = true;
        cv_.notify_all();
    }
}

} // namespace veltrix::jobs
