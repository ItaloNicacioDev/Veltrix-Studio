# Interface/README.md — the graphical interface of Veltrix Studio

ALL QML/Qt Quick interface files live in THIS directory (project rule).

```
Interface/
├── app/                C++ bridge + app entry (compiled into veltrix-studio)
│   ├── main.cpp              QGuiApplication, engine wiring, QML loading
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

## Build

The app target `veltrix-studio` is configured by the main CMake when a
Qt6 SDK is found (`CMAKE_PREFIX_PATH` or `VELTRIX_QT_DIR`):

```
cmake --preset win-msvc-release -DVELTRIX_QT_DIR=D:/.../qt6_design_studio_reduced_version
cmake --build --preset win-msvc-release
```

Without Qt the target is skipped (CLI/tests/core unaffected).

Runtime: `windeployqt` or PATH must expose the Qt DLLs; decode needs the
libav*.dll set next to the exe (same as veltrix-cli).
