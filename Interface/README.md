# Interface/README.md — the graphical interface of Veltrix Studio

ALL QML/Qt Quick interface files live in THIS directory (project rule).

```
Interface/
├── app/                C++ bridge + app entry (compiled into veltrix-studio)
│   ├── main.cpp              QGuiApplication, engine wiring, QML loading
│   │                         (+ --smoke automated verification mode)
│   ├── PreviewController.h/.cpp   QML bridge: media open/registry, jobs,
│   │                          FrameRequestor calls, QImage handoff
│   └── FrameImageProvider.h  image://veltrix-frame provider
├── qml/
│   └── Main.qml              main window (open media, scrub, decode, preview)
└── README.md
```

## Architecture rules honored here

- **No decode logic in QML.** QML calls `previewController` (Q_INVOKABLE +
  properties). PreviewController translates to core APIs:
  `MediaRegistry` (register/probe), `jobs::JobManager` (decode off the UI
  thread), `playback::FrameRequestor` (media-id + ticks → RGBA frame,
  stale-rejection, cancellation). The FFmpeg layer stays in
  `src/media/ffmpeg/` (ADR-008) — nothing here includes AV* headers.
- **Threading**: `await_frame` runs on the JobManager worker; the result
  crosses to the QML thread via a queued `frameReady(QImage, pts)` signal.
  Superseded seeks are rejected by the requestor (Conflict) — the UI can
  spam the slider without stale frames overwriting newer ones.
- **Timebase**: the UI works in SECONDS; conversion to decoder ticks uses
  the probed stream timebase (`StreamInfo::timebase_*`) — never a guessed
  fps. Verified: --pos=2.0 returns pts=24576 (= 2.0 s × 12288).

## Where the app loads QML from

The executable resolves `Interface/qml/Main.qml` next to itself
(build-time POST_BUILD copy from THIS directory — the source of truth).
Editing `Interface/qml/Main.qml` and rebuilding always updates what the
app runs.

## Build

Configured by the root CMake when a Qt6 SDK is found
(`VELTRIX_QT_DIR` or `CMAKE_PREFIX_PATH`):

```
cmake --preset win-msvc-release -DVELTRIX_QT_DIR=<qt-sdk>
cmake --build --preset win-msvc-release
build/win-msvc-release/Interface/veltrix-studio.exe
```

Without Qt the target is skipped (core/CLI/tests unaffected). Runtime
needs the Qt DLLs (`windeployqt --qmldir Interface/qml`) and the
libav*.dll set next to the exe.

## Automated verification (no GUI interaction)

```
veltrix-studio.exe --smoke <media-file> --pos=<seconds>
```
Proves QML loading + engine wiring + a real decode through the same
pipeline the UI uses; prints `SMOKE frame=WxH pts=...` and exits 0.
Verified on this machine:
```
SMOKE qml_loaded=yes
SMOKE engine=libav format=n9.0.2 (avformat 4129126, ...)
SMOKE frame=320x240 pts=24576 bytes=307200   (pos=2.0 s)
SMOKE exit=0
```

## Note on this machine's Qt

The pre-existing Qt 6.8.7 "Design Studio reduced" kit is RUNTIME-ONLY
(no C++ headers, missing import libs and QML plugin binaries), so the
interface builds against the open-source Qt 6.8.3 SDK
(aqtinstall, win64_msvc2022_64). Both are LGPL; no paid components.
