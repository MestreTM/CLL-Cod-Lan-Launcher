#include "SteamDetector.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSet>
#include <QTextStream>

#ifdef Q_OS_WIN
#  include <windows.h>
#endif

namespace {

QString readRegSteamPath()
{
#ifdef Q_OS_WIN
    const wchar_t *keys[] = {
        L"SOFTWARE\\WOW6432Node\\Valve\\Steam",
        L"SOFTWARE\\Valve\\Steam",
    };
    for (const wchar_t *sub : keys) {
        HKEY hKey = nullptr;
        if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, sub, 0, KEY_READ, &hKey) != ERROR_SUCCESS)
            continue;
        wchar_t buf[MAX_PATH] = {};
        DWORD size = sizeof(buf);
        DWORD type = 0;
        const LONG ok = RegQueryValueExW(hKey, L"InstallPath", nullptr, &type,
                                         reinterpret_cast<LPBYTE>(buf), &size);
        RegCloseKey(hKey);
        if (ok == ERROR_SUCCESS && type == REG_SZ) {
            const QString path = QString::fromWCharArray(buf).trimmed();
            if (!path.isEmpty() && QDir(path).exists())
                return QDir::cleanPath(path);
        }
    }
#endif
    // Fallbacks comuns
    const QStringList candidates = {
        "C:/Program Files (x86)/Steam",
        "C:/Program Files/Steam",
        QDir::homePath() + "/.steam/steam",
        QDir::homePath() + "/.local/share/Steam",
    };
    for (const QString &c : candidates) {
        if (QDir(c).exists())
            return QDir::cleanPath(c);
    }
    return QString();
}

// Parse simplificado de libraryfolders.vdf (formato KeyValues do Steam).
QStringList parseLibraryFoldersVdf(const QString &vdfPath)
{
    QStringList out;
    QFile f(vdfPath);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return out;

    const QString text = QTextStream(&f).readAll();
    // "path"		"D:\\SteamLibrary"
    static const QRegularExpression re(
        QStringLiteral("\"path\"\\s*\"([^\"]+)\""),
        QRegularExpression::CaseInsensitiveOption);
    auto it = re.globalMatch(text);
    while (it.hasNext()) {
        const QString p = QDir::cleanPath(it.next().captured(1).replace("\\\\", "\\"));
        if (QDir(p).exists())
            out << p;
    }
    return out;
}

QString findGameInLibrary(const QString &libraryRoot, const QStringList &folderNames)
{
    const QString common = QDir(libraryRoot).filePath("steamapps/common");
    for (const QString &name : folderNames) {
        const QString path = QDir(common).filePath(name);
        if (QDir(path).exists())
            return QDir::cleanPath(path);
    }
    return QString();
}

} // namespace

namespace SteamDetector {

bool isValidWaw(const QString &dir)
{
    return QFileInfo::exists(dir + "/main/iw_00.iwd");
}
bool isValidBo1(const QString &dir)
{
    return QFileInfo::exists(dir + "/main/iw_00.iwd");
}
bool isValidBo2(const QString &dir)
{
    return QFileInfo::exists(dir + "/zone/all/base.ipak");
}
bool isValidMw3(const QString &dir)
{
    return QFileInfo::exists(dir + "/main/iw_00.iwd");
}

QString steamInstallPath()
{
    return readRegSteamPath();
}

QStringList steamLibraryPaths()
{
    QStringList libs;
    const QString steam = steamInstallPath();
    if (steam.isEmpty())
        return libs;

    libs << steam;

    const QString vdf = QDir(steam).filePath("steamapps/libraryfolders.vdf");
    for (const QString &p : parseLibraryFoldersVdf(vdf)) {
        if (!libs.contains(p, Qt::CaseInsensitive))
            libs << p;
    }
    return libs;
}

QStringList commonGameRoots()
{
    QStringList roots;
    const QStringList drives = {"C:", "D:", "E:", "F:"};
    const QStringList names = {"Games", "Jogos", "SteamLibrary", "Steam/steamapps/common"};
    for (const QString &d : drives) {
        for (const QString &n : names) {
            const QString p = d + "/" + n;
            if (QDir(p).exists())
                roots << QDir::cleanPath(p);
        }
    }
    roots << QDir::cleanPath("C:/Program Files (x86)/Steam/steamapps/common");
    roots << QDir::cleanPath("C:/Program Files/Steam/steamapps/common");
    return roots;
}

QList<GameHit> detectInstalledGames()
{
    QList<GameHit> hits;
    QSet<QString> seenIds;

    struct Spec {
        QString id;
        QStringList folders;
        bool (*validate)(const QString &);
    };
    const QList<Spec> specs = {
        {"World at War", {"Call of Duty World at War", "Call of Duty - World at War", "World at War"}, &isValidWaw},
        {"Black ops", {"Call of Duty Black Ops", "Call of Duty - Black Ops", "Black Ops"}, &isValidBo1},
        {"Black ops II", {"Call of Duty Black Ops II", "Call of Duty - Black Ops II", "Black Ops II", "pluto_t6_full_game"}, &isValidBo2},
        {"Modern Warfare 3", {"Call of Duty Modern Warfare 3", "Call of Duty - Modern Warfare 3", "Modern Warfare 3"}, &isValidMw3},
    };

    auto tryPath = [&](const QString &path, const Spec &spec) {
        if (path.isEmpty() || seenIds.contains(spec.id))
            return;
        if (spec.validate(path)) {
            hits.append({spec.id, QDir::cleanPath(path)});
            seenIds.insert(spec.id);
        }
    };

    for (const QString &lib : steamLibraryPaths()) {
        for (const Spec &spec : specs)
            tryPath(findGameInLibrary(lib, spec.folders), spec);
    }

    for (const QString &root : commonGameRoots()) {
        for (const Spec &spec : specs) {
            for (const QString &name : spec.folders)
                tryPath(QDir(root).filePath(name), spec);
            // also if root itself is already the game folder
            tryPath(root, spec);
        }
    }

    return hits;
}

} // namespace SteamDetector
