#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#endif

#include "EventEditor.h"
#include <QtWidgets/QApplication>
#include <QDir>
#include <QFileInfo>
#include <QTextStream>
#include <cstdio>

#ifdef _WIN32
static void attachConsole()
{
    if (!AttachConsole(ATTACH_PARENT_PROCESS))
    {
        AllocConsole();
    }

    FILE *consoleStream = nullptr;
    freopen_s(&consoleStream, "CONIN$", "r", stdin);
    freopen_s(&consoleStream, "CONOUT$", "w", stdout);
    freopen_s(&consoleStream, "CONOUT$", "w", stderr);
}
#endif

int main(int argc, char *argv[])
{
#ifdef _WIN32
    if (argc > 1)
    {
        attachConsole();
    }
#endif
    QApplication app(argc, argv);

    if (app.arguments().size() > 1)
    {
        QTextStream out(stdout);
        QTextStream err(stderr);
        const bool showHelp = app.arguments().size() == 2 &&
            (app.arguments().at(1) == QStringLiteral("--help") || app.arguments().at(1) == QStringLiteral("-h"));
        if (app.arguments().size() > 3 || showHelp)
        {
            out << QStringLiteral("Usage: EventEditor.exe <file.events.json> [destination.fest|destination-folder]\n")
                << QStringLiteral("Without a destination, the .fest is created beside the input file.\n");
            return showHelp ? 0 : 2;
        }

        const QString inputFile = app.arguments().at(1);
        if (!inputFile.endsWith(QStringLiteral(".events.json"), Qt::CaseInsensitive))
        {
            err << QStringLiteral("Error: expected an .events.json file: %1\n").arg(inputFile);
            return 2;
        }

        const QFileInfo inputInfo(inputFile);
        QString festFileName = inputInfo.fileName();
        festFileName.chop(QStringLiteral(".events.json").size());
        festFileName += QStringLiteral(".fest");
        QString outputFile = inputInfo.absolutePath() + QLatin1Char('/') + festFileName;
        if (app.arguments().size() == 3)
        {
            const QString destination = app.arguments().at(2);
            const QFileInfo destinationInfo(destination);
            const bool destinationIsDirectory = destinationInfo.isDir() || destination.endsWith(QLatin1Char('/')) ||
                destination.endsWith(QLatin1Char('\\')) ||
                (!destinationInfo.exists() && !destination.endsWith(QStringLiteral(".fest"), Qt::CaseInsensitive));
            outputFile = destinationIsDirectory
                ? QDir::cleanPath(destinationInfo.absoluteFilePath() + QLatin1Char('/') + festFileName)
                : destinationInfo.absoluteFilePath();
        }
        QString errorMessage;
        if (!EventEditor::compileFileToFest(inputInfo.absoluteFilePath(), outputFile, &errorMessage))
        {
            err << QStringLiteral("Compilation failed: %1\n").arg(errorMessage);
            return 1;
        }

        out << QStringLiteral("Compiled %1 -> %2\n").arg(inputInfo.absoluteFilePath(), outputFile);
        return 0;
    }

    EventEditor window;
    window.show();
    return app.exec();
}
