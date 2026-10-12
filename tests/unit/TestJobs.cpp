// Unit tests - job system: states, cancellation, error propagation.

#include <catch2/catch.hpp>

#include <chrono>
#include <atomic>
#include <thread>

#include "veltrix/jobs/JobManager.h"

using namespace veltrix;

TEST_CASE("Job manager runs a successful job", "[jobs]") {
    jobs::JobManager manager;
    std::atomic<bool> ran{false};
    auto job = std::make_shared<jobs::FunctionJob>(
        "success", jobs::JobKind::Custom,
        [&](core::CancellationToken&) -> core::ResultVoid {
            ran.store(true);
            return core::success();
        });
    REQUIRE(manager.submit(job).is_ok());
    manager.wait_all();
    REQUIRE(ran.load());
    REQUIRE(job->state() == jobs::JobState::Succeeded);
    manager.shutdown();
}

TEST_CASE("Job failure propagates the error", "[jobs]") {
    jobs::JobManager manager;
    std::atomic<std::uint8_t> state{0};
    auto job = std::make_shared<jobs::FunctionJob>(
        "fail", jobs::JobKind::Custom,
        [&](core::CancellationToken& t) -> core::ResultVoid {
            (void)t;
            state.store(1);
            return core::failure(core::ErrorCode::IoError, "boom");
        });
    REQUIRE(manager.submit(job).is_ok());
    manager.wait_all();
    REQUIRE(job->state() == jobs::JobState::Failed);
    manager.shutdown();
}

TEST_CASE("Cancellation mid-run is observed", "[jobs]") {
    jobs::JobManager manager;
    auto job = std::make_shared<jobs::FunctionJob>(
        "cancel", jobs::JobKind::Custom,
        [&](core::CancellationToken& token) -> core::ResultVoid {
            for (int i = 0; i < 200; ++i) {
                if (token.is_cancelled())
                    return core::failure(core::ErrorCode::Cancelled, "cancelled");
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
            }
            return core::success();
        });
    REQUIRE(manager.submit(job).is_ok());
    // Wait until the job is observed running, then cancel it.
    for (int spin = 0; spin < 400 && job->state() != jobs::JobState::Running; ++spin)
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    manager.cancel(job->id());
    manager.wait_all();
    REQUIRE(job->state() == jobs::JobState::Cancelled);
    manager.shutdown();
}

TEST_CASE("Cancelling a queued job never runs it", "[jobs]") {
    jobs::JobManager manager;
    // Block the single worker with a slow first job.
    std::atomic<bool> release{false};
    auto blocker = std::make_shared<jobs::FunctionJob>(
        "blocker", jobs::JobKind::Custom,
        [&](core::CancellationToken& token) -> core::ResultVoid {
            while (!release.load() && !token.is_cancelled())
                std::this_thread::sleep_for(std::chrono::milliseconds(2));
            return core::success();
        });
    REQUIRE(manager.submit(blocker).is_ok());
    std::atomic<bool> second_ran{false};
    auto second = std::make_shared<jobs::FunctionJob>(
        "second", jobs::JobKind::Custom,
        [&](core::CancellationToken& token) -> core::ResultVoid {
            (void)token;
            second_ran.store(true);
            return core::success();
        });
    REQUIRE(manager.submit(second).is_ok());
    manager.cancel(second->id());
    release.store(true);
    manager.wait_all();
    REQUIRE_FALSE(second_ran.load());
    REQUIRE(second->state() == jobs::JobState::Cancelled);
    REQUIRE(blocker->state() == jobs::JobState::Succeeded);
    manager.shutdown();
}

TEST_CASE("Shutdown cancels queued jobs", "[jobs]") {
    jobs::JobManager manager;
    std::atomic<bool> release{false};
    auto blocker = std::make_shared<jobs::FunctionJob>(
        "blocker", jobs::JobKind::Custom,
        [&](core::CancellationToken& token) -> core::ResultVoid {
            while (!release.load() && !token.is_cancelled())
                std::this_thread::sleep_for(std::chrono::milliseconds(2));
            return core::success();
        });
    auto queued = std::make_shared<jobs::FunctionJob>(
        "queued", jobs::JobKind::Custom,
        [&](core::CancellationToken&) -> core::ResultVoid {
            return core::success();
        });
    REQUIRE(manager.submit(blocker).is_ok());
    REQUIRE(manager.submit(queued).is_ok());
    // Shutdown while the blocker still holds the worker: `queued` must be
    // cancelled, never run.
    manager.shutdown();
    REQUIRE(queued->state() == jobs::JobState::Cancelled);
    release.store(true);
    // The destructor joins the worker after the blocker observes release.
}

TEST_CASE("Submit validates the job", "[jobs]") {
    jobs::JobManager manager;
    REQUIRE(manager.submit(nullptr).is_error());
    auto job = std::make_shared<jobs::FunctionJob>(
        "x", jobs::JobKind::Custom,
        [](core::CancellationToken&) -> core::ResultVoid { return core::success(); });
    REQUIRE(manager.submit(job).is_ok());
    manager.wait_all();
    // Re-submitting a finished job is refused.
    REQUIRE(manager.submit(job).is_error());
    manager.shutdown();
}

TEST_CASE("Progress clamps to [0,1]", "[jobs]") {
    jobs::JobManager manager;
    auto job = std::make_shared<jobs::FunctionJob>(
        "p", jobs::JobKind::Custom,
        [](core::CancellationToken&) -> core::ResultVoid { return core::success(); });
    job->progress().set(2.0f);
    REQUIRE(job->progress().get() == 1.0f);
    job->progress().set(-3.0f);
    REQUIRE(job->progress().get() == 0.0f);
    manager.shutdown();
}
