#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QPalette>
#include <QColor>

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

        bool success = ::extractFramesAtFPS(inputPath.toStdString(), outputDir.toStdString(),
                                            fps, startTime.toStdString(), endTime.toStdString());
        
        if (success) {
            m_status = "Frames extracted successfully!";
            emit operationFinished(true, m_status);
        } else {
            m_status = "Error: Frame extraction failed. Check the terminal for details.";
            emit operationFinished(false, m_status);
        }
        emit statusChanged();
    }

    Q_INVOKABLE void extractFireCrop(const QString& inputPath, const QString& outputDir,
                                      int fps, const QString& startTime, const QString& endTime) {
        m_status = "Extracting fire crops...";
        emit statusChanged();

        bool success = ::extractFireCrop(inputPath.toStdString(), outputDir.toStdString(),
                                         startTime.toStdString(), endTime.toStdString(), fps);
        
        if (success) {
            m_status = "Fire crops extracted!";
            emit operationFinished(true, m_status);
        } else {
            m_status = "Error: Fire crop extraction failed. Check the terminal for details.";
            emit operationFinished(false, m_status);
        }
        emit statusChanged();
    }

    Q_INVOKABLE void analyzeVariance(const QString& inputPath, const QString& startTime,
                                      const QString& endTime, int n, int m) {
        m_status = "Analyzing variance...";
        emit statusChanged();

        auto results = ::analyzeVideoVariance(inputPath.toStdString(), startTime.toStdString(),
                                              endTime.toStdString(), n, m);
        if (results.empty()) {
            m_status = "Error: Variance analysis failed. Check the terminal for details.";
            emit operationFinished(false, m_status);
        } else {
            QString msg = "Variance results:\n";
            for (const auto& r : results) {
                msg += QString("Sample %1: %2%\n").arg(r.sampleNumber).arg(r.differencePercent, 0, 'f', 2);
            }
            m_status = msg;
            emit operationFinished(true, "Variance analysis complete!");
        }
        emit statusChanged();
    }

signals:
    void statusChanged();
    void operationFinished(bool success, const QString& message);

private:
    QString m_status = "Ready";
};

int main(int argc, char *argv[]) {
    QGuiApplication app(argc, argv);
    app.setApplicationName("Video Processor");
    app.setOrganizationName("MyCompany");

    // ---------------------------------------------------------
    // APPLY DARK FUSION PALETTE (For File Dialogs and Widgets)
    // ---------------------------------------------------------
    QQuickStyle::setStyle("Fusion");

    QPalette darkPalette;
    darkPalette.setColor(QPalette::Window, QColor(30, 30, 30));
    darkPalette.setColor(QPalette::WindowText, Qt::white);
    darkPalette.setColor(QPalette::Base, QColor(37, 37, 38));
    darkPalette.setColor(QPalette::AlternateBase, QColor(45, 45, 48));
    darkPalette.setColor(QPalette::ToolTipBase, Qt::white);
    darkPalette.setColor(QPalette::ToolTipText, Qt::white);
    darkPalette.setColor(QPalette::Text, Qt::white);
    darkPalette.setColor(QPalette::Button, QColor(45, 45, 48));
    darkPalette.setColor(QPalette::ButtonText, Qt::white);
    darkPalette.setColor(QPalette::BrightText, Qt::red);
    darkPalette.setColor(QPalette::Link, QColor(42, 130, 218));
    darkPalette.setColor(QPalette::Highlight, QColor(42, 130, 218));
    darkPalette.setColor(QPalette::HighlightedText, Qt::black);
    darkPalette.setColor(QPalette::Disabled, QPalette::Text, QColor(127, 127, 127));
    darkPalette.setColor(QPalette::Disabled, QPalette::WindowText, QColor(127, 127, 127));
    app.setPalette(darkPalette);
    // ---------------------------------------------------------

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

#include "main_gui.moc"