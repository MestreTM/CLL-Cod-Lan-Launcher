#include "SmartModInstaller.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>

namespace {

bool shouldSkipFile(const QString &rel)
{
    const QString name = QFileInfo(rel).fileName().toLower();
    if (name.endsWith(".log") || name.contains(".log."))
        return true;
    if (name == "missingasset.csv" || name == "thumbs.db" || name == "desktop.ini")
        return true;
    if (name.startsWith(QLatin1String("console_")) && name.contains(QLatin1String(".log")))
        return true;
    return false;
}

void collectFiles(const QString &root, const QString &rel, QStringList &out)
{
    const QString abs = rel.isEmpty() ? root : root + "/" + rel;
    QDir dir(abs);
    if (!dir.exists())
        return;
    const QFileInfoList entries = dir.entryInfoList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QFileInfo &fi : entries) {
        const QString childRel = rel.isEmpty() ? fi.fileName() : rel + "/" + fi.fileName();
        if (fi.isDir())
            collectFiles(root, childRel, out);
        else if (!shouldSkipFile(childRel))
            out << childRel;
    }
}

QString findNamedDir(const QString &root, const QString &name, int depth)
{
    if (depth < 0)
        return QString();
    QDir dir(root);
    if (!dir.exists())
        return QString();
    if (QDir(root).dirName().compare(name, Qt::CaseInsensitive) == 0)
        return QDir::cleanPath(root);
    if (QDir(root + "/" + name).exists())
        return QDir::cleanPath(root + "/" + name);
    if (depth == 0)
        return QString();
    for (const QFileInfo &sub : dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot)) {
        const QString hit = findNamedDir(sub.absoluteFilePath(), name, depth - 1);
        if (!hit.isEmpty())
            return hit;
    }
    return QString();
}

bool forceRemove(const QString &path)
{
    if (!QFileInfo::exists(path))
        return true;
    QFile f(path);
    f.setPermissions(QFile::ReadOwner | QFile::WriteOwner | QFile::ReadUser | QFile::WriteUser);
    return f.remove() || !QFileInfo::exists(path);
}

bool copyOneFile(const QString &src, const QString &dst, QString &error)
{
    QDir().mkpath(QFileInfo(dst).absolutePath());
    forceRemove(dst);
    if (QFile::copy(src, dst))
        return true;
    QFile in(src);
    if (!in.open(QIODevice::ReadOnly)) {
        error = QObject::tr("Nao foi possivel ler %1").arg(src);
        return false;
    }
    QFile out(dst);
    if (!out.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        error = QObject::tr("Nao foi possivel gravar %1").arg(dst);
        return false;
    }
    char buf[1024 * 256];
    while (!in.atEnd()) {
        const qint64 n = in.read(buf, sizeof(buf));
        if (n < 0 || out.write(buf, n) != n) {
            error = QObject::tr("Falha ao copiar %1").arg(QFileInfo(src).fileName());
            return false;
        }
    }
    return true;
}

struct CopyReport {
    QStringList added;
    QStringList replaced;
};

bool copyTreeRelative(const QString &srcRoot, const QString &dstRoot,
                      const QStringList &relativeFiles, const QString &backupRoot,
                      CopyReport &report, QString &error)
{
    for (const QString &rel : relativeFiles) {
        const QString src = srcRoot + "/" + rel;
        const QString dst = dstRoot + "/" + rel;
        if (QFileInfo::exists(dst)) {
            const QString bak = backupRoot + "/" + rel;
            QDir().mkpath(QFileInfo(bak).absolutePath());
            forceRemove(bak);
            QString bakErr;
            if (!copyOneFile(dst, bak, bakErr)) {
                error = QObject::tr("Falha ao guardar original de %1").arg(rel);
                return false;
            }
            report.replaced << rel;
        } else {
            report.added << rel;
        }
        if (!copyOneFile(src, dst, error))
            return false;
    }
    return true;
}

void locateRoots(const QString &extractedRoot, QString &storageRoot, QString &steamRoot)
{
    storageRoot = findNamedDir(extractedRoot, "storage", 4);
    steamRoot = findNamedDir(extractedRoot, "steam", 4);
}

QString suggestedIdFromPlan(const QString &storageRoot, const QString &gameId, const QString &fallback)
{
    if (!storageRoot.isEmpty() && !gameId.isEmpty()) {
        const QString mods = storageRoot + "/" + gameId + "/mods";
        QDir d(mods);
        const auto subs = d.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
        if (!subs.isEmpty())
            return subs.first();
    }
    QString id = fallback;
    id.replace(' ', '_');
    if (id.isEmpty())
        id = QStringLiteral("mod");
    return id;
}

QString checkpointsRoot(const QString &pu, const QString &gameId)
{
    return QDir(pu).filePath(".lanlauncher_checkpoints/" + gameId);
}

QString checkpointDir(const QString &pu, const QString &gameId, const QString &modId)
{
    return checkpointsRoot(pu, gameId) + "/" + modId;
}

QJsonArray toArr(const QStringList &rels, const QString &rootTag)
{
    QJsonArray a;
    for (const QString &rel : rels) {
        QJsonObject o;
        o.insert(QStringLiteral("root"), rootTag);
        o.insert(QStringLiteral("rel"), rel);
        a.append(o);
    }
    return a;
}

QString writeManifest(const QString &dir,
                      const QString &modId,
                      const QString &gameId,
                      const QString &archivePath,
                      const QStringList &addedPu,
                      const QStringList &replacedPu,
                      const QStringList &addedGame,
                      const QStringList &replacedGame)
{
    QDir().mkpath(dir);
    QJsonObject root;
    root.insert(QStringLiteral("id"), modId);
    root.insert(QStringLiteral("game"), gameId);
    root.insert(QStringLiteral("created"), QDateTime::currentDateTime().toString(Qt::ISODate));
    root.insert(QStringLiteral("archive"), archivePath);
    root.insert(QStringLiteral("backupDir"), dir + "/backup");
    QJsonArray added;
    for (const auto &v : toArr(addedPu, QStringLiteral("plutonium")))
        added.append(v);
    for (const auto &v : toArr(addedGame, QStringLiteral("game")))
        added.append(v);
    QJsonArray replaced;
    for (const auto &v : toArr(replacedPu, QStringLiteral("plutonium")))
        replaced.append(v);
    for (const auto &v : toArr(replacedGame, QStringLiteral("game")))
        replaced.append(v);
    root.insert(QStringLiteral("added"), added);
    root.insert(QStringLiteral("replaced"), replaced);

    QFile f(dir + "/manifest.json");
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return QObject::tr("Nao foi possivel gravar o checkpoint em %1").arg(dir);
    f.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    return QString();
}

QString destFor(const QString &rootTag, const QString &rel,
                const QString &pu, const QString &gameRoot)
{
    if (rootTag == QLatin1String("game"))
        return gameRoot + "/" + rel;
    if (rootTag == QLatin1String("pu") || rootTag == QLatin1String("plutonium_root"))
        return pu + "/" + rel;
    if (rel.startsWith(QLatin1String("storage/")))
        return pu + "/" + rel;
    return pu + "/storage/" + rel;
}

void removeEmptyParents(const QString &filePath, const QString &stopAt)
{
    QDir d = QFileInfo(filePath).dir();
    const QString stop = QDir(stopAt).absolutePath();
    while (true) {
        const QString cur = d.absolutePath();
        if (cur == stop || !cur.startsWith(stop))
            break;
        if (!d.entryList(QDir::AllEntries | QDir::NoDotAndDotDot).isEmpty())
            break;
        const QString name = d.dirName();
        if (!d.cdUp())
            break;
        d.rmdir(name);
    }
}

} // namespace

namespace SmartModInstaller {

Plan analyzeExtractedRoot(const QString &extractedRoot)
{
    Plan plan;
    if (!QDir(extractedRoot).exists())
        return plan;

    locateRoots(extractedRoot, plan.storageRoot, plan.steamRoot);
    plan.hasStorage = !plan.storageRoot.isEmpty();
    plan.hasSteam = !plan.steamRoot.isEmpty();
    plan.isSmart = plan.hasStorage || plan.hasSteam;
    if (!plan.isSmart)
        return plan;

    if (plan.hasStorage) {
        for (const QString &id : QStringList{"t6", "t5", "t4", "iw5"}) {
            if (QDir(plan.storageRoot + "/" + id).exists()) {
                plan.storageGameId = id;
                break;
            }
        }
        collectFiles(plan.storageRoot, QString(), plan.storageRelativePaths);
    }
    if (plan.hasSteam)
        collectFiles(plan.steamRoot, QString(), plan.steamRelativePaths);

    plan.suggestedModId = suggestedIdFromPlan(plan.storageRoot, plan.storageGameId, QString());

    QStringList bits;
    if (plan.hasStorage)
        bits << QObject::tr("%1 arquivos em storage/%2")
                    .arg(plan.storageRelativePaths.size())
                    .arg(plan.storageGameId.isEmpty() ? QStringLiteral("?") : plan.storageGameId);
    if (plan.hasSteam)
        bits << QObject::tr("%1 arquivos na pasta do jogo (steam/)").arg(plan.steamRelativePaths.size());
    plan.summary = bits.join(" · ");
    return plan;
}

QString applyPlan(const Plan &plan,
                  const QString &extractedRoot,
                  const QString &plutoniumRoot,
                  const QString &gameRoot,
                  const QString &modIdIn,
                  const QString &archivePath,
                  QString *warningOut)
{
    if (!plan.isSmart)
        return QObject::tr("Pacote nao reconhecido (precisa de pastas storage/ e/ou steam/).");
    if (plutoniumRoot.isEmpty())
        return QObject::tr("Pasta do Plutonium nao configurada (Configuracoes).");

    const QString pu = QDir::cleanPath(QFileInfo(plutoniumRoot).absoluteFilePath());
    const QString gameAbs = gameRoot.isEmpty()
                                ? QString()
                                : QDir::cleanPath(QFileInfo(gameRoot).absoluteFilePath());

    QString storageRoot = plan.storageRoot;
    QString steamRoot = plan.steamRoot;
    if (storageRoot.isEmpty() || steamRoot.isEmpty())
        locateRoots(extractedRoot, storageRoot, steamRoot);

    const QString gameId = plan.storageGameId.isEmpty() ? QStringLiteral("t6") : plan.storageGameId;
    QString modId = modIdIn.isEmpty()
                        ? suggestedIdFromPlan(storageRoot, gameId, QFileInfo(archivePath).completeBaseName())
                        : modIdIn;
    modId.replace(' ', '_');

    const QString ckpt = checkpointDir(pu, gameId, modId);
    if (QDir(ckpt).exists())
        QDir(ckpt).removeRecursively();
    const QString backupPu = ckpt + "/backup/plutonium";
    const QString backupGame = ckpt + "/backup/game";
    QDir().mkpath(backupPu);
    QDir().mkpath(backupGame);

    QString error;
    QStringList warnings;
    CopyReport puReport;
    CopyReport gameReport;

    if (plan.hasStorage && !storageRoot.isEmpty()) {
        QDir().mkpath(pu + "/storage");
        QStringList files;
        collectFiles(storageRoot, QString(), files);
        if (!copyTreeRelative(storageRoot, pu + "/storage", files, backupPu, puReport, error))
            return error;
    }

    if (plan.hasSteam && !steamRoot.isEmpty()) {
        if (gameAbs.isEmpty()) {
            warnings << QObject::tr("Arquivos steam/ ignorados: pasta do jogo nao configurada.");
        } else {
            QStringList files;
            collectFiles(steamRoot, QString(), files);
            if (!copyTreeRelative(steamRoot, gameAbs, files, backupGame, gameReport, error))
                return error;
        }
    }

    const QString werr = writeManifest(ckpt, modId, gameId, archivePath,
                                       puReport.added, puReport.replaced,
                                       gameReport.added, gameReport.replaced);
    if (!werr.isEmpty())
        return werr;

    if (warningOut)
        *warningOut = warnings.join('\n');
    return QString();
}

QString recordStandardInstall(const QString &plutoniumRoot,
                              const QString &gameStorageId,
                              const QString &modFolderAbs,
                              const QString &modId,
                              const QString &archivePath)
{
    if (plutoniumRoot.isEmpty() || gameStorageId.isEmpty() || modId.isEmpty())
        return QObject::tr("Checkpoint: dados incompletos.");
    const QString ckpt = checkpointDir(plutoniumRoot, gameStorageId, modId);
    if (QDir(ckpt).exists())
        QDir(ckpt).removeRecursively();
    QDir().mkpath(ckpt + "/backup/plutonium");

    QStringList files;
    collectFiles(modFolderAbs, QString(), files);
    // Relative to storage/<game>/mods/<modId>
    QStringList added;
    for (const QString &rel : files)
        added << (gameStorageId + "/mods/" + modId + "/" + rel);

    return writeManifest(ckpt, modId, gameStorageId, archivePath, added, {}, {}, {});
}

bool hasCheckpoint(const QString &plutoniumRoot, const QString &gameStorageId, const QString &modId)
{
    return QFileInfo::exists(checkpointDir(plutoniumRoot, gameStorageId, modId) + "/manifest.json");
}

QStringList checkpointIds(const QString &plutoniumRoot, const QString &gameStorageId)
{
    QDir d(checkpointsRoot(plutoniumRoot, gameStorageId));
    if (!d.exists())
        return {};
    return d.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
}

static QJsonObject readCheckpointManifest(const QString &plutoniumRoot,
                                          const QString &gameStorageId,
                                          const QString &modId)
{
    QFile f(checkpointDir(plutoniumRoot, gameStorageId, modId) + "/manifest.json");
    if (!f.open(QIODevice::ReadOnly))
        return {};
    return QJsonDocument::fromJson(f.readAll()).object();
}

QString checkpointDisplayName(const QString &plutoniumRoot, const QString &gameStorageId, const QString &modId)
{
    const QJsonObject o = readCheckpointManifest(plutoniumRoot, gameStorageId, modId);
    const QString name = o.value(QStringLiteral("displayName")).toString().trimmed();
    if (!name.isEmpty())
        return name;
    const QString alt = o.value(QStringLiteral("name")).toString().trimmed();
    return alt.isEmpty() ? modId : alt;
}

QStringList checkpointOwnedModFolders(const QString &plutoniumRoot, const QString &gameStorageId, const QString &modId)
{
    const QJsonObject o = readCheckpointManifest(plutoniumRoot, gameStorageId, modId);
    QStringList owned;
    const QJsonArray explicitFolders = o.value(QStringLiteral("modFolders")).toArray();
    for (const QJsonValue &v : explicitFolders) {
        const QString folder = v.toString().trimmed();
        if (!folder.isEmpty() && !owned.contains(folder, Qt::CaseInsensitive))
            owned << folder;
    }
    auto takeFolder = [&](const QString &relIn) {
        QString rel = relIn;
        rel.replace(QLatin1Char('\\'), QLatin1Char('/'));
        const int i = rel.indexOf(QStringLiteral("/mods/"));
        const int j = rel.startsWith(QStringLiteral("mods/")) ? 0 : i;
        if (j < 0 && i < 0 && !rel.startsWith(QStringLiteral("mods/")))
            return;
        const int cut = rel.startsWith(QStringLiteral("mods/")) ? 5 : (i + 6);
        const QString folder = rel.mid(cut).section(QLatin1Char('/'), 0, 0);
        if (!folder.isEmpty() && !owned.contains(folder, Qt::CaseInsensitive))
            owned << folder;
    };
    auto scan = [&](const QJsonArray &arr) {
        for (const QJsonValue &v : arr)
            takeFolder(v.toObject().value(QStringLiteral("rel")).toString());
    };
    scan(o.value(QStringLiteral("added")).toArray());
    scan(o.value(QStringLiteral("replaced")).toArray());
    return owned;
}

QString rollback(const QString &plutoniumRoot,
                 const QString &gameRoot,
                 const QString &gameStorageId,
                 const QString &modId)
{
    const QString ckpt = checkpointDir(plutoniumRoot, gameStorageId, modId);
    const QString manPath = ckpt + "/manifest.json";
    if (!QFileInfo::exists(manPath))
        return QString(); // nada a reverter alem da pasta do mod

    QFile f(manPath);
    if (!f.open(QIODevice::ReadOnly))
        return QObject::tr("Nao foi possivel ler o checkpoint de %1").arg(modId);
    const QJsonObject root = QJsonDocument::fromJson(f.readAll()).object();
    f.close();

    const QString backupDir = root.value(QStringLiteral("backupDir")).toString(ckpt + "/backup");
    QString error;

    auto restoreReplaced = [&](const QJsonArray &arr) -> bool {
        for (const QJsonValue &v : arr) {
            const QJsonObject o = v.toObject();
            const QString tag = o.value(QStringLiteral("root")).toString();
            const QString rel = o.value(QStringLiteral("rel")).toString();
            const QString dst = destFor(tag, rel, plutoniumRoot, gameRoot);
            const QString bak = backupDir + "/" + tag + "/" + rel;
            if (!QFileInfo::exists(bak))
                continue;
            if (tag == QLatin1String("game") && gameRoot.isEmpty())
                continue;
            if (!copyOneFile(bak, dst, error))
                return false;
        }
        return true;
    };

    auto removeAdded = [&](const QJsonArray &arr) {
        for (const QJsonValue &v : arr) {
            const QJsonObject o = v.toObject();
            const QString tag = o.value(QStringLiteral("root")).toString();
            const QString rel = o.value(QStringLiteral("rel")).toString();
            const QString dst = destFor(tag, rel, plutoniumRoot, gameRoot);
            if (tag == QLatin1String("game") && gameRoot.isEmpty())
                continue;
            forceRemove(dst);
            const QString stop = (tag == QLatin1String("game")) ? gameRoot : (plutoniumRoot + "/storage");
            removeEmptyParents(dst, stop);
        }
    };

    if (!restoreReplaced(root.value(QStringLiteral("replaced")).toArray()))
        return error;

    removeAdded(root.value(QStringLiteral("added")).toArray());

    QDir(ckpt).removeRecursively();
    return QString();
}

} // namespace SmartModInstaller
