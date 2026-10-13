#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>

#include "video_utils.h"
#include "fire_crop.h"
#include "video_variance.h"

class VideoProcessorBackend : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString status READ status NOTIFY statusChanged)

public:
    explicit VideoProcessorBackend(QObject* parent = nullptr) : QObject(parent) {}

    QString status() const { return m_status; }

    Q_INVOKABLE void extractFrames(const QString& inputPath, const QString& outputDir,
                                    int fps, const QString& startTime, const QString& endTime) {
        m_status = "Extracting frames...";
        emit statusChanged();

        bool success = extractFramesAtFPS(inputPath.toStdString(), outputDir.toStdString(),
                                           fps, startTime.toStdString(), endTime.toStdString());
        m_status = success ? "Frames extracted successfully!" : "Frame extraction failed.";
        emit statusChanged();
    }

    Q_INVOKABLE void extractFireCrop(const QString& inputPath, const QString& outputDir,
                                      int fps, const QString& startTime, const QString& endTime) {
        m_status = "Extracting fire crops...";
        emit statusChanged();

        bool success = extractFireCrop(inputPath.toStdString(), outputDir.toStdString(),
                                        startTime.toStdString(), endTime.toStdString(), fps);
        m_status = success ? "Fire crops extracted!" : "Fire crop extraction failed.";
        emit statusChanged();
    }

    Q_INVOKABLE void analyzeVariance(const QString& inputPath, const QString& startTime,
                                      const QString& endTime, int n, int m) {
        m_status = "Analyzing variance...";
        emit statusChanged();

        auto results = analyzeVideoVariance(inputPath.toStdString(), startTime.toStdString(),
                                             endTime.toStdString(), n, m);
        if (results.empty()) {
            m_status = "Variance analysis failed.";
        } else {
            QString msg = "Variance results:\n";
            for (const auto& r : results) {
                msg += QString("Sample %1: %2%\n").arg(r.sampleNumber).arg(r.differencePercent, 0, 'f', 2);
            }
            m_status = msg;
        }
        emit statusChanged();
    }

signals:
    void statusChanged();

private:
    QString m_status = "Ready";
};

#include "main_gui.moc"

int main(int argc, char *argv[]) {
    QGuiApplication app(argc, argv);
    app.setApplicationName("Video Processor");
    app.setOrganizationName("MyCompany");

    QQuickStyle::setStyle("Material");

    qmlRegisterType<VideoProcessorBackend>("VideoProcessor", 1, 0, "VideoProcessorBackend");

    QQmlApplicationEngine engine;
    VideoProcessorBackend backend;
    engine.rootContext()->setContextProperty("backend", &backend);

    const QUrl url(QStringLiteral("qrc:/main.qml"));
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed,
                     &app, []() { QCoreApplication::exit(-1); },
                     Qt::QueuedConnection);
    engine.load(url);

    return app.exec();
}