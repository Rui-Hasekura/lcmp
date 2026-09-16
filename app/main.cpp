#include <QCoreApplication>
#include <QGuiApplication>
#include <QLocale>
#include <QQmlApplicationEngine>
#include <QTranslator>

int main(int argc, char* argv[])
{
  QGuiApplication app(argc, argv);

  QTranslator translator;

  const QString locale = QLocale::system().name();

  const QString translationPath =
    QCoreApplication::applicationDirPath()
    + "/lcmp_" + locale + ".qm";

  if (translator.load(translationPath)) {
    app.installTranslator(&translator);
  }

  QQmlApplicationEngine engine;

  QObject::connect(
    &engine,
    &QQmlApplicationEngine::objectCreationFailed,
    &app,
    []() {
      QCoreApplication::exit(-1);
    },
    Qt::QueuedConnection
  );

  engine.loadFromModule("lcmp.gui", "Main");

  return app.exec();
}