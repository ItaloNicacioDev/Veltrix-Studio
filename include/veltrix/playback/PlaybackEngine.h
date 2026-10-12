// Veltrix Playback - PlaybackEngine.h
//
// CONTRACT + minimal state machine. The playback engine coordinates
// video/audio scheduling; the real-time clock loop is NOT implemented in
// this stage (no audio device integration). What exists and is tested:
// the playback state machine (states + legal transitions) shared by any
// future implementation.
//
// Status: state machine implemented and tested; frame scheduling, audio
// clock and buffering are planned (docs/ROADMAP.md).

#pragma once

#include <cstdint>

#include "veltrix/core/Result.h"

namespace veltrix::playback {

enum class PlayState : std::uint8_t {
    Stopped = 0,
    Playing,
    Paused,
};

class PlaybackStateMachine {
public:
    PlayState state() const { return state_; }

    // Legal transitions (STRICT - the UI layer may make them idempotent):
    //   play : Stopped|Paused -> Playing (error when already Playing)
    //   pause: Playing -> Paused (error otherwise)
    //   stop : any -> Stopped
    veltrix::core::ResultVoid play() {
        if (state_ == PlayState::Playing)
            return veltrix::core::failure(veltrix::core::ErrorCode::Conflict,
                                           "already playing");
        state_ = PlayState::Playing;
        return veltrix::core::success();
    }

    veltrix::core::ResultVoid pause() {
        if (state_ != PlayState::Playing)
            return veltrix::core::failure(veltrix::core::ErrorCode::Conflict,
                                           "can only pause while playing");
        state_ = PlayState::Paused;
        return veltrix::core::success();
    }

    veltrix::core::ResultVoid stop() {
        state_ = PlayState::Stopped;
        return veltrix::core::success();
    }

private:
    PlayState state_ = PlayState::Stopped;
};

} // namespace veltrix::playback
