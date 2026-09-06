#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QtQml>
#include "lcmp_bridge.h"

int main(int argc, char *argv[]) {
    QGuiApplication app(argc, argv);

    qmlRegisterType<LcmpBridge>("lcmp.gui", 1, 0, "LcmpBridge");

    QQmlApplicationEngine engine;
    engine.loadFromModule("lcmp.gui", "Main");

    return app.exec();
}