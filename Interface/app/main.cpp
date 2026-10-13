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
#include <QtPlugin>

#include <filesystem>
#include <memory>

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
    QGuiApplication app(argc, argv);
    app.setOrganizationName(QStringLiteral("Veltrix"));
    app.setApplicationName(QStringLiteral("Veltrix Studio"));
    QQuickStyle::setStyle(QStringLiteral("Material"));

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
    return app.exec();
}
