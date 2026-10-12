// Veltrix Core - CancellationToken.h
//
// Cooperative cancellation for long operations (jobs, decode, export).
// Invariants:
//   * cancellation is a one-way latch: once cancelled it never resets;
//   * testing the token is lock-free (atomic loads);
//   * callbacks registered after cancellation still fire exactly once.

#pragma once

#include <atomic>
#include <functional>
#include <mutex>
#include <vector>

namespace veltrix::core {

class CancellationToken {
public:
    CancellationToken() = default;
    CancellationToken(const CancellationToken&) = delete;
    CancellationToken& operator=(const CancellationToken&) = delete;

    void cancel() {
        bool expected = false;
        if (!cancelled_.compare_exchange_strong(expected, true, std::memory_order_acq_rel))
            return; // already cancelled
        std::lock_guard<std::mutex> lock(callbacks_mutex_);
        for (const auto& cb : callbacks_) cb();
        callbacks_.clear();
        callbacks_.shrink_to_fit();
    }

    bool is_cancelled() const { return cancelled_.load(std::memory_order_acquire); }

    // Registers a callback executed on cancel(), or immediately (on the
    // caller thread) when the token is already cancelled.
    void on_cancel(std::function<void()> callback) {
        {
            std::lock_guard<std::mutex> lock(callbacks_mutex_);
            if (!cancelled_.load(std::memory_order_acquire)) {
                callbacks_.push_back(std::move(callback));
                return;
            }
        }
        callback();
    }

private:
    std::atomic<bool> cancelled_{false};
    std::mutex callbacks_mutex_;
    std::vector<std::function<void()>> callbacks_;
};

// Scoped helper: cancels the token when the guard dies (RAII), used to
// guarantee that jobs submitted to a worker are cancelled on teardown.
class CancelOnDestroy {
public:
    explicit CancelOnDestroy(CancellationToken& token) : token_(token) {}
    CancelOnDestroy(const CancelOnDestroy&) = delete;
    CancelOnDestroy& operator=(const CancelOnDestroy&) = delete;
    ~CancelOnDestroy() { token_.cancel(); }

private:
    CancellationToken& token_;
};

} // namespace veltrix::core
