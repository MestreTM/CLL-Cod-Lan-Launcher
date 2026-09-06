#pragma once
#include <QString>

class QWidget;
class AppSettings;

// Typed error enum instead of a loose errorType string.
// Same cases as the original match errorType: list.
namespace Dialogs
{
    enum class Msg
    {
        Help,
        Username,
        T4,
        T5,
        T6,
        IW5,
        Plutonium,
        WrongGame,
        WrongGameServer,
        NoMod,
        NoCfg,
        UnsupportedExtension,
        NotStandardModFormat,
        FailedInstallMod,
        MultiConfigUnsupported,
        CustomMapEmpty,
        MustNameConfig,
        DownloadedMainConfigs,
        DownloadedT5Configs,
        DownloadedT4Configs,
        SevenZipSuccess,
        SevenZipFailDownload,
        SevenZipFailInstall,
        UpdateCheckFailed,
        NoUpdateAvailable,
    };

    // Simple info message (single Close button).
    void info(QWidget *parent, Msg kind, const AppSettings &settings);

    // returns true if the user confirmed.
    bool confirmDownload7z(QWidget *parent);

    // Ask whether to download the main Black Ops II gamesettings.
    // Returns true if confirmed.
    bool confirmDownloadGameSettings(QWidget *parent);
    bool confirmDownloadT5Settings(QWidget *parent);
    bool confirmDownloadT4Settings(QWidget *parent);

    // Generic “update available” prompt (unused until updates ship).
    bool confirmUpdate(QWidget *parent, const QString &newVersion);

    // Free-form error (ArchiveTool/Downloader already format the text).
    void error(QWidget *parent, const QString &message);
}
