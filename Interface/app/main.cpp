// Veltrix UI - main.cpp
//
// Graphical Veltrix Studio (Qt Quick/QML). All QML lives in Interface/qml
// (loaded from the Interface directory per the project rules); this main
// only wires the C++ bridge (PreviewController) to the QML engine.
//
// The decode engine (veltrix-ffmpeg) is optional: without it the app
// still runs and reports "no decode engine" (documented degradation).

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QTimer>
#include <QtPlugin>

#include <cstdio>
#include <filesystem>
#include <memory>

#include "CrashHandler.h"
#include "FrameImageProvider.h"
#include "PreviewController.h"

#include "veltrix/playback/Decoder.h"

#ifdef VELTRIX_HAS_FFMPEG
#include "media/ffmpeg/AvDecoderFactory.h"
#endif

namespace {

using veltrix::ui::PreviewController;

#ifdef VELTRIX_HAS_FFMPEG
class AvDecoderFactoryProvider final
    : public veltrix::ui::IDecoderFactoryProvider {
public:
    veltrix::core::Result<std::unique_ptr<veltrix::playback::IDecoderFactory>>
    create_factory() override {
        return veltrix::core::Result<
            std::unique_ptr<veltrix::playback::IDecoderFactory>>(
            std::make_unique<veltrix::media::ffmpeg::AvDecoderFactory>());
    }
};
#endif

} // namespace

int main(int argc, char* argv[]) {
    install_crash_handler();
    QGuiApplication app(argc, argv);
    app.setOrganizationName(QStringLiteral("Veltrix"));
    app.setApplicationName(QStringLiteral("Veltrix Studio"));
    QQuickStyle::setStyle(QStringLiteral("Material"));

    // Smoke mode: --smoke <media> [position_seconds] — decode one frame
    // through the FULL UI pipeline (controller + jobs + requestor + QML
    // image provider wiring), print evidence, exit 0/1. Never shows a
    // window; used by the automated verification.
    bool smoke = false;
    QString smoke_media;
    double smoke_pos = 1.0;
    for (int i = 1; i < argc; ++i) {
        const QString arg = QString::fromLocal8Bit(argv[i]);
        if (arg == QStringLiteral("--smoke")) smoke = true;
        else if (!arg.startsWith(QLatin1String("--")) && smoke_media.isEmpty())
            smoke_media = arg;
        else if (arg.startsWith(QLatin1String("--pos=")))
            smoke_pos = arg.mid(6).toDouble();
    }

    PreviewController controller;
#ifdef VELTRIX_HAS_FFMPEG
    AvDecoderFactoryProvider factory_provider;
    controller.set_factory_provider(&factory_provider);
#endif

    auto* frame_provider = new veltrix::ui::FrameImageProvider();
    QObject::connect(&controller, &PreviewController::frameReady,
                     [frame_provider](const QImage& frame, qint64) {
                         if (frame.isNull()) frame_provider->clear();
                         else frame_provider->setFrame(frame);
                     });

    QQmlApplicationEngine engine;
    engine.addImageProvider(QStringLiteral("veltrix-frame"), frame_provider);
    engine.rootContext()->setContextProperty(QStringLiteral("previewController"),
                                             &controller);

    // QML lives in Interface/qml — resolve relative to the executable,
    // then fall back to the source tree layout (dev runs from build dir).
    namespace fs = std::filesystem;
    fs::path qml_dir;
    const fs::path exe_dir = fs::path(argv[0]).parent_path();
    for (const fs::path& candidate : {exe_dir / "Interface" / "qml",
                                      exe_dir.parent_path() / "Interface" / "qml",
                                      exe_dir.parent_path().parent_path() /
                                          "Interface" / "qml"}) {
        if (fs::exists(candidate / "Main.qml")) {
            qml_dir = candidate;
            break;
        }
    }
    if (qml_dir.empty()) {
        qWarning() << "Interface/qml/Main.qml not found (looked near"
                   << QString::fromStdString(exe_dir.string()) << ")";
        return 1;
    }

    QObject::connect(
        &engine, &QQmlApplicationEngine::objectCreated, &app,
        [url = QUrl::fromLocalFile(QString::fromStdString((qml_dir / "Main.qml").string()))]
        (QObject* obj, const QUrl& obj_url) {
            if (!obj && url == obj_url)
                QCoreApplication::exit(1);
        }, Qt::QueuedConnection);

    engine.load(QUrl::fromLocalFile(
        QString::fromStdString((qml_dir / "Main.qml").string())));
    if (engine.rootObjects().isEmpty()) {
        qWarning() << "Failed to load Main.qml";
        return 1;
    }

    if (smoke) {
        // ---- automated verification (no window interaction) --------------
        // Prove: QML loaded from Interface/, engine wired, and one real
        // frame decoded through the same pipeline the UI uses.
        std::printf("SMOKE qml_loaded=yes\n");
        std::printf("SMOKE engine=%s\n", controller.engineName().toUtf8().constData());
        if (smoke_media.isEmpty()) {
            std::printf("SMOKE no media given\n");
            return 1;
        }
        QObject::connect(&controller, &PreviewController::frameReady,
                         &app, [&app](const QImage& frame, qint64 pts) {
            if (frame.isNull()) {
                std::printf("SMOKE frame=EMPTY\n");
                QCoreApplication::exit(3);
                return;
            }
            std::printf("SMOKE frame=%dx%d pts=%lld bytes=%lld\n",
                        frame.width(), frame.height(),
                        static_cast<long long>(pts),
                        static_cast<long long>(frame.sizeInBytes()));
            QCoreApplication::exit(0);
        });
        QObject::connect(&controller, &PreviewController::statusChanged,
                         &app, []() {}); // keep status flowing
        QTimer::singleShot(30000, &app, []() {
            std::printf("SMOKE timeout\n");
            QCoreApplication::exit(4);
        });
        // Note: QTimer::singleShot with a context object takes a callable
        // with zero args; the lambda above satisfies it.
        controller.openMedia(smoke_media);
        controller.requestFrameAt(smoke_pos);
        const int code = app.exec();
        std::printf("SMOKE exit=%d\n", code);
        return code;
    }
    return app.exec();
}
