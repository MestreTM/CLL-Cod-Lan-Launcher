#include "AppSettings.h"
#include "GameLauncher.h"
#include "MainWindow.h"

#include <QApplication>
#include <QIcon>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QDir>
#include <QTextStream>
#include <QTimer>
#include <cstdio>

namespace {

// Headless mode: build the launch from CLI args and start the bootstrapper.
int runNoGui(AppSettings &settings, const QCommandLineParser &parser)
{
    settings.noGui = true;

    if (parser.isSet("plutoniumdir"))
        settings.plutoniumInstance = parser.value("plutoniumdir");
    if (parser.isSet("name"))
        settings.username = parser.value("name");

    const QString gameId = parser.value("gameid").toUpper();
    const QString mode = parser.value("mode").toUpper();

    if (gameId == "T4") {
        settings.modeId = (mode == "MP") ? "t4mp" : "t4sp";
        settings.gameId = "World at War";
        if (parser.isSet("gamedir")) settings.waw = parser.value("gamedir");
    } else if (gameId == "T5") {
        settings.modeId = (mode == "MP") ? "t5mp" : "t5sp";
        settings.gameId = "Black ops";
        if (parser.isSet("gamedir")) settings.bo1 = parser.value("gamedir");
    } else if (gameId == "T6") {
        settings.modeId = (mode == "MP") ? "t6mp" : "t6zm";
        settings.gameId = "Black ops II";
        if (parser.isSet("gamedir")) settings.bo2 = parser.value("gamedir");
    } else if (gameId == "IW5") {
        settings.modeId = "iw5mp";
        settings.gameId = "Modern Warfare 3";
        if (parser.isSet("gamedir")) settings.mw3 = parser.value("gamedir");
    } else {
        QTextStream(stdout) << "No valid game selected.\n";
        return 1;
    }

    const GameLauncher::Result result = GameLauncher::launch(settings, QString());
    if (result.hasError) {
        QTextStream(stdout) << "Cod Lan Launcher: falha ao lancar (veja LanLauncher.ini / parametros).\n";
        return 1;
    }
    return 0;
}

} // namespace

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName("Cod Lan Launcher");
    QApplication::setApplicationVersion("1.0.0");
    QApplication::setOrganizationName("MestreTM");
    QApplication::setWindowIcon(QIcon(QStringLiteral(":/icons/icon.ico")));
    QDir::setCurrent(QCoreApplication::applicationDirPath());

    QCommandLineParser parser;
    // Accepts "-nogui", "-gameid", etc. with a single dash
    // instead of treating them as clustered short options (-n -o -g -u -i).
    parser.setSingleDashWordOptionMode(QCommandLineParser::ParseAsLongOptions);
    parser.setApplicationDescription(
        "Cod Lan Launcher, para quando voce quer jogar Plutonium sem uma conexao de internet ativa!");
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addOption({"name", "Nome de usuario, ex: \"MestreTM\". Padrao: o definido na GUI.", "name"});
    parser.addOption({"plutoniumdir", "Local da pasta do Plutonium. Deve ser usado com -nogui.", "plutoniumdir"});
    parser.addOption({"mode", "Seletor de modo: \"MP\" ou \"ZM\". Deve ser usado com -nogui.", "mode", "ZM"});
    parser.addOption({"gamedir", "Local em que o jogo esta instalado. Deve ser usado com -nogui.", "gamedir"});
    parser.addOption({"gameid", "ID do jogo: T4, T5, T6, ou IW5. Deve ser usado com -nogui.", "gameid"});
    parser.addOption({"nogui", "Lanca sem interface grafica."});
    parser.process(app);

    if (parser.isSet("nogui")) {
        AppSettings settings = AppSettings::loadForStartup();
        return runNoGui(settings, parser);
    }

    MainWindow window;
    window.show();

    // Internal debug helper: dump a window screenshot when
    // LANLAUNCHER_SCREENSHOT=path.png is set (useful without an interactive display).
    const QString screenshotPath = qEnvironmentVariable("LANLAUNCHER_SCREENSHOT");
    if (!screenshotPath.isEmpty()) {
        const int pageIndex = qEnvironmentVariableIntValue("LANLAUNCHER_PAGE");
        QTimer::singleShot(100, &window, [&window, pageIndex]() { window.debugShowPage(pageIndex); });
        QTimer::singleShot(400, &window, [&window, screenshotPath]() {
            window.grab().save(screenshotPath);
            QApplication::quit();
        });
    }

    return app.exec();
}
