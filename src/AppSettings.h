#pragma once
#include <QString>

// Settings stored in LanLauncher.ini plus session state.
class AppSettings
{
public:
    QString versionNum = "1.1.0";
    QString theme      = "DarkAmber";
    QString username;
    QString plutoniumInstance;
    QString waw;
    QString bo1;
    QString bo2;
    QString mw3;
    bool setupCompleted = false;
    QString language = "en";
    bool homeEnabled = true;

    QString modId;
    QString gameId;
    QString serverId;
    QString modeId;          // t4mp, t6zm, iw5mp...
    bool    noGui = false;
    QString wawServMult;
    QString activeGame;
    bool serverMultiplayerSelected = false;

    static QString projectRoot();
    static QString iniPath();
    static QString localPuDir();
    static QString localPuBootstrapper();
    bool usingLocalPortableKit() const;
    static QString officialPlutoniumDir();
    static bool isPlutoniumRoot(const QString &root);
    static QString plutoniumRootSummary(const QString &root);
    // "./pu" is resolved to an absolute path next to the exe
    static QString resolvePath(const QString &path);

    void resolveStoredPaths();
    void useLocalPuIfPresent();
    bool loadFromIni();
    void saveToIni() const;
    void applyDefaultPlutoniumInstanceIfEmpty();
    QString gameFolder(const QString &gameIdName) const;
    static AppSettings loadForStartup();
};
