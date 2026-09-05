#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickWindow>
#include <QDir>
#include <QFile>
#include <QProcessEnvironment>
#include <QDebug>
#include "fueltracker.h"

int main(int argc, char *argv[])
{
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QGuiApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("fuel-tracker"));
    // Keine Organization setzen: Dadurch löst AppDataLocation (QDirs) zu
    // <XDG_DATA_HOME>/fuel-tracker auf, der von der Click-Apparmor-Whitelist
    // erlaubte Datapath (~/.local/share/fuel-tracker/**).
    QCoreApplication::setOrganizationName(QString());

    QQmlApplicationEngine engine;

    // Desktop-Override per Umgebungsvariable; sonst QML-Pfad von bin/ aus finden.
    QString qmlPath;
    QString overridePath = QProcessEnvironment::systemEnvironment().value("FUELTRACKER_QML");
    if (!overridePath.isEmpty()) {
        qmlPath = overridePath;
    } else {
        QDir dir(QCoreApplication::applicationDirPath());
        qmlPath.clear();
        for (int i = 0; i < 5 && qmlPath.isEmpty(); ++i) {
            QString candidate = dir.absoluteFilePath("qml/Main.qml");
            if (QFile::exists(candidate)) {
                qmlPath = candidate;
            }
            dir.cdUp();
        }
        if (qmlPath.isEmpty()) {
            qmlPath = QDir(QCoreApplication::applicationDirPath())
                          .absoluteFilePath("../qml/Main.qml");
        }
    }

    FuelTracker tracker;
    engine.rootContext()->setContextProperty("fuelTracker", &tracker);

    engine.load(QUrl::fromLocalFile(qmlPath));
    if (engine.rootObjects().isEmpty()) {
        qCritical() << "QML nicht geladen:" << qmlPath;
        return -1;
    }

    return app.exec();
}