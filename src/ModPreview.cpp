#include "ModPreview.h"
#include "SmartModInstaller.h"
#include "GameCatalog.h"
#include "Storage.h"
#include "CllInstaller.h"
#include "GithubModInstaller.h"
#include "ArchiveTool.h"

#include <QCheckBox>
#include <QEvent>
#include <QMetaObject>
#include <QTimer>
#include <QtConcurrent/QtConcurrent>
#include <QHash>
#include <QSet>
#include <QPair>
#include <QRegularExpression>
#include <QDialog>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>
#include <QScrollArea>
#include <QStorageInfo>
#include <QTextEdit>
#include <QVBoxLayout>

#include <functional>

namespace {

QString stripColor(QString s)
{
    s.replace(QRegularExpression(QStringLiteral("\\^[0-9]")), QString());
    return s.trimmed();
}

bool skipName(const QString &name)
{
    const QString n = name.toLower();
    return n.endsWith(".log") || n == "thumbs.db" || n == "desktop.ini"
        || n == "arvore.txt" || n == "lista.txt" || n == "info.json";
}

void walkFiles(const QString &root, const QString &rel, const std::function<void(const QString &, const QFileInfo &)> &fn)
{
    const QString abs = rel.isEmpty() ? root : root + "/" + rel;
    const QDir dir(abs);
    if (!dir.exists())
        return;
    for (const QFileInfo &fi : dir.entryInfoList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot)) {
        if (skipName(fi.fileName()))
            continue;
        const QString child = rel.isEmpty() ? fi.fileName() : rel + "/" + fi.fileName();
        if (fi.isDir())
            walkFiles(root, child, fn);
        else
            fn(child, fi);
    }
}

QString findFileNamed(const QString &root, const QString &fileName, int depth)
{
    if (depth < 0)
        return {};
    const QDir d(root);
    for (const QFileInfo &fi : d.entryInfoList(QDir::Files | QDir::NoDotAndDotDot)) {
        if (fi.fileName().compare(fileName, Qt::CaseInsensitive) == 0)
            return fi.absoluteFilePath();
    }
    if (depth == 0)
        return {};
    for (const QFileInfo &fi : d.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot)) {
        const QString hit = findFileNamed(fi.absoluteFilePath(), fileName, depth - 1);
        if (!hit.isEmpty())
            return hit;
    }
    return {};
}

QString findDirNamed(const QString &root, const QString &name, int depth)
{
    if (depth < 0)
        return {};
    if (QDir(root).dirName().compare(name, Qt::CaseInsensitive) == 0)
        return QDir::cleanPath(root);
    if (QDir(root + "/" + name).exists())
        return QDir::cleanPath(root + "/" + name);
    if (depth == 0)
        return {};
    for (const QFileInfo &fi : QDir(root).entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot)) {
        const QString hit = findDirNamed(fi.absoluteFilePath(), name, depth - 1);
        if (!hit.isEmpty())
            return hit;
    }
    return {};
}

void findAllDirsNamed(const QString &root, const QString &name, int depth, QStringList &out)
{
    if (depth < 0)
        return;
    if (QDir(root).dirName().compare(name, Qt::CaseInsensitive) == 0)
        out << QDir::cleanPath(root);
    if (depth == 0)
        return;
    for (const QFileInfo &fi : QDir(root).entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot))
        findAllDirsNamed(fi.absoluteFilePath(), name, depth - 1, out);
}

int pathSeg(const QString &path, const QString &name)
{
    const QStringList parts = path.split(QLatin1Char('/'), Qt::SkipEmptyParts);
    for (int i = 0; i < parts.size(); ++i) {
        if (parts[i].compare(name, Qt::CaseInsensitive) == 0)
            return i;
    }
    return -1;
}


QString normName(QString s)
{
    s = stripColor(s).toLower();
    s.replace(QRegularExpression("[^a-z0-9]+"), QStringLiteral(" "));
    return s.simplified();
}

struct OwnedFile {
    QString key;
    QString modId;
    QString display;
};

QList<OwnedFile> loadOwnedFiles(const QString &pu, const QString &gameCode)
{
    QList<OwnedFile> out;
    const QString root = QDir(pu).filePath(".lanlauncher_checkpoints/" + gameCode);
    QDir d(root);
    if (!d.exists())
        return out;
    for (const QString &id : d.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
        QFile f(root + "/" + id + "/manifest.json");
        if (!f.open(QIODevice::ReadOnly))
            continue;
        const QJsonObject o = QJsonDocument::fromJson(f.readAll()).object();
        QString display = o.value(QStringLiteral("displayName")).toString();
        if (display.isEmpty())
            display = o.value(QStringLiteral("name")).toString();
        if (display.isEmpty())
            display = id;
        auto take = [&](const QJsonArray &arr) {
            for (const QJsonValue &v : arr) {
                const QJsonObject e = v.toObject();
                OwnedFile of;
                of.modId = id;
                of.display = display;
                of.key = e.value(QStringLiteral("root")).toString() + "/"
                         + e.value(QStringLiteral("rel")).toString();
                of.key.replace(QLatin1Char('\\'), QLatin1Char('/'));
                out.push_back(of);
            }
        };
        take(o.value(QStringLiteral("added")).toArray());
        take(o.value(QStringLiteral("replaced")).toArray());
    }
    return out;
}

struct InstalledMod {
    QString id;
    QString name;
    QString version;
    QString description;
    QString folder;
};

QJsonObject readJsonFile(const QString &path);

QList<InstalledMod> loadInstalledMods(const QString &pu, const QString &gameCode)
{
    QList<InstalledMod> out;
    QSet<QString> seen;
    const QString ck = QDir(pu).filePath(".lanlauncher_checkpoints/" + gameCode);
    for (const QString &id : QDir(ck).entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
        QFile f(ck + "/" + id + "/manifest.json");
        InstalledMod m;
        m.id = id;
        if (f.open(QIODevice::ReadOnly)) {
            const QJsonObject o = QJsonDocument::fromJson(f.readAll()).object();
            m.name = o.value(QStringLiteral("displayName")).toString();
            if (m.name.isEmpty())
                m.name = o.value(QStringLiteral("name")).toString();
            m.version = o.value(QStringLiteral("version")).toString();
            m.description = o.value(QStringLiteral("description")).toString();
            auto grabFolder = [&](const QJsonArray &arr) {
                for (const QJsonValue &v : arr) {
                    QString rel = v.toObject().value(QStringLiteral("rel")).toString();
                    rel.replace(QLatin1Char('\\'), QLatin1Char('/'));
                    const int i = rel.indexOf(QStringLiteral("/mods/"), 0, Qt::CaseInsensitive);
                    if (i >= 0 && m.folder.isEmpty())
                        m.folder = rel.mid(i + 6).section(QLatin1Char('/'), 0, 0);
                }
            };
            grabFolder(o.value(QStringLiteral("added")).toArray());
            grabFolder(o.value(QStringLiteral("replaced")).toArray());
        }
        if (m.name.isEmpty())
            m.name = id;
        out.push_back(m);
        seen.insert(normName(m.name));
        seen.insert(normName(id));
    }
    const QString mods = QDir(pu).filePath("storage/" + gameCode + "/mods");
    for (const QString &folder : QDir(mods).entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
        InstalledMod m;
        m.id = folder;
        m.folder = folder;
        const QString mj = mods + "/" + folder + "/mod.json";
        if (QFileInfo::exists(mj)) {
            const QJsonObject o = readJsonFile(mj);
            m.name = stripColor(o.value(QStringLiteral("name")).toString());
            m.version = stripColor(o.value(QStringLiteral("version")).toString());
            m.description = stripColor(o.value(QStringLiteral("description")).toString());
        }
        if (m.name.isEmpty())
            m.name = folder;
        if (!seen.contains(normName(m.name)) && !seen.contains(normName(folder)))
            out.push_back(m);
    }
    return out;
}

void attachConflictsAndUpdate(ModPreview::Preview &p, const QString &pu, const QString &gameRoot,
                              const QList<QPair<QString, QString>> &destFiles)
{
    const auto owned = loadOwnedFiles(pu, p.gameCode);
    QHash<QString, QStringList> byKey;
    for (const auto &of : owned) {
        if (!byKey[of.key].contains(of.display))
            byKey[of.key] << of.display;
    }
    QSet<QString> seenC;
    for (const auto &pair : destFiles) {
        const QString key = pair.first + "/" + pair.second;
        const QStringList others = byKey.value(key);
        if (others.isEmpty())
            continue;
        const QString sig = key + others.join(QLatin1Char(','));
        if (seenC.contains(sig))
            continue;
        seenC.insert(sig);
        ModPreview::Conflict c;
        c.destRoot = pair.first;
        c.destRel = pair.second;
        c.otherMods = others;
        p.conflicts.push_back(c);
        if (p.conflicts.size() >= 40)
            break;
    }

    QSet<QString> incomingFolders;
    auto takeModsFolder = [&](const QString &rel) {
        const QString n = QString(rel).replace(QLatin1Char('\\'), QLatin1Char('/'));
        const int i = n.indexOf(QStringLiteral("/mods/"), 0, Qt::CaseInsensitive);
        if (i < 0)
            return;
        const QString rest = n.mid(i + 6);
        const QString folder = rest.section(QLatin1Char('/'), 0, 0);
        if (!folder.isEmpty())
            incomingFolders.insert(folder.toLower());
    };
    for (const ModPreview::Mapping &map : p.mappings) {
        takeModsFolder(map.destRel);
        takeModsFolder(map.fromRel);
    }
    for (const auto &pair : destFiles)
        takeModsFolder(pair.second);

    const auto installed = loadInstalledMods(pu, p.gameCode);
    const QString want = normName(p.name);
    auto consider = [&](const InstalledMod &im) {
        if (p.replacesExisting)
            return;
        const bool nameHit = !want.isEmpty() && (normName(im.name) == want || normName(im.id) == want);
        const bool folderHit = incomingFolders.contains(im.folder.toLower())
                               || incomingFolders.contains(im.id.toLower());
        if (!nameHit && !folderHit)
            return;
        p.replacesExisting = true;
        p.existingId = im.id;
        p.existingName = im.name;
        p.existingVersion = im.version;
        p.existingDescription = im.description;
    };
    for (const auto &im : installed)
        consider(im);
}

QString pathPrefix(const QString &path, int lastInclusive)
{
    const QStringList parts = path.split(QLatin1Char('/'), Qt::SkipEmptyParts);
    if (lastInclusive < 0 || lastInclusive >= parts.size())
        return {};
    return QStringList(parts.mid(0, lastInclusive + 1)).join(QLatin1Char('/'));
}

QJsonObject readJsonFile(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return {};
    return QJsonDocument::fromJson(f.readAll()).object();
}

QString sharedIndexPath(const QString &pu)
{
    return QDir(pu).filePath(".lanlauncher_checkpoints/_shared/index.json");
}

QJsonObject loadSharedIndex(const QString &pu)
{
    return readJsonFile(sharedIndexPath(pu));
}

QStringList ownersOf(const QJsonObject &index, const QString &key)
{
    QStringList out;
    const QJsonArray a = index.value(key).toObject().value(QStringLiteral("owners")).toArray();
    for (const QJsonValue &v : a) {
        const QString s = v.toString();
        if (!s.isEmpty())
            out << s;
    }
    return out;
}

qint64 folderBytes(const QString &root)
{
    qint64 n = 0;
    walkFiles(root, QString(), [&](const QString &, const QFileInfo &fi) { n += fi.size(); });
    return n;
}

int folderFiles(const QString &root)
{
    int n = 0;
    walkFiles(root, QString(), [&](const QString &, const QFileInfo &) { ++n; });
    return n;
}

qint64 existingOverlapBytes(const QString &srcRoot, const QString &dstRoot)
{
    qint64 n = 0;
    walkFiles(srcRoot, QString(), [&](const QString &rel, const QFileInfo &fi) {
        if (QFileInfo::exists(dstRoot + "/" + rel))
            n += fi.size();
    });
    return n;
}

QString driveLabel(const QString &path)
{
    const QStorageInfo st(path);
    QString root = QString::fromUtf8(st.rootPath().toUtf8());
    if (root.isEmpty())
        root = QFileInfo(path).absoluteFilePath().left(2);
    return root;
}

} // namespace


namespace ModPreview {

Preview analyze(const QString &extractedRoot,
                const QString &gameId,
                const QString &gameCode,
                const QString &plutoniumRoot,
                const QString &gameRoot)
{
    Preview p;
    p.extractedRoot = extractedRoot;
    p.gameId = gameId;
    p.gameCode = gameCode.isEmpty() ? Storage::gameStorageId(gameId) : gameCode;
    if (p.gameCode.isEmpty())
        p.gameCode = QStringLiteral("t6");

    const QString cllPath = findFileNamed(extractedRoot, QStringLiteral("cll_installer.json"), 5);
    const QString modJsonPath = findFileNamed(extractedRoot, QStringLiteral("mod.json"), 5);

    if (!cllPath.isEmpty()) {
        const auto man = CllInstaller::parseJsonFile(cllPath);
        if (man.valid) {
            p.name = stripColor(man.name);
            p.version = stripColor(man.version);
            p.author = stripColor(man.author);
            p.description = stripColor(man.description);
            p.gameCode = man.gameCode;
            p.gameId = man.gameId;
            p.sourceKind = QStringLiteral("cll");
            const QString packRoot = QFileInfo(cllPath).absolutePath();
            for (const auto &rule : man.folders) {
                Mapping m;
                m.fromRel = QDir(extractedRoot).relativeFilePath(packRoot + "/" + rule.from);
                m.destRoot = rule.target == QLatin1String("game_folder")
                                 ? QStringLiteral("game")
                                 : QStringLiteral("plutonium");
                m.destRel = rule.dest;
                const QString src = packRoot + "/" + rule.from;
                m.bytes = folderBytes(src);
                m.files = folderFiles(src);
                p.mappings.push_back(m);
            }
        }
    }

    if (!modJsonPath.isEmpty() && p.name.isEmpty()) {
        const QJsonObject o = readJsonFile(modJsonPath);
        p.name = stripColor(o.value(QStringLiteral("name")).toString());
        p.version = stripColor(o.value(QStringLiteral("version")).toString());
        p.author = stripColor(o.value(QStringLiteral("author")).toString());
        p.description = stripColor(o.value(QStringLiteral("description")).toString());
        if (p.sourceKind.isEmpty())
            p.sourceKind = QStringLiteral("mod.json");
    }

    if (p.mappings.isEmpty()) {
        const QString storage = findDirNamed(extractedRoot, QStringLiteral("storage"), 4);
        const QString steam = findDirNamed(extractedRoot, QStringLiteral("steam"), 4);
        if (!storage.isEmpty() || !steam.isEmpty())
            p.sourceKind = p.sourceKind.isEmpty() ? QStringLiteral("smart") : p.sourceKind;

        auto relOf = [&](const QString &abs) {
            return QDir(extractedRoot).relativeFilePath(abs);
        };
        auto hasSeg = [](const QString &rel, const QString &name) {
            return pathSeg(rel, name) >= 0;
        };

        if (!storage.isEmpty()) {
            Mapping m;
            m.fromRel = QDir(extractedRoot).relativeFilePath(storage);
            m.destRoot = QStringLiteral("plutonium");
            m.destRel = QStringLiteral("storage");
            m.bytes = folderBytes(storage);
            m.files = folderFiles(storage);
            p.mappings.push_back(m);
            for (const QString &id : QStringList{"t6", "t5", "t4", "iw5"}) {
                if (QDir(storage + "/" + id).exists()) {
                    p.gameCode = id;
                    break;
                }
            }
        }
        if (!steam.isEmpty()) {
            Mapping m;
            m.fromRel = QDir(extractedRoot).relativeFilePath(steam);
            m.destRoot = QStringLiteral("game");
            m.destRel = QString();
            m.bytes = folderBytes(steam);
            m.files = folderFiles(steam);
            p.mappings.push_back(m);
        }

        bool hasGameMap = false;
        for (const Mapping &ex : p.mappings) {
            if (ex.destRoot == QLatin1String("game"))
                hasGameMap = true;
        }
        if (!hasGameMap) {
            QStringList zones, sounds;
            findAllDirsNamed(extractedRoot, QStringLiteral("zone"), 5, zones);
            findAllDirsNamed(extractedRoot, QStringLiteral("sound"), 5, sounds);
            auto usable = [&](const QString &abs) {
                const QString rel = QDir(extractedRoot).relativeFilePath(abs);
                return !rel.contains(QLatin1String("storage"), Qt::CaseInsensitive)
                    && !rel.contains(QLatin1String("/mods/"), Qt::CaseInsensitive);
            };
            for (const QString &z : zones) {
                if (!usable(z))
                    continue;
                Mapping m;
                m.fromRel = QDir(extractedRoot).relativeFilePath(z);
                m.destRoot = QStringLiteral("game");
                m.destRel = QStringLiteral("zone");
                m.bytes = folderBytes(z);
                m.files = folderFiles(z);
                p.mappings.push_back(m);
            }
            for (const QString &s : sounds) {
                if (!usable(s))
                    continue;
                Mapping m;
                m.fromRel = QDir(extractedRoot).relativeFilePath(s);
                m.destRoot = QStringLiteral("game");
                m.destRel = QStringLiteral("sound");
                m.bytes = folderBytes(s);
                m.files = folderFiles(s);
                p.mappings.push_back(m);
            }
        }

        // Overlay folders used by Plutonium without fs_game: storage/<game>/raw and images.
        if (storage.isEmpty()) {
            QStringList rawDirs;
            findAllDirsNamed(extractedRoot, QStringLiteral("raw"), 5, rawDirs);
            for (const QString &raw : rawDirs) {
                const QString rel = relOf(raw);
                if (hasSeg(rel, QStringLiteral("storage")) || hasSeg(rel, QStringLiteral("mods")))
                    continue;
                Mapping m;
                m.fromRel = rel;
                m.destRoot = QStringLiteral("plutonium");
                m.destRel = QStringLiteral("storage/") + p.gameCode + "/raw";
                m.bytes = folderBytes(raw);
                m.files = folderFiles(raw);
                p.mappings.push_back(m);
                if (p.sourceKind.isEmpty() || p.sourceKind == QLatin1String("mod.json"))
                    p.sourceKind = QStringLiteral("raw");
            }
            QStringList imageDirs;
            findAllDirsNamed(extractedRoot, QStringLiteral("images"), 5, imageDirs);
            for (const QString &img : imageDirs) {
                const QString rel = relOf(img);
                if (hasSeg(rel, QStringLiteral("storage")) || hasSeg(rel, QStringLiteral("mods"))
                    || hasSeg(rel, QStringLiteral("raw")))
                    continue;
                Mapping m;
                m.fromRel = rel;
                m.destRoot = QStringLiteral("plutonium");
                m.destRel = QStringLiteral("storage/") + p.gameCode + "/images";
                m.bytes = folderBytes(img);
                m.files = folderFiles(img);
                p.mappings.push_back(m);
                if (p.sourceKind.isEmpty() || p.sourceKind == QLatin1String("mod.json"))
                    p.sourceKind = QStringLiteral("raw");
            }
            QStringList modsRoots;
            findAllDirsNamed(extractedRoot, QStringLiteral("mods"), 5, modsRoots);
            QSet<QString> mappedMods;
            for (const QString &modsRoot : modsRoots) {
                if (hasSeg(relOf(modsRoot), QStringLiteral("storage")))
                    continue;
                for (const QFileInfo &sub : QDir(modsRoot).entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot)) {
                    const QString key = sub.fileName().toLower();
                    if (mappedMods.contains(key))
                        continue;
                    mappedMods.insert(key);
                    Mapping m;
                    m.fromRel = relOf(sub.absoluteFilePath());
                    m.destRoot = QStringLiteral("plutonium");
                    m.destRel = QStringLiteral("storage/") + p.gameCode + "/mods/" + sub.fileName();
                    m.bytes = folderBytes(sub.absoluteFilePath());
                    m.files = folderFiles(sub.absoluteFilePath());
                    p.mappings.push_back(m);
                }
            }
        }

        // Isolated mod.json (no storage/steam/raw wrapper): that folder goes to mods/
        if (p.mappings.isEmpty() && !modJsonPath.isEmpty()) {
            const QString modFolder = QFileInfo(modJsonPath).absolutePath();
            Mapping m;
            m.fromRel = QDir(extractedRoot).relativeFilePath(modFolder);
            m.destRoot = QStringLiteral("plutonium");
            m.destRel = QStringLiteral("storage/") + p.gameCode + "/mods/" + QDir(modFolder).dirName();
            m.bytes = folderBytes(modFolder);
            m.files = folderFiles(modFolder);
            p.mappings.push_back(m);
        }

        if (p.mappings.isEmpty()) {
            const QString zone = findDirNamed(extractedRoot, QStringLiteral("zone"), 3);
            const QString sound = findDirNamed(extractedRoot, QStringLiteral("sound"), 3);
            if (!zone.isEmpty() && (QDir(zone + "/all").exists() || QDir(zone + "/english").exists())) {
                Mapping m;
                m.fromRel = QDir(extractedRoot).relativeFilePath(zone);
                m.destRoot = QStringLiteral("game");
                m.destRel = QStringLiteral("zone");
                m.bytes = folderBytes(zone);
                m.files = folderFiles(zone);
                p.mappings.push_back(m);
            }
            if (!sound.isEmpty()) {
                Mapping m;
                m.fromRel = QDir(extractedRoot).relativeFilePath(sound);
                m.destRoot = QStringLiteral("game");
                m.destRel = QStringLiteral("sound");
                m.bytes = folderBytes(sound);
                m.files = folderFiles(sound);
                p.mappings.push_back(m);
            }
        }
        if (p.mappings.isEmpty()) {
            Mapping m;
            m.fromRel = QString();
            m.destRoot = QStringLiteral("plutonium");
            m.destRel = QStringLiteral("storage/") + p.gameCode + "/mods/"
                        + QFileInfo(extractedRoot).fileName();
            m.bytes = folderBytes(extractedRoot);
            m.files = folderFiles(extractedRoot);
            p.mappings.push_back(m);
        }
    }

    if (p.name.isEmpty())
        p.name = QFileInfo(extractedRoot).fileName();

    const QJsonObject index = loadSharedIndex(plutoniumRoot);
    for (const Mapping &map : p.mappings) {
        const QString src = map.fromRel.isEmpty() ? extractedRoot : extractedRoot + "/" + map.fromRel;
        const QString dstRoot = map.destRoot == QLatin1String("game")
                                    ? gameRoot
                                    : plutoniumRoot;
        const QString dstBase = map.destRel.isEmpty() ? dstRoot : dstRoot + "/" + map.destRel;
        walkFiles(src, QString(), [&](const QString &rel, const QFileInfo &) {
            const QString destRel = map.destRel.isEmpty() ? rel : (map.destRel + "/" + rel);
            const QString key = map.destRoot + "/" + destRel;
            const QStringList owners = ownersOf(index, key);
            if (!owners.isEmpty()) {
                ModPreview::Conflict c;
                c.destRoot = map.destRoot;
                c.destRel = destRel;
                c.otherMods = owners;
                p.conflicts.push_back(c);
            } else if (!dstBase.isEmpty() && QFileInfo::exists(dstBase + "/" + rel)) {
                ModPreview::Conflict c;
                c.destRoot = map.destRoot;
                c.destRel = destRel;
                c.otherMods = QStringList{QObject::tr("(arquivo ja existente)")};
                p.conflicts.push_back(c);
            }
        });
    }

    QHash<QString, DiskUse> disks;
    auto addDisk = [&](const QString &path, const QString &label, qint64 payload, qint64 backup) {
        if (path.isEmpty())
            return;
        const QStorageInfo st(path);
        const QString id = st.rootPath().isEmpty() ? path : st.rootPath();
        DiskUse &d = disks[id];
        d.rootPath = path;
        d.label = label;
        d.available = st.bytesAvailable();
        d.total = st.bytesTotal();
        d.used = (d.total > 0 && d.available >= 0) ? (d.total - d.available) : 0;
        d.payload += payload;
        d.backupEstimate += backup;
    };

    for (const Mapping &map : p.mappings) {
        p.totalPayload += map.bytes;
        const QString src = map.fromRel.isEmpty() ? extractedRoot : extractedRoot + "/" + map.fromRel;
        const QString dstRoot = map.destRoot == QLatin1String("game") ? gameRoot : plutoniumRoot;
        const QString dstBase = map.destRel.isEmpty() ? dstRoot : dstRoot + "/" + map.destRel;
        qint64 bak = dstRoot.isEmpty() ? 0 : existingOverlapBytes(src, dstBase);
        p.totalBackupEstimate += bak;
        addDisk(dstRoot,
                map.destRoot == QLatin1String("game")
                    ? QObject::tr("Jogo")
                    : QObject::tr("Plutonium"),
                map.bytes, bak);
    }
    p.disks = disks.values();

    if (p.mappings.isEmpty())
        p.error = QObject::tr("Nao foi possivel projetar a instalacao deste pacote.");
    return p;
}

Preview analyzeArchive(const QString &archivePath,
                       const QString &gameId,
                       const QString &gameCode,
                       const QString &plutoniumRoot,
                       const QString &gameRoot)
{
    Preview p;
    p.gameId = gameId;
    p.gameCode = gameCode.isEmpty() ? Storage::gameStorageId(gameId) : gameCode;
    if (p.gameCode.isEmpty())
        p.gameCode = QStringLiteral("t6");
    p.sourceKind = QStringLiteral("smart");
    p.name = QFileInfo(archivePath).completeBaseName();

    const auto listed = ArchiveTool::listDetailed(archivePath);
    if (listed.isEmpty()) {
        p.error = QObject::tr("Nao foi possivel ler o arquivo compactado.");
        return p;
    }

    QStringList jsonInner;
    for (const auto &e : listed) {
        const QString base = QFileInfo(e.path).fileName();
        if (base.compare(QLatin1String("mod.json"), Qt::CaseInsensitive) == 0
            || base.compare(QLatin1String("cll_installer.json"), Qt::CaseInsensitive) == 0)
            jsonInner << e.path;
    }
    QString peekDir;
    if (!jsonInner.isEmpty()) {
        peekDir = QDir::temp().filePath(QStringLiteral("LanLauncher_mod_peek"));
        QDir(peekDir).removeRecursively();
        QDir().mkpath(peekDir);
        QString err;
        ArchiveTool::extractPaths(archivePath, peekDir, jsonInner, &err);
        const QString cll = findFileNamed(peekDir, QStringLiteral("cll_installer.json"), 6);
        const QString mj = findFileNamed(peekDir, QStringLiteral("mod.json"), 6);
        if (!cll.isEmpty()) {
            const auto man = CllInstaller::parseJsonFile(cll);
            if (man.valid) {
                p.name = stripColor(man.name);
                p.version = stripColor(man.version);
                p.author = stripColor(man.author);
                p.description = stripColor(man.description);
                p.gameCode = man.gameCode.isEmpty() ? p.gameCode : man.gameCode;
                p.gameId = man.gameId.isEmpty() ? p.gameId : man.gameId;
                p.sourceKind = QStringLiteral("cll");
            }
        } else if (!mj.isEmpty()) {
            const QJsonObject o = readJsonFile(mj);
            p.name = stripColor(o.value(QStringLiteral("name")).toString());
            p.version = stripColor(o.value(QStringLiteral("version")).toString());
            p.author = stripColor(o.value(QStringLiteral("author")).toString());
            p.description = stripColor(o.value(QStringLiteral("description")).toString());
            p.sourceKind = QStringLiteral("mod.json");
        }
        QDir(peekDir).removeRecursively();
    }

    struct Bucket { qint64 bytes = 0; int files = 0; };
    QHash<QString, Bucket> buckets; // fromRel|destRoot|destRel

    QList<QPair<QString, QString>> destFiles;
    auto add = [&](const QString &fromRel, const QString &destRoot, const QString &destRel, qint64 sz, const QString &innerRel = QString()) {
        const QString key = fromRel + QLatin1Char('\n') + destRoot + QLatin1Char('\n') + destRel;
        buckets[key].bytes += sz;
        buckets[key].files += 1;
        QString rel = destRel;
        if (!innerRel.isEmpty())
            rel = destRel.isEmpty() ? innerRel : (destRel + "/" + innerRel);
        if (!rel.isEmpty())
            destFiles.append(qMakePair(destRoot, rel));
    };

    bool hasStorage = false, hasSteam = false;
    for (const auto &e : listed) {
        if (e.isDir || e.path.isEmpty())
            continue;
        if (pathSeg(e.path, QStringLiteral("storage")) >= 0)
            hasStorage = true;
        if (pathSeg(e.path, QStringLiteral("steam")) >= 0)
            hasSteam = true;
    }

    for (const auto &e : listed) {
        if (e.isDir || e.path.isEmpty())
            continue;
        const QString base = QFileInfo(e.path).fileName().toLower();
        if (base.endsWith(".log") || base == "thumbs.db" || base == "desktop.ini")
            continue;
        const int iStore = pathSeg(e.path, QStringLiteral("storage"));
        const int iSteam = pathSeg(e.path, QStringLiteral("steam"));
        if (iStore >= 0) {
            {
                const QString pref = pathPrefix(e.path, iStore);
                add(pref, QStringLiteral("plutonium"), QStringLiteral("storage"), e.size, e.path.mid(pref.size() + 1));
            }
            const int iGame = pathSeg(e.path, QStringLiteral("t6"));
            if (iGame < 0) {
                if (pathSeg(e.path, QStringLiteral("t5")) >= 0) p.gameCode = QStringLiteral("t5");
                else if (pathSeg(e.path, QStringLiteral("t4")) >= 0) p.gameCode = QStringLiteral("t4");
                else if (pathSeg(e.path, QStringLiteral("iw5")) >= 0) p.gameCode = QStringLiteral("iw5");
            } else {
                p.gameCode = QStringLiteral("t6");
            }
            continue;
        }
        if (iSteam >= 0) {
            {
                const QString pref = pathPrefix(e.path, iSteam);
                add(pref, QStringLiteral("game"), QString(), e.size, e.path.mid(pref.size() + 1));
            }
            continue;
        }
        if (!hasSteam) {
            const int iZone = pathSeg(e.path, QStringLiteral("zone"));
            const int iSound = pathSeg(e.path, QStringLiteral("sound"));
            if (iZone >= 0) {
                {
                    const QString pref = pathPrefix(e.path, iZone);
                    add(pref, QStringLiteral("game"), QStringLiteral("zone"), e.size, e.path.mid(pref.size() + 1));
                }
                continue;
            }
            if (iSound >= 0) {
                {
                    const QString pref = pathPrefix(e.path, iSound);
                    add(pref, QStringLiteral("game"), QStringLiteral("sound"), e.size, e.path.mid(pref.size() + 1));
                }
                continue;
            }
        }
        if (!hasStorage) {
            const int iMods = pathSeg(e.path, QStringLiteral("mods"));
            const int iRaw = pathSeg(e.path, QStringLiteral("raw"));
            const int iImages = pathSeg(e.path, QStringLiteral("images"));
            if (iRaw >= 0 && iMods < 0) {
                const QString pref = pathPrefix(e.path, iRaw);
                add(pref, QStringLiteral("plutonium"),
                    QStringLiteral("storage/") + p.gameCode + "/raw",
                    e.size, e.path.mid(pref.size() + 1));
                if (p.sourceKind != QLatin1String("cll"))
                    p.sourceKind = QStringLiteral("raw");
                continue;
            }
            if (iImages >= 0 && iMods < 0 && iRaw < 0) {
                const QString pref = pathPrefix(e.path, iImages);
                add(pref, QStringLiteral("plutonium"),
                    QStringLiteral("storage/") + p.gameCode + "/images",
                    e.size, e.path.mid(pref.size() + 1));
                if (p.sourceKind != QLatin1String("cll"))
                    p.sourceKind = QStringLiteral("raw");
                continue;
            }
            if (iMods >= 0) {
                const QStringList parts = e.path.split(QLatin1Char('/'), Qt::SkipEmptyParts);
                if (iMods + 1 < parts.size()) {
                    const QString modName = parts.at(iMods + 1);
                    const QString fromRel = QStringList(parts.mid(0, iMods + 2)).join(QLatin1Char('/'));
                    const QString inner = QStringList(parts.mid(iMods + 2)).join(QLatin1Char('/'));
                    add(fromRel, QStringLiteral("plutonium"),
                        QStringLiteral("storage/") + p.gameCode + "/mods/" + modName,
                        e.size, inner);
                }
                continue;
            }
            const QString fn = QFileInfo(e.path).fileName();
            if (fn.compare(QLatin1String("mod.json"), Qt::CaseInsensitive) == 0) {
                QString folder = QFileInfo(e.path).path();
                if (folder == QLatin1String("."))
                    folder = QFileInfo(archivePath).completeBaseName();
                add(folder, QStringLiteral("plutonium"),
                    QStringLiteral("storage/") + p.gameCode + "/mods/" + QFileInfo(folder).fileName(),
                    e.size);
            }
        }
    }

    for (auto it = buckets.begin(); it != buckets.end(); ++it) {
        const QStringList bits = it.key().split(QLatin1Char('\n'));
        Mapping m;
        m.fromRel = bits.value(0);
        m.destRoot = bits.value(1);
        m.destRel = bits.value(2);
        m.bytes = it.value().bytes;
        m.files = it.value().files;
        p.mappings.push_back(m);
    }

    if (p.mappings.isEmpty())
        p.error = QObject::tr("Nao foi possivel projetar a instalacao deste pacote.");

    // disk + conflicts reuse analyze on a fake extract? compute disks only
    QHash<QString, DiskUse> disks;
    auto addDisk = [&](const QString &path, const QString &label, qint64 payload) {
        if (path.isEmpty())
            return;
        const QStorageInfo st(path);
        const QString id = st.rootPath().isEmpty() ? path : st.rootPath();
        DiskUse &d = disks[id];
        d.rootPath = path;
        d.label = label;
        d.available = st.bytesAvailable();
        d.total = st.bytesTotal();
        d.used = (d.total > 0 && d.available >= 0) ? (d.total - d.available) : 0;
        d.payload += payload;
        d.backupEstimate += payload / 4;
    };
    for (const Mapping &map : p.mappings) {
        p.totalPayload += map.bytes;
        const QString dst = map.destRoot == QLatin1String("game") ? gameRoot : plutoniumRoot;
        addDisk(dst,
                map.destRoot == QLatin1String("game") ? QObject::tr("Jogo") : QObject::tr("Plutonium"),
                map.bytes);
        p.totalBackupEstimate += map.bytes / 4;
    }
    p.disks = disks.values();
    attachConflictsAndUpdate(p, plutoniumRoot, gameRoot, destFiles);
    return p;
}


bool confirm(QWidget *parent, Preview &preview)
{
    QDialog dlg(parent);
    dlg.setWindowTitle(preview.name);
    dlg.setModal(true);
    dlg.setMinimumSize(560, 460);
    dlg.resize(640, 520);
    dlg.setStyleSheet(
        "QDialog { background: #161821; }"
        "QLabel { color: #e8e6f2; }"
        "QLabel#CllKicker { color: #9184d9; font-size: 11px; font-weight: 700; letter-spacing: 1.4px; }"
        "QLabel#CllTitle { color: #f4f2ff; font-size: 20px; font-weight: 700; }"
        "QLabel#CllMeta { color: #8a8ba3; font-size: 12px; }"
        "QLabel#CllChip { background: #242636; color: #d9d6ea; border-radius: 11px; padding: 4px 10px; font-size: 11px; }"
        "QLabel#CllChipAccent { background: #9184d9; color: #14121f; border-radius: 11px; padding: 4px 10px; font-size: 11px; font-weight: 700; }"
        "QLabel#CllChipWarn { background: #3a2d18; color: #e6b450; border: 1px solid #8a6a2a; border-radius: 11px; padding: 4px 10px; font-size: 11px; font-weight: 700; }"
        "QLabel#CllWarnBanner { color: #e6b450; font-size: 12px; font-weight: 700; }"
        "QPushButton#CllMore { background: #242636; color: #d9d6ea; border: 1px solid #2b2e42; }"
        "QPushButton#CllMore:hover { background: #2b2e42; }"
        "QFrame#CllCard { background: #1c1e2b; border: 1px solid #2b2e42; border-radius: 10px; }"
        "QLabel#CllDestFrom { color: #c8c6d8; font-size: 12px; }"
        "QLabel#CllDestTo { color: #9184d9; font-size: 11px; font-weight: 600; }"
        "QTextEdit { background: #12131c; color: #d7d5e6; border: 1px solid #2b2e42; border-radius: 8px; padding: 8px; }"
        "QCheckBox { color: #d7d5e6; }"
        "QPushButton { min-height: 34px; padding: 0 16px; border-radius: 8px; }"
        "QPushButton#CllCancel { background: #242636; color: #d9d6ea; border: 1px solid #2b2e42; }"
        "QPushButton#CllCancel:hover { background: #2b2e42; }"
        "QPushButton#CllInstall { background: #9184d9; color: #14121f; font-weight: 700; border: none; }"
        "QPushButton#CllInstall:hover { background: #7a6cc9; }"
        "QScrollArea { border: none; background: transparent; }");

    auto *root = new QVBoxLayout(&dlg);
    root->setContentsMargins(22, 20, 22, 18);
    root->setSpacing(14);

    auto *hero = new QHBoxLayout();
    hero->setSpacing(14);
    auto *icon = new QLabel(&dlg);
    icon->setFixedSize(64, 64);
    const QPixmap pix = GameCatalog::icon(preview.gameCode, QSize(64, 64));
    if (!pix.isNull()) {
        icon->setPixmap(pix);
        icon->setScaledContents(true);
    } else {
        icon->setText(preview.gameCode.toUpper());
        icon->setAlignment(Qt::AlignCenter);
        icon->setStyleSheet("background:#242636; border-radius:12px; color:#9184d9; font-weight:700;");
    }
    hero->addWidget(icon, 0, Qt::AlignTop);

    auto *head = new QVBoxLayout();
    head->setSpacing(4);
    const QString kickerText = preview.sourceKind == QLatin1String("cll")
                                   ? QObject::tr("CLL PACK")
                                   : (preview.sourceKind == QLatin1String("raw")
                                          ? QObject::tr("RAW PACK")
                                          : (preview.sourceKind == QLatin1String("github")
                                                 ? QObject::tr("GITHUB PACK")
                                                 : (preview.sourceKind == QLatin1String("host")
                                                        ? QObject::tr("HOST PACK")
                                                        : QObject::tr("MOD PACK"))));
    auto *kicker = new QLabel(kickerText, &dlg);
    kicker->setObjectName("CllKicker");
    auto *title = new QLabel(preview.name, &dlg);
    title->setObjectName("CllTitle");
    title->setWordWrap(true);
    head->addWidget(kicker);
    head->addWidget(title);

    auto *chips = new QHBoxLayout();
    chips->setSpacing(6);
    auto addChip = [&](const QString &text, bool accent) {
        if (text.isEmpty())
            return;
        auto *c = new QLabel(text, &dlg);
        c->setObjectName(accent ? "CllChipAccent" : "CllChip");
        c->setAlignment(Qt::AlignCenter);
        chips->addWidget(c);
    };
    if (preview.sourceKind == QLatin1String("cll"))
        addChip(QObject::tr("CLL"), true);
    else if (preview.sourceKind == QLatin1String("raw"))
        addChip(QObject::tr("RAW"), true);
    else if (preview.sourceKind == QLatin1String("github"))
        addChip(QObject::tr("GITHUB"), true);
    else if (preview.sourceKind == QLatin1String("host"))
        addChip(QObject::tr("HOST"), true);
    else
        addChip(QObject::tr("MOD"), true);
    if (!preview.version.isEmpty())
        addChip(QObject::tr("v%1").arg(preview.version), false);
    if (!preview.author.isEmpty())
        addChip(preview.author, false);
    const auto game = GameCatalog::byCode(preview.gameCode);
    addChip(game.title.isEmpty() ? preview.gameCode.toUpper() : game.title, false);
    chips->addStretch();
    head->addLayout(chips);
    hero->addLayout(head, 1);
    root->addLayout(hero);

    auto *desc = new QTextEdit(&dlg);
    desc->setReadOnly(true);
    desc->setFixedHeight(88);
    desc->setText(preview.description.isEmpty() ? QObject::tr("No description.") : preview.description);
    root->addWidget(desc);

    if (preview.replacesExisting) {
        auto *upd = new QFrame(&dlg);
        upd->setObjectName("CllCard");
        auto *ul = new QVBoxLayout(upd);
        ul->setSpacing(10);
        auto *wh = new QLabel(QObject::tr("Este mod (ou uma versao dele) ja esta instalado."), upd);
        wh->setStyleSheet("color:#e89a3a; font-weight:700;");
        ul->addWidget(wh);

        auto *grid = new QGridLayout();
        grid->setHorizontalSpacing(16);
        grid->setVerticalSpacing(6);
        grid->setColumnStretch(1, 1);
        grid->setColumnStretch(2, 1);
        auto *hInstalled = new QLabel(QObject::tr("Instalado agora:"), upd);
        hInstalled->setObjectName("CllDestTo");
        auto *hNew = new QLabel(QObject::tr("Pacote novo:"), upd);
        hNew->setObjectName("CllDestTo");
        grid->addWidget(hInstalled, 0, 1);
        grid->addWidget(hNew, 0, 2);

        auto addRow = [&](int row, const QString &label, const QString &left, const QString &right) {
            auto *k = new QLabel(label, upd);
            k->setObjectName("CllMeta");
            auto *l = new QLabel(left.isEmpty() ? QStringLiteral("—") : left, upd);
            l->setWordWrap(true);
            auto *r = new QLabel(right.isEmpty() ? QStringLiteral("—") : right, upd);
            r->setWordWrap(true);
            grid->addWidget(k, row, 0, Qt::AlignTop);
            grid->addWidget(l, row, 1, Qt::AlignTop);
            grid->addWidget(r, row, 2, Qt::AlignTop);
        };
        addRow(1, QObject::tr("Name"), preview.existingName, preview.name);
        addRow(2, QObject::tr("Version"),
               preview.existingVersion.isEmpty() ? QObject::tr("(sem versao)") : preview.existingVersion,
               preview.version.isEmpty() ? QObject::tr("(sem versao)") : preview.version);
        addRow(3, QObject::tr("Description"),
               preview.existingDescription.isEmpty() ? QStringLiteral("-") : preview.existingDescription,
               preview.description.isEmpty() ? QStringLiteral("-") : preview.description);
        ul->addLayout(grid);
        root->addWidget(upd);
    }

    auto fmt = [](qint64 b) -> QString {
        if (b < 0)
            return QStringLiteral("—");
        if (b < 1024)
            return QObject::tr("%1 B").arg(b);
        if (b < 1024 * 1024)
            return QObject::tr("%1 KB").arg(b / 1024.0, 0, 'f', 1);
        if (b < 1024LL * 1024 * 1024)
            return QObject::tr("%1 MB").arg(b / (1024.0 * 1024.0), 0, 'f', 1);
        return QObject::tr("%1 GB").arg(b / (1024.0 * 1024.0 * 1024.0), 0, 'f', 2);
    };

    auto *diskRow = new QHBoxLayout();
    for (const DiskUse &d : preview.disks) {
        auto *card = new QFrame(&dlg);
        card->setObjectName("CllCard");
        auto *vl = new QVBoxLayout(card);
        const bool diskLow = d.available >= 0 && d.available < 25LL * 1024 * 1024 * 1024;
        auto *diskName = new QLabel(d.label + QStringLiteral("  ") + driveLabel(d.rootPath)
                                        + (diskLow ? QStringLiteral(" — ") + QObject::tr("Disk almost full!")
                                                   : QString()), card);
        if (diskLow)
            diskName->setStyleSheet("color:#e45b5b; font-weight:700;");
        vl->addWidget(diskName);
        auto *barHost = new QWidget(card);
        barHost->setFixedHeight(16);
        barHost->setStyleSheet("background:#12131c; border:1px solid #2b2e42; border-radius:6px;");
        auto *barLay = new QHBoxLayout(barHost);
        barLay->setContentsMargins(1, 1, 1, 1);
        barLay->setSpacing(0);
        const qint64 need = d.payload + d.backupEstimate;
        int usedPct = 0, modPct = 0;
        if (d.total > 0) {
            usedPct = int(qBound(qint64(0), d.used * 100 / d.total, qint64(100)));
            modPct = int(qBound(qint64(1), need * 100 / d.total, qint64(100)));
            if (usedPct + modPct > 100)
                usedPct = qMax(0, 100 - modPct);
        } else {
            modPct = 8;
        }
        auto *usedChunk = new QFrame(barHost);
        usedChunk->setStyleSheet(diskLow
                                    ? "background:#c23b3b; border:none; border-top-left-radius:5px; border-bottom-left-radius:5px; border-top-right-radius:0; border-bottom-right-radius:0;"
                                    : "background:#5c5e78; border:none; border-top-left-radius:5px; border-bottom-left-radius:5px; border-top-right-radius:0; border-bottom-right-radius:0;");
        auto *modChunk = new QFrame(barHost);
        modChunk->setStyleSheet("background:#e89a3a; border:none; border-top-left-radius:0; border-bottom-left-radius:0; border-top-right-radius:5px; border-bottom-right-radius:5px;");
        barLay->addWidget(usedChunk, qMax(1, usedPct));
        barLay->addWidget(modChunk, qMax(2, modPct));
        barLay->addStretch(qMax(1, 100 - usedPct - modPct));
        vl->addWidget(barHost);
        auto *diskTxt = new QLabel(card);
        diskTxt->setTextFormat(Qt::RichText);
        diskTxt->setText(QObject::tr("Livre %1 de %2").arg(fmt(d.available), fmt(d.total))
                         + QStringLiteral("<br>")
                         + QStringLiteral("<span style='color:#e89a3a'>%1 %2</span>  ·  <span style='color:#e89a3a'>%3 ~%4</span>")
                               .arg(QObject::tr("Mod"), fmt(d.payload),
                                    QObject::tr("backup"), fmt(d.backupEstimate)));
        vl->addWidget(diskTxt);
        diskRow->addWidget(card);
    }
    root->addLayout(diskRow);

    auto *mapHead = new QHBoxLayout();
    auto *mapTitle = new QLabel(QObject::tr("Install map"), &dlg);
    mapTitle->setObjectName("CllMeta");
    mapHead->addWidget(mapTitle);
    if (!preview.conflicts.isEmpty()) {
        auto *warnChip = new QLabel(QObject::tr("%1 warning(s)").arg(preview.conflicts.size()), &dlg);
        warnChip->setObjectName("CllChipWarn");
        warnChip->setAlignment(Qt::AlignCenter);
        mapHead->addWidget(warnChip);
    }
    mapHead->addStretch();
    root->addLayout(mapHead);

    if (!preview.conflicts.isEmpty()) {
        auto *warnBanner = new QLabel(
            QObject::tr("This pack has warnings — files already used by another mod."), &dlg);
        warnBanner->setObjectName("CllWarnBanner");
        warnBanner->setWordWrap(true);
        root->addWidget(warnBanner);
    }

    if (!preview.mappings.isEmpty() || !preview.conflicts.isEmpty()) {
        auto *moreBtn = new QPushButton(
            preview.conflicts.isEmpty()
                ? QObject::tr("See more")
                : QObject::tr("See more (%1 warning(s))").arg(preview.conflicts.size()),
            &dlg);
        moreBtn->setObjectName("CllMore");
        moreBtn->setCursor(Qt::PointingHandCursor);
        root->addWidget(moreBtn);
        QObject::connect(moreBtn, &QPushButton::clicked, &dlg, [&preview, &dlg, fmt]() {
            QDialog details(&dlg);
            details.setWindowTitle(QObject::tr("Install map"));
            details.setModal(true);
            details.resize(560, 360);
            details.setStyleSheet(dlg.styleSheet());
            auto *vl = new QVBoxLayout(&details);
            vl->setContentsMargins(16, 14, 16, 14);
            vl->setSpacing(8);
            auto *scroll = new QScrollArea(&details);
            scroll->setWidgetResizable(true);
            scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
            auto *inner = new QWidget;
            auto *il = new QVBoxLayout(inner);
            il->setContentsMargins(0, 0, 2, 0);
            il->setSpacing(6);
            if (!preview.mappings.isEmpty()) {
                auto *sec = new QLabel(QObject::tr("Folders to copy"), inner);
                sec->setObjectName("CllDestTo");
                il->addWidget(sec);
                auto *hint = new QLabel(QObject::tr("Each line is a folder in the pack and where it will be installed."), inner);
                hint->setObjectName("CllMeta");
                hint->setWordWrap(true);
                il->addWidget(hint);
            }
            for (const Mapping &m : preview.mappings) {
                auto *card = new QFrame(inner);
                card->setObjectName("CllCard");
                auto *cl = new QHBoxLayout(card);
                cl->setContentsMargins(12, 8, 12, 8);
                auto *from = new QLabel(m.fromRel.isEmpty() ? QStringLiteral(".") : m.fromRel, card);
                from->setObjectName("CllDestFrom");
                from->setWordWrap(true);
                auto *arrow = new QLabel(QStringLiteral("→"), card);
                arrow->setObjectName("CllMeta");
                const QString destRoot = m.destRoot == QLatin1String("game")
                                             ? QObject::tr("Game")
                                             : QObject::tr("Plutonium");
                auto *to = new QLabel(destRoot + " / " + m.destRel
                                          + QStringLiteral("\n")
                                          + QObject::tr("%n arquivo(s)", "", m.files)
                                          + QStringLiteral(" · ") + fmt(m.bytes), card);
                to->setObjectName("CllDestTo");
                to->setWordWrap(true);
                cl->addWidget(from, 1);
                cl->addWidget(arrow, 0);
                cl->addWidget(to, 1);
                il->addWidget(card);
            }
            if (!preview.conflicts.isEmpty()) {
                auto *sec = new QLabel(QObject::tr("Files that already exist"), inner);
                sec->setObjectName("CllWarnBanner");
                il->addWidget(sec);
                auto *hint = new QLabel(QObject::tr("These files will be replaced. A backup is kept if that option is enabled."), inner);
                hint->setObjectName("CllMeta");
                hint->setWordWrap(true);
                il->addWidget(hint);
            }
            for (const auto &c : preview.conflicts) {
                auto *line = new QLabel(QStringLiteral("• %1/%2  (%3)")
                                            .arg(c.destRoot, c.destRel, c.otherMods.join(QStringLiteral(", "))), inner);
                line->setWordWrap(true);
                il->addWidget(line);
            }
            il->addStretch();
            scroll->setWidget(inner);
            vl->addWidget(scroll, 1);
            auto *closeBtn = new QPushButton(QObject::tr("Close"), &details);
            closeBtn->setObjectName("CllCancel");
            closeBtn->setCursor(Qt::PointingHandCursor);
            vl->addWidget(closeBtn, 0, Qt::AlignRight);
            QObject::connect(closeBtn, &QPushButton::clicked, &details, &QDialog::accept);
            details.exec();
        });
    }

    auto *backup = new QCheckBox(QObject::tr("Guardar backup dos arquivos substituidos (compartilhado)"), &dlg);
    backup->setChecked(true);
    root->addWidget(backup);

    auto *row = new QHBoxLayout();
    row->addStretch();
    auto *cancel = new QPushButton(QObject::tr("Cancel"), &dlg);
    cancel->setObjectName("CllCancel");
    auto *ok = new QPushButton(QObject::tr("Install"), &dlg);
    ok->setObjectName("CllInstall");
    ok->setCursor(Qt::PointingHandCursor);
    row->addWidget(cancel);
    row->addWidget(ok);
    root->addLayout(row);
    QObject::connect(cancel, &QPushButton::clicked, &dlg, &QDialog::reject);
    QObject::connect(ok, &QPushButton::clicked, &dlg, &QDialog::accept);

    auto *overlay = new QWidget(&dlg);
    overlay->setObjectName("CompareOverlay");
    overlay->setStyleSheet(
        "QWidget#CompareOverlay { background-color: rgba(12, 14, 22, 210); }"
        "QLabel#CompareTitle { color:#f4f2ff; font-size:16px; font-weight:700; }"
        "QLabel#CompareFile { color:#c8c6d8; font-size:12px; }"
        "QProgressBar { background:#242636; border:none; border-radius:5px; min-height:8px; max-height:8px; }"
        "QProgressBar::chunk { background:#9184d9; border-radius:5px; }");
    overlay->hide();
    auto *ol = new QVBoxLayout(overlay);
    ol->setContentsMargins(40, 24, 40, 24);
    ol->addStretch();
    auto *loadTitle = new QLabel(QObject::tr("Comparing installed files…"), overlay);
    loadTitle->setObjectName("CompareTitle");
    loadTitle->setAlignment(Qt::AlignCenter);
    auto *loadBar = new QProgressBar(overlay);
    loadBar->setRange(0, 0);
    loadBar->setFixedWidth(260);
    auto *loadFile = new QLabel(QObject::tr("Starting…"), overlay);
    loadFile->setObjectName("CompareFile");
    loadFile->setAlignment(Qt::AlignCenter);
    loadFile->setWordWrap(true);
    ol->addWidget(loadTitle, 0, Qt::AlignHCenter);
    ol->addSpacing(14);
    ol->addWidget(loadBar, 0, Qt::AlignHCenter);
    ol->addSpacing(10);
    ol->addWidget(loadFile, 0, Qt::AlignHCenter);
    ol->addStretch();

    struct OverlayFit : public QObject {
        explicit OverlayFit(QObject *parent = nullptr) : QObject(parent) {}
        QWidget *host = nullptr;
        QWidget *cover = nullptr;
        bool eventFilter(QObject *obj, QEvent *ev) override {
            if (obj == host && (ev->type() == QEvent::Resize || ev->type() == QEvent::Show))
                cover->setGeometry(host->rect());
            return QObject::eventFilter(obj, ev);
        }
    };
    auto *fit = new OverlayFit(&dlg);
    fit->host = &dlg;
    fit->cover = overlay;
    dlg.installEventFilter(fit);

    if (preview.needsCompare && !preview.tracked.isEmpty()) {
        ok->setEnabled(false);
        overlay->show();
        overlay->raise();
        QTimer::singleShot(0, &dlg, [&]() {
            overlay->setGeometry(dlg.rect());
            overlay->show();
            overlay->raise();
            auto future = QtConcurrent::run([&preview, loadFile, loadBar]() {
                return GithubModInstaller::runUpdateCheck(preview, [loadFile, loadBar](int pct, const QString &name) {
                    QMetaObject::invokeMethod(loadFile, [loadFile, loadBar, pct, name]() {
                        loadBar->setRange(0, 100);
                        loadBar->setValue(pct);
                        loadFile->setText(QObject::tr("Comparing: %1").arg(name));
                    }, Qt::QueuedConnection);
                });
            });
            auto *done = new QTimer(&dlg);
            QObject::connect(done, &QTimer::timeout, &dlg, [done, future, desc, overlay, ok, &preview]() mutable {
                if (!future.isFinished())
                    return;
                done->stop();
                const QString extra = future.result();
                preview.description = preview.description.isEmpty()
                                         ? extra
                                         : (preview.description + QLatin1String("\n\n") + extra);
                desc->setText(preview.description);
                overlay->hide();
                ok->setEnabled(true);
            });
            done->start(60);
        });
    }

    if (dlg.exec() != QDialog::Accepted)
        return false;
    preview.makeBackup = backup->isChecked();
    return true;
}

QString apply(const Preview &preview,
              const QString &plutoniumRoot,
              const QString &gameRoot,
              const QString &archivePath)
{
    QList<SmartModInstaller::Mapping> maps;
    for (const Mapping &m : preview.mappings) {
        SmartModInstaller::Mapping sm;
        sm.srcAbs = m.fromRel.isEmpty() ? preview.extractedRoot
                                        : preview.extractedRoot + "/" + m.fromRel;
        sm.destRootTag = m.destRoot;
        sm.destRel = m.destRel;
        sm.makeBackup = preview.makeBackup;
        maps.push_back(sm);
    }
    QString id = preview.name;
    id.replace(QRegularExpression(QStringLiteral("[^A-Za-z0-9_.-]")), QStringLiteral("_"));
    if (id.isEmpty())
        id = QFileInfo(archivePath).completeBaseName();
    const QString gameSid = preview.gameCode.isEmpty()
                                ? Storage::gameStorageId(preview.gameId)
                                : preview.gameCode;
    const QString err = SmartModInstaller::applyMappings(maps, plutoniumRoot, gameRoot, gameSid,
                                            id, preview.name, archivePath, preview.makeBackup);
    if (err.isEmpty()) {
        const QString man = QDir(plutoniumRoot).filePath(
            QStringLiteral(".lanlauncher_checkpoints/") + gameSid + "/" + id + "/manifest.json");
        QFile f(man);
        if (f.open(QIODevice::ReadOnly)) {
            QJsonObject o = QJsonDocument::fromJson(f.readAll()).object();
            f.close();
            o.insert(QStringLiteral("name"), preview.name);
            o.insert(QStringLiteral("version"), preview.version);
            o.insert(QStringLiteral("description"), preview.description);
            o.insert(QStringLiteral("author"), preview.author);
            if (f.open(QIODevice::WriteOnly | QIODevice::Truncate))
                f.write(QJsonDocument(o).toJson(QJsonDocument::Indented));
        }
    }
    return err;
}

} // namespace ModPreview
