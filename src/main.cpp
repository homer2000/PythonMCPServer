#include <QCoreApplication>
#include <QCommandLineParser>
#include "PythonMCPServer.h"

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    app.setApplicationName("PythonMCPServer");
    app.setApplicationVersion("1.0.0");

    QCommandLineParser parser;
    parser.setApplicationDescription(
        "MCP-сервер для создания и запуска Python-скриптов (stdio transport).");
    parser.addHelpOption();
    parser.addVersionOption();

    QCommandLineOption workDirOpt(
        { "d", "work-dir" },
        "Рабочая директория для скриптов и окружений (по умолчанию: текущая директория).",
        "path");
    QCommandLineOption debugOpt(
        { "v", "verbose" },
        "Включить подробное логирование в stderr.");

    parser.addOption(workDirOpt);
    parser.addOption(debugOpt);
    parser.process(app);

    PythonMCPServer server;

    if (parser.isSet(debugOpt))
        server.setDebug(true);

    if (parser.isSet(workDirOpt))
        server.setWorkDir(parser.value(workDirOpt));

    server.start();
    return app.exec();
}
