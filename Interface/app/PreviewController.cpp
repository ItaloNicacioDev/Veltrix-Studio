// Veltrix UI - PreviewController.cpp

#include "PreviewController.h"

#include <QMetaObject>
#include <QThread>

#include "veltrix/core/CancellationToken.h"
#include "veltrix/core/Result.h"
#include "veltrix/media/MediaPathResolver.h"
#include "veltrix/media/MediaProber.h"
#include "veltrix/playback/Decoder.h"
#include "veltrix/playback/MediaFrames.h"

namespace veltrix::ui {

using veltrix::core::ErrorCode;
using veltrix::playback::FrameRequestHandle;

PreviewController::PreviewController(QObject* parent) : QObject(parent) {
    registry_ = std::make_unique<veltrix::media::MediaRegistry>();
    jobs_ = std::make_unique<veltrix::jobs::JobManager>();
}

PreviewController::~PreviewController() {
    // Jobs must finish before members die (JobManager joins its worker on
    // destruction; the requestor/registry outlive the worker by order).
    if (jobs_) jobs_->shutdown();
}

void PreviewController::set_factory_provider(IDecoderFactoryProvider* provider) {
    factory_provider_ = provider;
    if (provider) {
        auto factory = provider->create_factory();
        if (factory.is_ok()) {
            resolver_ = std::make_unique<veltrix::media::MediaPathResolver>(*registry_);
            requestor_ = std::make_unique<veltrix::playback::FrameRequestor>(
                *factory.take_value(), *resolver_);
            emit engineInfo(QString::fromUtf8(engineName()));
        } else {
            set_status(QString::fromUtf8(factory.error().to_string().c_str()));
        }
    }
}

QString PreviewController::engineName() const {
    return requestor_ ? QString::fromUtf8(requestor_->engine_name()) : QStringLiteral("none");
}

void PreviewController::setMediaPath(const QString& path) {
    if (media_path_ == path) return;
    media_path_ = path;
    emit mediaChanged();
    if (!path.isEmpty()) registerMedia(path);
}

void PreviewController::openMedia(const QString& path) {
    setMediaPath(path);
}

void PreviewController::clearMedia() {
    setMediaPath(QString());
    last_frame_ = QImage();
    emit frameReady(QImage(), 0);
}

void PreviewController::registerMedia(const QString& path) {
    // Convert QString (UTF-16) -> std::string (UTF-8): the core consumes UTF-8.
    const std::string utf8_path = path.toStdString();
    auto existing = registry_->find_by_path(utf8_path);
    veltrix::core::MediaId id{};
    if (existing) {
        id = existing->id;
    } else {
        auto registered = registry_->register_media(utf8_path);
        if (registered.is_error()) {
            set_status(QString::fromUtf8(registered.error().to_string().c_str()));
            return;
        }
        id = registered.value();
    }

    // Probe for the info summary (metadata path — external ffprobe).
    auto located = veltrix::media::MediaProber::locate_ffprobe("");
    if (located.is_ok()) {
        veltrix::media::MediaItem temp_item;
        temp_item.path = utf8_path;
        auto* item = registry_->find(id);
        if (item) {
            auto probed = veltrix::media::MediaProber::probe(*item, located.value(), 10);
            QString summary;
            if (probed.is_ok()) {
                summary = QString::fromStdString(item->display_name) + QStringLiteral("  ·  ");
                bool first = true;
                for (const auto& s : item->streams) {
                    if (!first) summary += QStringLiteral(", ");
                    first = false;
                    if (s.width > 0)
                        summary += QString("%1x%2").arg(s.width).arg(s.height);
                    else if (s.sample_rate > 0)
                        summary += QString("%1 Hz").arg(s.sample_rate);
                }
            } else {
                summary = QString::fromStdString(item->display_name);
            }
            emit mediaInfo(summary);
        }
    }
    set_status(QStringLiteral("ready"));
}

void PreviewController::requestFrameAt(double position_seconds) {
    if (!requestor_ || media_path_.isEmpty()) {
        set_status(media_path_.isEmpty() ? QStringLiteral("no media loaded")
                                         : QStringLiteral("no decode engine"));
        return;
    }
    const std::string utf8_path = media_path_.toStdString();
    const veltrix::core::MediaId* id_ptr = nullptr;
    veltrix::core::MediaId media_id{};
    if (auto* item = registry_->find_by_path(utf8_path)) media_id = item->id;
    else {
        set_status(QStringLiteral("media not registered"));
        return;
    }

    set_decoding(true);
    // Position in MEDIA ticks: the requestor interprets ticks in the
    // media's video timebase. We do not know the fps here — the requestor
    // receives SECONDS converted via the decoder timebase inside
    // await_frame's seek... The FrameRequestor takes ticks; the UI passes
    // position * fps. Since the registry probe stored stream fps, use it.
    double fps = 0.0;
    if (const auto* item = registry_->find(media_id)) {
        for (const auto& s : item->streams) {
            if (s.fps_den > 0 && s.fps_num > 0 && (s.width > 0)) {
                fps = static_cast<double>(s.fps_num) / s.fps_den;
                break;
            }
        }
    }
    const std::int64_t ticks =
        fps > 0.0 ? static_cast<std::int64_t>(position_seconds * fps) : 0;

    auto handle = std::make_shared<FrameRequestHandle>(
        requestor_->request_frame(media_id, ticks));
    current_handle_ = handle;
    current_token_ = handle->token;

    // Run the await on the job worker; deliver via queued signal.
    const qint64 request_id = static_cast<qint64>(handle->id);
    auto job = std::make_shared<veltrix::jobs::FunctionJob>(
        QStringLiteral("preview-decode").toStdString(), veltrix::jobs::JobKind::Decode,
        [this, handle, request_id](veltrix::core::CancellationToken&) -> veltrix::core::ResultVoid {
            auto frame = requestor_->await_frame(*handle);
            // Hand the result to the QML thread.
            QMetaObject::invokeMethod(this, [this, frame = std::move(frame)]() mutable {
                if (frame.is_error()) {
                    decode_failed_ = true;
                    decode_error_ = QString::fromUtf8(frame.error().to_string().c_str());
                } else {
                    pending_frame_ = frame.take_value();
                    has_pending_frame_ = true;
                }
                onDecodeFinished();
            }, Qt::QueuedConnection);
            return veltrix::core::success();
        });
    (void)request_id;
    jobs_->submit(job);
}

void PreviewController::onDecodeFinished() {
    set_decoding(false);
    if (has_pending_frame_) {
        const auto& vf = pending_frame_;
        QImage image(vf.data->bytes.data(), vf.width, vf.height,
                     vf.width * 4, QImage::Format_RGBA8888);
        last_frame_ = image.copy(); // detach from the core buffer
        has_pending_frame_ = false;
        emit frameReady(last_frame_, vf.pts_ticks);
        set_status(QStringLiteral("frame decoded"));
    } else if (decode_failed_) {
        set_status(QStringLiteral("decode: ") + decode_error_);
        decode_failed_ = false;
    }
}

void PreviewController::set_status(const QString& message) {
    if (status_ == message) return;
    status_ = message;
    emit statusChanged();
}

void PreviewController::set_decoding(bool value) {
    if (decoding_ == value) return;
    decoding_ = value;
    emit decodingChanged();
}

} // namespace veltrix::ui
