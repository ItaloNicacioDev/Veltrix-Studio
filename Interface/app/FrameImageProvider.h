// Veltrix UI - FrameImageProvider.h
//
// QQuickImageProvider that serves the last decoded frame to QML
// ("image://veltrix-frame/latest"). The QImage is copied on request; the
// core buffer ownership stays with PreviewController.

#pragma once

#include <QImage>
#include <QQuickImageProvider>

#include <atomic>
#include <mutex>

namespace veltrix::ui {

class FrameImageProvider : public QQuickImageProvider {
public:
    FrameImageProvider() : QQuickImageProvider(QQuickImageProvider::Image) {}

    void setFrame(const QImage& frame) {
        const std::lock_guard<std::mutex> lock(mutex_);
        frame_ = frame;
        has_frame_ = true;
    }
    void clear() {
        const std::lock_guard<std::mutex> lock(mutex_);
        frame_ = QImage();
        has_frame_ = false;
    }

    QImage requestImage(const QString& id, QSize* size,
                        const QSize& requested_size) override {
        const std::lock_guard<std::mutex> lock(mutex_);
        (void)id;
        if (!has_frame_ || frame_.isNull()) {
            if (size) *size = QSize(0, 0);
            return QImage();
        }
        if (size) *size = frame_.size();
        if (requested_size.isValid())
            return frame_.scaled(requested_size, Qt::KeepAspectRatio,
                                  Qt::SmoothTransformation);
        return frame_;
    }

private:
    std::mutex mutex_;
    QImage frame_;
    bool has_frame_ = false;
};

} // namespace veltrix::ui
