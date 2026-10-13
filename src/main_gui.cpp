#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQuickStyle>

int main(int argc, char *argv[]) {
    // 1. Initialize Qt Application
    QGuiApplication app(argc, argv);
    app.setApplicationName("Video Processor");
    app.setOrganizationName("MyCompany");

    // Optional: Set a modern UI style (Material, Fusion, etc.)
    // QQuickStyle::setStyle("Material"); 

    // 2. Load the QML UI
    QQmlApplicationEngine engine;
    
    // Load the main QML file from the Qt Resource System
    const QUrl url(QStringLiteral("qrc:/main.qml"));
    
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed,
        &app, []() { QCoreApplication::exit(-1); },
        Qt::QueuedConnection);
        
    engine.load(url);

    // 3. Run the event loop
    return app.exec();
}