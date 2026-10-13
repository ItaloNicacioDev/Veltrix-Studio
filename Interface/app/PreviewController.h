// Veltrix UI - PreviewController.h
//
// QML bridge for video preview (Phase: Interface). Lives in the UI layer
// per the architecture rules: it TRANSLATES between QML-friendly types and
// the core contracts; all decode logic stays in veltrix-core/veltrix-ffmpeg
// (FrameRequestor, MediaRegistry). No FFmpeg types here — only the public
// contracts from include/veltrix/.
//
// Threading contract:
//   * FrameRequestor::await_frame decodes on the CALLING thread; the QML
//     UI thread must NOT decode. PreviewController runs requests on the
//     JobManager worker (single worker — one preview request at a time,
//     which is exactly the requestor's contract) and delivers the result
//     back to the QML thread via a queued signal.
//   * Stale-request handling comes free: request_frame() cancels the
//     previous request; the requestor rejects superseded handles.

#pragma once

#include <QImage>
#include <QObject>
#include <QString>
#include <QStringList>

#include <memory>

#include "veltrix/jobs/JobManager.h"
#include "veltrix/media/MediaRegistry.h"
#include "veltrix/playback/FrameRequestor.h"

namespace veltrix::ui {

// Produces IDecoderFactory instances for the UI layer (implemented by the
// ffmpeg-capable app target; null when the build has no decode engine).
class IDecoderFactoryProvider {
public:
    virtual ~IDecoderFactoryProvider() = default;
    virtual veltrix::core::Result<std::unique_ptr<veltrix::playback::IDecoderFactory>>
    create_factory() = 0;
};

class PreviewController : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString mediaPath READ mediaPath WRITE setMediaPath NOTIFY mediaChanged)
    Q_PROPERTY(bool decoding READ decoding NOTIFY decodingChanged)
    Q_PROPERTY(QString statusMessage READ statusMessage NOTIFY statusChanged)
    Q_PROPERTY(QString engineName READ engineName CONSTANT)

public:
    explicit PreviewController(QObject* parent = nullptr);
    ~PreviewController() override;

    // Dependency injection (no globals): called once at app startup.
    void set_factory_provider(IDecoderFactoryProvider* provider);

    QString mediaPath() const { return media_path_; }
    void setMediaPath(const QString& path);
    bool decoding() const { return decoding_; }
    const QString& statusMessage() const { return status_; }
    QString engineName() const;

    // Q_INVOKABLE so QML can drive it without knowing C++ types.
    // positionSeconds: timeline position to preview (in media time).
    Q_INVOKABLE void requestFrameAt(double position_seconds);
    Q_INVOKABLE void openMedia(const QString& path);
    Q_INVOKABLE void clearMedia();

    // Last decoded frame as QImage (QML reads via image provider or the
    // frameReady signal argument).
    QImage lastFrame() const { return last_frame_; }

signals:
    void mediaChanged();
    void decodingChanged();
    void statusChanged();
    // frameReady carries the decoded frame to the QML view; empty QImage
    // means "no frame" (e.g. cleared).
    void frameReady(const QImage& frame, qint64 ptsTicks);
    void engineInfo(const QString& engine);
    void mediaInfo(const QString& summary);

public slots:
    // Called by the job wrapper when the decode finishes.
    void onDecodeFinished();

private:
    void set_status(const QString& message);
    void set_decoding(bool value);
    std::shared_ptr<veltrix::playback::FrameRequestHandle> current_handle_;
    std::shared_ptr<veltrix::core::CancellationToken> current_token_;

    QString media_path_;
    bool decoding_ = false;
    QString status_;
    QImage last_frame_;

    std::unique_ptr<veltrix::jobs::JobManager> jobs_;
    std::unique_ptr<veltrix::media::MediaRegistry> registry_;
    std::unique_ptr<veltrix::media::MediaPathResolver> resolver_;
    std::unique_ptr<veltrix::playback::FrameRequestor> requestor_;
    IDecoderFactoryProvider* factory_provider_ = nullptr;

    // Decoded result handoff from worker -> QML thread.
    veltrix::playback::VideoFrame pending_frame_;
    bool has_pending_frame_ = false;
    bool decode_failed_ = false;
    QString decode_error_;
};

} // namespace veltrix::ui
