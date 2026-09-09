#include "GithubModInstaller.h"
#include "ArchiveTool.h"
#include "Downloader.h"
#include "GameCatalog.h"
#include "Storage.h"

#include <QCryptographicHash>
#include <QHash>


#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QStorageInfo>
#include <QSet>
#include <QThread>
#include <QUrl>

namespace {

QString slash(QString s)
{
    s.replace(QLatin1Char('\\'), QLatin1Char('/'));
    while (s.startsWith(QLatin1Char('/')))
        s.remove(0, 1);
    return s;
}

QString fileHash(const QString &path, QCryptographicHash::Algorithm algo)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return QString();
    QCryptographicHash h(algo);
    while (!f.atEnd())
        h.addData(f.read(1 << 16));
    return QString::fromLatin1(h.result().toHex());
}

QString fileSha256(const QString &path)
{
    return fileHash(path, QCryptographicHash::Sha256);
}

QString fileMd5(const QString &path)
{
    return fileHash(path, QCryptographicHash::Md5);
}

bool fileMatches(const QString &path, const QString &md5, const QString &sha256)
{
    if (!QFileInfo::exists(path))
        return false;
    if (!md5.isEmpty() && fileMd5(path) != md5.toLower())
        return false;
    if (!sha256.isEmpty() && fileSha256(path) != sha256.toLower())
        return false;
    if (md5.isEmpty() && sha256.isEmpty())
        return QFileInfo(path).isFile();
    return true;
}

QString gameCodeFromToken(QString token)
{
    token = token.toLower();
    token.replace(QLatin1Char('\\'), QLatin1Char('/'));
    if (token.contains(QLatin1String("pluto_iw5")) || token.contains(QLatin1String("iw5"))
        || token.contains(QLatin1String("mw3")))
        return QStringLiteral("iw5");
    if (token.contains(QLatin1String("pluto_t5")) || token.contains(QLatin1String("t5"))
        || token.contains(QLatin1String("bo1")))
        return QStringLiteral("t5");
    if (token.contains(QLatin1String("pluto_t4")) || token.contains(QLatin1String("t4"))
        || token.contains(QLatin1String("waw")))
        return QStringLiteral("t4");
    if (token.contains(QLatin1String("pluto_t6")) || token.contains(QLatin1String("t6"))
        || token.contains(QLatin1String("bo2")))
        return QStringLiteral("t6");
    return QString();
}

bool isSteamRoot(const QString &root, const QString &kind)
{
    if (kind.compare(QLatin1String("steam-app"), Qt::CaseInsensitive) == 0)
        return true;
    const QString r = root.toUpper();
    return r == QLatin1String("BO2") || r == QLatin1String("BO1")
           || r == QLatin1String("WAW") || r == QLatin1String("MW3")
           || r == QLatin1String("T6") || r == QLatin1String("T5")
           || r == QLatin1String("T4") || r == QLatin1String("IW5")
           || r.startsWith(QLatin1String("STEAM"));
}

bool isPlutoRoot(const QString &root)
{
    const QString r = root.toUpper();
    return r == QLatin1String("PLUTO_T6") || r == QLatin1String("PLUTO_T5")
           || r == QLatin1String("PLUTO_T4") || r == QLatin1String("PLUTO_IW5");
}

QString siblingReleaseUrl(const QString &manifestUrl, const QString &fileName)
{
    QString u = manifestUrl;
    const int q = u.indexOf(QLatin1Char('?'));
    if (q >= 0)
        u = u.left(q);
    if (u.endsWith(QLatin1String("/manifest.json"), Qt::CaseInsensitive))
        return u.left(u.size() - int(QStringLiteral("/manifest.json").size())) + QLatin1Char('/') + fileName;
    if (u.endsWith(QLatin1String("manifest.json"), Qt::CaseInsensitive))
        return u.left(u.size() - int(QStringLiteral("manifest.json").size())) + fileName;
    const int slash = u.lastIndexOf(QLatin1Char('/'));
    if (slash >= 0)
        return u.left(slash + 1) + fileName;
    return QString();
}

void mapDestination(const QString &root, const QString &kind, const QString &rel,
                    const QString &gameCode, QString &destRoot, QString &destRel)
{
    const QString cleanRel = slash(rel);
    if (isSteamRoot(root, kind)) {
        destRoot = QStringLiteral("game");
        destRel = cleanRel;
        return;
    }
    destRoot = QStringLiteral("plutonium");
    const QString code = gameCode.isEmpty() ? gameCodeFromToken(root) : gameCode;
    const QString codeOrT6 = code.isEmpty() ? QStringLiteral("t6") : code;
    if (cleanRel.startsWith(QLatin1String("storage/"), Qt::CaseInsensitive)) {
        destRel = cleanRel;
        return;
    }
    destRel = QStringLiteral("storage/") + codeOrT6 + QLatin1Char('/') + cleanRel;
}

QString bucketKey(const QString &destRoot, const QString &destRel)
{
    const QStringList parts = destRel.split(QLatin1Char('/'), Qt::SkipEmptyParts);
    if (destRoot == QLatin1String("game")) {
        if (!parts.isEmpty())
            return destRoot + QLatin1Char('|') + parts.first();
        return destRoot + QLatin1Char('|');
    }
    // storage/<game>/<top>
    if (parts.size() >= 3)
        return destRoot + QLatin1Char('|') + QStringList(parts.mid(0, 3)).join(QLatin1Char('/'));
    return destRoot + QLatin1Char('|') + destRel;
}

int timeoutForSize(qint64 size)
{
    if (size >= 200LL * 1024 * 1024)
        return 90 * 60 * 1000;
    if (size >= 40LL * 1024 * 1024)
        return 30 * 60 * 1000;
    return 180 * 1000;
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

bool isUnsafeBinaryName(const QString &path)
{
    const QString ext = QFileInfo(path).suffix().toLower();
    return ext == QLatin1String("exe") || ext == QLatin1String("dll")
           || ext == QLatin1String("bat") || ext == QLatin1String("cmd")
           || ext == QLatin1String("msi") || ext == QLatin1String("scr")
           || ext == QLatin1String("com") || ext == QLatin1String("ps1");
}

bool copyFileOverwrite(const QString &src, const QString &dst, QString &error)
{
    QDir().mkpath(QFileInfo(dst).absolutePath());
    if (QFileInfo::exists(dst))
        QFile::remove(dst);
    if (QFile::copy(src, dst))
        return true;
    error = QObject::tr("Could not copy %1").arg(src);
    return false;
}

QString fileKey(const QString &root, const QString &rel)
{
    return root + QLatin1Char('|') + slash(rel);
}

QJsonObject loadDownloadJson(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return {};
    return QJsonDocument::fromJson(f.readAll()).object();
}

QString findDownloadJsonPath(const QString &pu, const QString &gameAbs, const QString &gameCode,
                             const GithubModInstaller::Manifest &man)
{
    QString fromModJson;
    for (const auto &f : man.files) {
        if (QFileInfo(f.relpath).fileName().compare(QLatin1String("mod.json"), Qt::CaseInsensitive) != 0)
            continue;
        QString tag, destRel;
        mapDestination(f.root, QString(), f.relpath, gameCode, tag, destRel);
        const QString dst = (tag == QLatin1String("game") ? gameAbs : pu) + QLatin1Char('/') + destRel;
        fromModJson = QFileInfo(dst).absolutePath() + QStringLiteral("/download.json");
        break;
    }
    if (!fromModJson.isEmpty() && QFileInfo::exists(fromModJson))
        return fromModJson;

    const QString mods = pu + "/storage/" + gameCode + "/mods";
    for (const QString &folder : QDir(mods).entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
        const QString cand = mods + "/" + folder + "/download.json";
        if (!QFileInfo::exists(cand))
            continue;
        const QJsonObject o = loadDownloadJson(cand);
        const QString src = o.value(QStringLiteral("sourceUrl")).toString();
        if (!src.isEmpty() && !man.sourceUrl.isEmpty() && src.compare(man.sourceUrl, Qt::CaseInsensitive) == 0)
            return cand;
    }
    if (!fromModJson.isEmpty())
        return fromModJson;
    return pu + "/storage/" + gameCode + "/.cll_host/" + (man.releaseTag.isEmpty() ? QStringLiteral("pack") : man.releaseTag)
           + "/download.json";
}

} // namespace

namespace GithubModInstaller {

bool looksLikeUrl(const QString &text)
{
    const QString t = text.trimmed();
    if (t.size() >= 2 && t[1] == QLatin1Char(':'))
        return false;
    if (t.startsWith(QLatin1String("\\\\")))
        return false;
    if (t.startsWith(QLatin1String("http://"), Qt::CaseInsensitive)
        || t.startsWith(QLatin1String("https://"), Qt::CaseInsensitive))
        return true;
    if (t.contains(QLatin1String("github.com"), Qt::CaseInsensitive))
        return true;
    if (t.contains(QLatin1String("manifest.json"), Qt::CaseInsensitive))
        return true;
    if (QRegularExpression(QStringLiteral(R"(^[\w.-]+/[\w.-]+$)")).match(t).hasMatch())
        return true;
    return QRegularExpression(QStringLiteral(R"(^[\w.-]+\.[A-Za-z]{2,}(/.*)?$)")).match(t).hasMatch();
}

QString resolveManifestUrl(const QString &input, QString *error)
{
    QString t = input.trimmed();
    t.replace(QLatin1Char('\\'), QLatin1Char('/'));
    if (t.endsWith(QLatin1Char('*')))
        t.chop(1);
    while (t.endsWith(QLatin1Char('/')))
        t.chop(1);

    if (!t.startsWith(QLatin1String("http://"), Qt::CaseInsensitive)
        && !t.startsWith(QLatin1String("https://"), Qt::CaseInsensitive)
        && (t.contains(QLatin1Char('.')) || t.contains(QLatin1String("github.com"), Qt::CaseInsensitive)))
        t = QStringLiteral("https://") + t;

    if (t.endsWith(QLatin1String("manifest.json"), Qt::CaseInsensitive))
        return t;

    QRegularExpression gh(QStringLiteral(R"(github\.com/([^/]+)/([^/]+))"),
                          QRegularExpression::CaseInsensitiveOption);
    const auto m = gh.match(t);
    if (m.hasMatch()) {
        QString owner = m.captured(1);
        QString repo = m.captured(2);
        if (repo.endsWith(QLatin1String(".git"), Qt::CaseInsensitive))
            repo.chop(4);
        return QStringLiteral("https://github.com/%1/%2/releases/latest/download/manifest.json")
            .arg(owner, repo);
    }

    if (t.contains(QLatin1Char('.')) && t.contains(QLatin1Char('/')))
        return t + QStringLiteral("/manifest.json");

    const auto parts = t.split(QLatin1Char('/'), Qt::SkipEmptyParts);
    if (parts.size() >= 2 && !parts[0].contains(QLatin1Char('.'))) {
        QString owner = parts[0];
        QString repo = parts[1];
        if (repo.endsWith(QLatin1String(".git"), Qt::CaseInsensitive))
            repo.chop(4);
        return QStringLiteral("https://github.com/%1/%2/releases/latest/download/manifest.json")
            .arg(owner, repo);
    }

    if (error)
        *error = QObject::tr("Could not read a pack from this link.");
    return QString();
}

Manifest parseBytes(const QByteArray &json, const QString &sourceUrl)
{
    Manifest man;
    man.sourceUrl = sourceUrl;
    const auto doc = QJsonDocument::fromJson(json);
    if (!doc.isObject()) {
        man.error = QObject::tr("Invalid GitHub manifest.");
        return man;
    }
    const QJsonObject root = doc.object();
    man.releaseTag = root.value(QStringLiteral("releaseTag")).toString();
    man.releaseVersion = root.value(QStringLiteral("releaseVersion")).toString();
    if (man.releaseVersion.isEmpty())
        man.releaseVersion = man.releaseTag;
    man.layoutId = root.value(QStringLiteral("layoutId")).toString();
    man.shortHash = root.value(QStringLiteral("shortHash")).toString();
    {
        const QJsonObject b = root.value(QStringLiteral("bundle")).toObject();
        man.bundle.assetName = b.value(QStringLiteral("assetName")).toString();
        man.bundle.url = b.value(QStringLiteral("url")).toString().trimmed();
        man.bundle.size = qint64(b.value(QStringLiteral("size")).toDouble());
        man.bundle.sha256 = b.value(QStringLiteral("sha256")).toString().trimmed().toLower();
        if (man.bundle.url.isEmpty())
            man.bundle.url = b.value(QStringLiteral("download")).toString().trimmed();
        if (man.bundle.assetName.isEmpty() && !man.bundle.url.isEmpty())
            man.bundle.assetName = QFileInfo(QUrl(man.bundle.url).path()).fileName();
    }

    QHash<QString, QString> kinds;
    const QJsonObject roots = root.value(QStringLiteral("roots")).toObject();
    for (auto it = roots.begin(); it != roots.end(); ++it) {
        const QJsonObject o = it.value().toObject();
        kinds.insert(it.key(), o.value(QStringLiteral("kind")).toString());
        const QString code = gameCodeFromToken(it.key() + o.value(QStringLiteral("kind")).toString());
        if (isPlutoRoot(it.key()) && !code.isEmpty())
            man.gameCode = code;
        else if (!code.isEmpty() && man.gameCode.isEmpty())
            man.gameCode = code;
    }

    const QJsonArray files = root.value(QStringLiteral("files")).toArray();
    for (const QJsonValue &v : files) {
        const QJsonObject o = v.toObject();
        RemoteFile f;
        f.relpath = slash(o.value(QStringLiteral("relpath")).toString());
        f.root = o.value(QStringLiteral("root")).toString();
        f.size = qint64(o.value(QStringLiteral("size")).toDouble());
        f.md5 = o.value(QStringLiteral("md5")).toString().trimmed().toLower();
        f.sha256 = o.value(QStringLiteral("sha256")).toString().trimmed().toLower();
        const QJsonArray channels = o.value(QStringLiteral("channels")).toArray();
        for (const QJsonValue &cv : channels) {
            const QJsonObject c = cv.toObject();
            const QString url = c.value(QStringLiteral("url")).toString();
            if (!url.isEmpty()) {
                f.url = url;
                break;
            }
        }
        if (f.relpath.isEmpty() || f.url.isEmpty())
            continue;
        if (man.gameCode.isEmpty())
            man.gameCode = gameCodeFromToken(f.root);
        man.files.push_back(f);
        if (isUnsafeBinaryName(f.relpath) || isUnsafeBinaryName(f.url)) {
            man.hasExecutables = true;
            const QString n = QFileInfo(f.relpath).fileName();
            if (!n.isEmpty() && !man.executableNames.contains(n))
                man.executableNames << n;
        }
    }

    const QJsonArray retire = root.value(QStringLiteral("retire")).toArray();
    for (const QJsonValue &v : retire) {
        const QJsonObject o = v.toObject();
        RetireFile r;
        r.relpath = slash(o.value(QStringLiteral("relpath")).toString());
        r.root = o.value(QStringLiteral("root")).toString();
        r.note = o.value(QStringLiteral("note")).toString();
        if (!r.relpath.isEmpty())
            man.retire.push_back(r);
    }

    if (man.gameCode.isEmpty())
        man.gameCode = QStringLiteral("t6");
    man.gameId = GameCatalog::byCode(man.gameCode).id;
    man.valid = !man.files.isEmpty() || man.bundle.valid();
    if (!man.valid)
        man.error = QObject::tr("The GitHub manifest has no files.");
    Q_UNUSED(kinds);
    return man;
}

Manifest fetchManifest(const QString &input, QString &error)
{
    const QString url = resolveManifestUrl(input, &error);
    if (url.isEmpty())
        return {};
    const QByteArray raw = Downloader::downloadBytes(url, error, 60000);
    if (raw.isEmpty()) {
        if (error.isEmpty())
            error = QObject::tr("Could not download manifest.json.");
        return {};
    }
    Manifest man = parseBytes(raw, url);
    if (!man.valid && error.isEmpty())
        error = man.error;
    return man;
}

Manifest peek(const QString &input,
              const QString &fallbackGameId,
              const QString &fallbackGameCode,
              const QString &plutoniumRoot,
              const QString &gameRoot,
              ModPreview::Preview *previewOut,
              QString &error)
{
    Manifest man = fetchManifest(input, error);
    if (!man.valid)
        return man;
    if (man.gameCode.isEmpty())
        man.gameCode = fallbackGameCode.isEmpty() ? QStringLiteral("t6") : fallbackGameCode;
    if (man.gameId.isEmpty())
        man.gameId = fallbackGameId.isEmpty() ? GameCatalog::byCode(man.gameCode).id : fallbackGameId;

    ModPreview::Preview p;
    p.gameCode = man.gameCode;
    p.gameId = man.gameId;
    p.sourceKind = man.sourceUrl.contains(QLatin1String("github.com"), Qt::CaseInsensitive)
                       ? QStringLiteral("github")
                       : QStringLiteral("host");
    p.version = man.releaseVersion;
    p.name = QUrl(man.sourceUrl).path().section(QLatin1Char('/'), 2, 2);
    if (p.name.isEmpty())
        p.name = QStringLiteral("GitHub pack");

    auto applyModJson = [&](const QByteArray &raw) {
        if (raw.isEmpty())
            return false;
        const auto doc = QJsonDocument::fromJson(raw);
        if (!doc.isObject())
            return false;
        const QJsonObject o = doc.object();
        auto take = [](const QJsonValue &v) {
            QString s = v.toString();
            s.replace(QRegularExpression(QStringLiteral(R"(\^[0-9])")), QString());
            return s.trimmed();
        };
        if (!take(o.value(QStringLiteral("name"))).isEmpty())
            p.name = take(o.value(QStringLiteral("name")));
        if (!take(o.value(QStringLiteral("version"))).isEmpty())
            p.version = take(o.value(QStringLiteral("version")));
        p.author = take(o.value(QStringLiteral("author")));
        p.description = take(o.value(QStringLiteral("description")));
        return true;
    };

    QStringList modJsonUrls;
    for (const RemoteFile &f : man.files) {
        if (QFileInfo(f.relpath).fileName().compare(QLatin1String("mod.json"), Qt::CaseInsensitive) == 0
            && !f.url.isEmpty())
            modJsonUrls << f.url;
    }
    const QString sibling = siblingReleaseUrl(man.sourceUrl, QStringLiteral("mod.json"));
    if (!sibling.isEmpty() && !modJsonUrls.contains(sibling))
        modJsonUrls << sibling;
    for (const QString &url : modJsonUrls) {
        QString dj;
        if (applyModJson(Downloader::downloadBytes(url, dj, 60000)))
            break;
    }

    struct Bucket { QString fromRel; QString destRoot; QString destRel; qint64 bytes = 0; int files = 0; };
    QHash<QString, Bucket> buckets;
    QList<QPair<QString, QString>> destFiles;
    QHash<QString, QString> kinds; // unused after parse; infer steam by root name

    for (const RemoteFile &f : man.files) {
        QString destRoot, destRel;
        mapDestination(f.root, QString(), f.relpath, p.gameCode, destRoot, destRel);
        const QString key = bucketKey(destRoot, destRel);
        Bucket &b = buckets[key];
        b.destRoot = destRoot;
        b.destRel = key.section(QLatin1Char('|'), 1);
        b.fromRel = f.relpath.section(QLatin1Char('/'), 0, b.destRel.split(QLatin1Char('/')).size() - (destRoot == QLatin1String("game") ? 1 : 3));
        if (b.fromRel.isEmpty())
            b.fromRel = f.relpath.section(QLatin1Char('/'), 0, 0);
        b.bytes += f.size;
        b.files += 1;
        destFiles.append(qMakePair(destRoot, destRel));
    }

    for (auto it = buckets.begin(); it != buckets.end(); ++it) {
        ModPreview::Mapping m;
        m.fromRel = it->fromRel;
        m.destRoot = it->destRoot;
        m.destRel = it->destRel;
        m.bytes = it->bytes;
        m.files = it->files;
        p.mappings.push_back(m);
        p.totalPayload += m.bytes;
    }

    if (man.bundle.valid()) {
        p.bundleMode = true;
        p.bundleSha256 = man.bundle.sha256;
        p.shortHash = man.shortHash;
        if (p.mappings.isEmpty()) {
            ModPreview::Mapping pu;
            pu.fromRel = QStringLiteral("Plutonium");
            pu.destRoot = QStringLiteral("plutonium");
            pu.destRel = QStringLiteral("storage/") + p.gameCode;
            pu.bytes = man.bundle.size;
            pu.files = 1;
            p.mappings.push_back(pu);
            ModPreview::Mapping gm;
            gm.fromRel = QStringLiteral("steam / BO2");
            gm.destRoot = QStringLiteral("game");
            gm.destRel = QStringLiteral(".");
            gm.bytes = 0;
            gm.files = 1;
            p.mappings.push_back(gm);
            p.totalPayload = man.bundle.size;
        } else if (p.totalPayload <= 0) {
            p.totalPayload = man.bundle.size;
        }
        if (!man.bundle.assetName.isEmpty()) {
            const QString note = QObject::tr("Compressed pack: %1").arg(man.bundle.assetName);
            if (p.description.isEmpty())
                p.description = note;
            else if (!p.description.contains(note))
                p.description += QLatin1String("\n") + note;
        }
    }
    p.shortHash = man.shortHash;
    p.bundleSha256 = man.bundle.sha256;
    p.bundleMode = man.bundle.valid();

    QHash<QString, ModPreview::DiskUse> disks;
    auto addDisk = [&](const QString &path, const QString &label, qint64 payload) {
        if (path.isEmpty())
            return;
        const QStorageInfo st(path);
        const QString id = st.rootPath().isEmpty() ? path : st.rootPath();
        ModPreview::DiskUse &d = disks[id];
        d.rootPath = path;
        d.label = label;
        d.available = st.bytesAvailable();
        d.total = st.bytesTotal();
        d.used = (d.total > 0 && d.available >= 0) ? (d.total - d.available) : 0;
        d.payload += payload;
        d.backupEstimate += payload / 4;
    };
    for (const auto &m : p.mappings) {
        addDisk(m.destRoot == QLatin1String("game") ? gameRoot : plutoniumRoot,
                m.destRoot == QLatin1String("game") ? QObject::tr("Jogo") : QObject::tr("Plutonium"),
                m.bytes);
        p.totalBackupEstimate += m.bytes / 4;
    }
    p.disks = disks.values();

    if (p.mappings.isEmpty()) {
        p.error = QObject::tr("Nao foi possivel projetar a instalacao deste pacote.");
        man.valid = false;
        man.error = p.error;
    }
    const QString statePath = findDownloadJsonPath(plutoniumRoot, gameRoot, p.gameCode, man);
    p.compareStatePath = statePath;
    for (const RemoteFile &f : man.files) {
        QString tag, destRel;
        mapDestination(f.root, QString(), f.relpath, p.gameCode, tag, destRel);
        ModPreview::Preview::TrackedFile t;
        t.name = QFileInfo(f.relpath).fileName();
        t.destPath = (tag == QLatin1String("game") ? gameRoot : plutoniumRoot) + QLatin1Char('/') + destRel;
        t.root = f.root;
        t.relpath = f.relpath;
        t.md5 = f.md5;
        t.sha256 = f.sha256;
        p.tracked.push_back(t);
    }
    if (QFileInfo::exists(statePath)) {
        const QJsonObject prev = loadDownloadJson(statePath);
        p.needsCompare = true;
        p.replacesExisting = true;
        p.existingName = p.name;
        p.existingVersion = prev.value(QStringLiteral("releaseVersion")).toString();
        p.existingDescription = prev.value(QStringLiteral("sourceUrl")).toString();
    }

    if (previewOut)
        *previewOut = p;
    Q_UNUSED(kinds);
    return man;
}

QString runUpdateCheck(ModPreview::Preview &preview, const ProgressFn &onProgress)
{
    const QJsonObject prev = loadDownloadJson(preview.compareStatePath);
    if (preview.bundleMode) {
        if (onProgress)
            onProgress(100, QObject::tr("Comparing bundle"));
        const QString prevHash = prev.value(QStringLiteral("shortHash")).toString();
        const QString prevSha = prev.value(QStringLiteral("bundleSha256")).toString().toLower();
        const bool hashOk = !preview.shortHash.isEmpty() && prevHash == preview.shortHash;
        const bool shaOk = preview.bundleSha256.isEmpty()
                           || prevSha == preview.bundleSha256.toLower();
        if (hashOk && shaOk)
            return QObject::tr("Update check: bundle already up to date.");
        return QObject::tr("Update check: new compressed pack available.");
    }
    QSet<QString> newKeys;
    int missing = 0, changed = 0, upToDate = 0;
    const int total = preview.tracked.size();
    int i = 0;
    for (const auto &t : preview.tracked) {
        ++i;
        newKeys.insert(fileKey(t.root, t.relpath));
        if (onProgress) {
            const int pct = total > 0 ? int(i * 100 / total) : 0;
            onProgress(pct, t.name);
        }
        if (!QFileInfo::exists(t.destPath))
            ++missing;
        else if (!fileMatches(t.destPath, t.md5, t.sha256))
            ++changed;
        else
            ++upToDate;
    }
    int removed = 0;
    for (const QJsonValue &v : prev.value(QStringLiteral("files")).toArray()) {
        const QJsonObject o = v.toObject();
        if (!newKeys.contains(fileKey(o.value(QStringLiteral("root")).toString(),
                                      o.value(QStringLiteral("relpath")).toString())))
            ++removed;
    }
    const int prevFailed = prev.value(QStringLiteral("failed")).toArray().size();
    QString extra = QObject::tr("Update check: %1 changed, %2 missing, %3 removed.")
                        .arg(changed).arg(missing).arg(removed);
    extra += QLatin1Char(' ') + QObject::tr("%1 already up to date.").arg(upToDate);
    if (prevFailed > 0)
        extra += QLatin1Char(' ') + QObject::tr("%1 previous download(s) will be retried.").arg(prevFailed);
    return extra;
}

bool nameLooksPlutonium(const QString &name)
{
    const QString n = name.toLower();
    return n == QLatin1String("plutonium") || n == QLatin1String("pluto")
           || n == QLatin1String("pu");
}

bool nameLooksGame(const QString &name)
{
    const QString n = name.toLower();
    return n == QLatin1String("steam") || n == QLatin1String("steamapps")
           || n == QLatin1String("bo2") || n == QLatin1String("bo1")
           || n == QLatin1String("waw") || n == QLatin1String("mw3")
           || n.contains(QLatin1String("black ops"))
           || n.contains(QLatin1String("world at war"))
           || n.contains(QLatin1String("modern warfare"));
}

bool isLayoutRootName(const QString &name)
{
    const QString n = name.toLower();
    return nameLooksPlutonium(n) || nameLooksGame(n)
           || n == QLatin1String("storage") || n == QLatin1String("zone");
}

QString unwrapExtractRoot(const QString &dir)
{
    QDir d(dir);
    const auto entries = d.entryInfoList(QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot);
    if (entries.size() == 1 && entries.first().isDir()) {
        const QString name = entries.first().fileName();
        if (isLayoutRootName(name))
            return QDir::cleanPath(entries.first().absoluteFilePath());
        return unwrapExtractRoot(entries.first().absoluteFilePath());
    }
    return QDir::cleanPath(dir);
}

QString apply(const Manifest &man,
              const ModPreview::Preview &preview,
              const QString &plutoniumRoot,
              const QString &gameRoot,
              const ProgressFn &onProgress,
              QList<FailedDownload> *failedOut)
{
    if (!man.valid)
        return man.error.isEmpty() ? QObject::tr("Invalid GitHub manifest.") : man.error;
    if (plutoniumRoot.isEmpty())
        return QObject::tr("Set the Plutonium folder in Settings first.");

    const QString pu = QDir::cleanPath(QFileInfo(plutoniumRoot).absoluteFilePath());
    const QString gameAbs = gameRoot.isEmpty()
                                ? QString()
                                : QDir::cleanPath(QFileInfo(gameRoot).absoluteFilePath());
    const QString gameCode = preview.gameCode.isEmpty() ? man.gameCode : preview.gameCode;
    QString modId = preview.name;
    modId.replace(QLatin1Char(' '), QLatin1Char('_'));
    modId.replace(QLatin1Char('/'), QLatin1Char('_'));
    if (modId.isEmpty())
        modId = QStringLiteral("github_mod");

    const QString ckpt = pu + "/.lanlauncher_checkpoints/" + gameCode + "/" + modId;
    QDir().mkpath(ckpt + "/backup/plutonium");
    QDir().mkpath(ckpt + "/backup/game");
    const QString statePath = findDownloadJsonPath(pu, gameAbs, gameCode, man);
    const QJsonObject prevState = loadDownloadJson(statePath);
    QSet<QString> prevKeys;
    QHash<QString, QString> prevDest;
    for (const QJsonValue &v : prevState.value(QStringLiteral("files")).toArray()) {
        const QJsonObject o = v.toObject();
        const QString key = fileKey(o.value(QStringLiteral("root")).toString(),
                                    o.value(QStringLiteral("relpath")).toString());
        prevKeys.insert(key);
        const QString tag = o.value(QStringLiteral("destRoot")).toString();
        const QString rel = o.value(QStringLiteral("destRel")).toString();
        if (!rel.isEmpty())
            prevDest.insert(key, (tag == QLatin1String("game") ? gameAbs : pu) + QLatin1Char('/') + rel);
    }

    qint64 total = 0;
    for (const RemoteFile &f : man.files)
        total += qMax(qint64(1), f.size);
    qint64 done = 0;
    QStringList addedPu, replacedPu, addedGame, replacedGame;
    QList<FailedDownload> failedItems;

    auto destOf = [&](const QString &root, const QString &rel, QString &tag, QString &destRel) {
        mapDestination(root, QString(), rel, gameCode, tag, destRel);
    };

    auto backupIfNeeded = [&](const QString &tag, const QString &destRel, const QString &dst, QString &error) {
        if (!preview.makeBackup || !QFileInfo::exists(dst))
            return true;
        const QString bak = ckpt + "/backup/" + tag + "/" + destRel;
        return copyFileOverwrite(dst, bak, error);
    };

    std::function<QString(const QString &, const QString &, const QString &)> copyTree;
    copyTree = [&](const QString &srcDir, const QString &dstDir, const QString &tag) -> QString {
        QDir src(srcDir);
        const auto items = src.entryInfoList(QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot);
        for (const QFileInfo &it : items) {
            const QString dest = dstDir + QLatin1Char('/') + it.fileName();
            if (it.isDir()) {
                QDir().mkpath(dest);
                const QString err = copyTree(it.absoluteFilePath(), dest, tag);
                if (!err.isEmpty())
                    return err;
                continue;
            }
            const QString destRel = QDir(tag == QLatin1String("game") ? gameAbs : pu).relativeFilePath(dest);
            QString error;
            const bool existed = QFileInfo::exists(dest);
            if (existed && !backupIfNeeded(tag, destRel, dest, error))
                return error;
            if (!copyFileOverwrite(it.absoluteFilePath(), dest, error))
                return error;
            if (existed) {
                if (tag == QLatin1String("game"))
                    replacedGame << destRel;
                else
                    replacedPu << destRel;
            } else {
                if (tag == QLatin1String("game"))
                    addedGame << destRel;
                else
                    addedPu << destRel;
            }
        }
        return QString();
    };

    if (man.bundle.valid()) {
        const QString prevHash = prevState.value(QStringLiteral("shortHash")).toString();
        const QString prevSha = prevState.value(QStringLiteral("bundleSha256")).toString().toLower();
        const bool sameBundle = !man.shortHash.isEmpty() && prevHash == man.shortHash
                                && (man.bundle.sha256.isEmpty() || prevSha == man.bundle.sha256);
        if (!sameBundle) {
            if (!ArchiveTool::hasSevenZip())
                return QObject::tr("7-Zip is required to extract this compressed pack.");
            const QString tempRoot = QDir::temp().filePath(
                QStringLiteral("LanLauncher_bundle_%1").arg(QDateTime::currentMSecsSinceEpoch()));
            QDir().mkpath(tempRoot);
            const QString packPath = tempRoot + QLatin1Char('/')
                + (man.bundle.assetName.isEmpty() ? QStringLiteral("pack.7z") : QFileInfo(man.bundle.assetName).fileName());
            QString error;
            bool ok = false;
            for (int attempt = 1; attempt <= 10 && !ok; ++attempt) {
                QFile::remove(packPath);
                if (onProgress) {
                    onProgress(attempt > 1 ? 8 : 4,
                               attempt > 1
                                   ? QObject::tr("Retry %1/10: %2").arg(attempt).arg(QFileInfo(packPath).fileName())
                                   : QObject::tr("Downloading pack %1").arg(QFileInfo(packPath).fileName()));
                }
                if (attempt > 1)
                    QThread::msleep(qMin(12000, 1500 * attempt));
                ok = Downloader::downloadToFile(man.bundle.url, packPath, error, nullptr,
                                                timeoutForSize(man.bundle.size));
                if (ok && !man.bundle.sha256.isEmpty() && fileSha256(packPath) != man.bundle.sha256) {
                    ok = false;
                    error = QObject::tr("Checksum mismatch: %1").arg(QFileInfo(packPath).fileName());
                    QFile::remove(packPath);
                }
            }
            if (!ok) {
                QDir(tempRoot).removeRecursively();
                FailedDownload miss;
                miss.name = QFileInfo(packPath).fileName();
                miss.url = man.bundle.url;
                miss.destPath = packPath;
                miss.error = error.isEmpty() ? QObject::tr("Failed to download pack") : error;
                failedItems << miss;
                if (failedOut)
                    *failedOut = failedItems;
                return QStringLiteral("__PARTIAL__");
            }
            const QString extractDir = tempRoot + QStringLiteral("/extract");
            QDir().mkpath(extractDir);
            if (onProgress)
                onProgress(40, QObject::tr("Extracting pack..."));
            QString extractError;
            if (!ArchiveTool::extractToDirectory(packPath, extractDir, &extractError,
                                                 [&](int pct) {
                                                     if (onProgress)
                                                         onProgress(40 + int(pct * 0.25),
                                                                    QObject::tr("Extracting pack... %1%").arg(pct));
                                                 })) {
                QDir(tempRoot).removeRecursively();
                return extractError.isEmpty() ? QObject::tr("Failed to extract the pack.") : extractError;
            }
            const QString root = unwrapExtractRoot(extractDir);
            if (onProgress)
                onProgress(70, QObject::tr("Copying files..."));
            QDir extracted(root);
            bool mapped = false;
            auto copyOrFail = [&](const QString &src, const QString &dst, const QString &tag) -> QString {
                const QString err = copyTree(src, dst, tag);
                if (!err.isEmpty())
                    QDir(tempRoot).removeRecursively();
                return err;
            };
            if (nameLooksPlutonium(QFileInfo(root).fileName())
                || QDir(root + QStringLiteral("/storage")).exists()) {
                const QString err = copyOrFail(root, pu, QStringLiteral("plutonium"));
                if (!err.isEmpty())
                    return err;
                mapped = true;
            }
            for (const QFileInfo &it : extracted.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot)) {
                if (mapped && (nameLooksPlutonium(it.fileName())
                               || it.fileName().compare(QLatin1String("storage"), Qt::CaseInsensitive) == 0))
                    continue;
                if (nameLooksPlutonium(it.fileName())) {
                    const QString err = copyOrFail(it.absoluteFilePath(), pu, QStringLiteral("plutonium"));
                    if (!err.isEmpty())
                        return err;
                    mapped = true;
                } else if (it.fileName().compare(QLatin1String("storage"), Qt::CaseInsensitive) == 0) {
                    const QString err = copyOrFail(it.absoluteFilePath(),
                                                   pu + QStringLiteral("/storage"),
                                                   QStringLiteral("plutonium"));
                    if (!err.isEmpty())
                        return err;
                    mapped = true;
                } else if (nameLooksGame(it.fileName())) {
                    if (gameAbs.isEmpty()) {
                        QDir(tempRoot).removeRecursively();
                        return QObject::tr("This mod needs the game folder for %1.").arg(preview.gameId);
                    }
                    const QString err = copyOrFail(it.absoluteFilePath(), gameAbs, QStringLiteral("game"));
                    if (!err.isEmpty())
                        return err;
                    mapped = true;
                }
            }
            if (!mapped) {
                if (QDir(root + QStringLiteral("/storage")).exists()
                    || QDir(root + QStringLiteral("/bin")).exists()) {
                    const QString err = copyTree(root, pu, QStringLiteral("plutonium"));
                    if (!err.isEmpty()) {
                        QDir(tempRoot).removeRecursively();
                        return err;
                    }
                } else if (QDir(root + QStringLiteral("/zone")).exists()) {
                    if (gameAbs.isEmpty()) {
                        QDir(tempRoot).removeRecursively();
                        return QObject::tr("This mod needs the game folder for %1.").arg(preview.gameId);
                    }
                    const QString err = copyTree(root, gameAbs, QStringLiteral("game"));
                    if (!err.isEmpty()) {
                        QDir(tempRoot).removeRecursively();
                        return err;
                    }
                } else {
                    const QString err = copyTree(root, pu, QStringLiteral("plutonium"));
                    if (!err.isEmpty()) {
                        QDir(tempRoot).removeRecursively();
                        return err;
                    }
                }
            }
            QDir(tempRoot).removeRecursively();
        } else if (onProgress) {
            onProgress(70, QObject::tr("Bundle already up to date."));
        }

        for (const RetireFile &r : man.retire) {
            QString tag, destRel;
            destOf(r.root, r.relpath, tag, destRel);
            const QString dst = (tag == QLatin1String("game") ? gameAbs : pu) + QLatin1Char('/') + destRel;
            if (!QFileInfo::exists(dst))
                continue;
            QString error;
            if (!backupIfNeeded(tag, destRel, dst, error))
                return error;
            if (tag == QLatin1String("game"))
                replacedGame << destRel;
            else
                replacedPu << destRel;
            QFile::remove(dst);
        }

        QJsonObject root;
        root.insert(QStringLiteral("id"), modId);
        root.insert(QStringLiteral("displayName"), preview.name);
        root.insert(QStringLiteral("game"), gameCode);
        root.insert(QStringLiteral("kind"), QStringLiteral("github-bundle"));
        root.insert(QStringLiteral("created"), QDateTime::currentDateTime().toString(Qt::ISODate));
        root.insert(QStringLiteral("archive"), man.sourceUrl);
        root.insert(QStringLiteral("backupDir"), ckpt + "/backup");
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

        QJsonObject state;
        state.insert(QStringLiteral("schemaVersion"), 1);
        state.insert(QStringLiteral("sourceUrl"), man.sourceUrl);
        state.insert(QStringLiteral("releaseTag"), man.releaseTag);
        state.insert(QStringLiteral("releaseVersion"), man.releaseVersion);
        state.insert(QStringLiteral("shortHash"), man.shortHash);
        state.insert(QStringLiteral("bundleSha256"), man.bundle.sha256);
        state.insert(QStringLiteral("bundleUrl"), man.bundle.url);
        state.insert(QStringLiteral("bundleName"), man.bundle.assetName);
        state.insert(QStringLiteral("updated"), QDateTime::currentDateTime().toString(Qt::ISODate));
        state.insert(QStringLiteral("files"), QJsonArray());
        state.insert(QStringLiteral("failed"), QJsonArray());
        QDir().mkpath(QFileInfo(statePath).absolutePath());
        QFile sf(statePath);
        if (sf.open(QIODevice::WriteOnly | QIODevice::Truncate))
            sf.write(QJsonDocument(state).toJson(QJsonDocument::Indented));
        QDir().mkpath(ckpt);
        QFile mf(ckpt + "/manifest.json");
        if (mf.open(QIODevice::WriteOnly | QIODevice::Truncate))
            mf.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
        if (onProgress)
            onProgress(100, QObject::tr("Done"));
        if (failedOut)
            *failedOut = failedItems;
        return QString();
    }

    int index = 0;
    for (const RemoteFile &f : man.files) {
        ++index;
        QString tag, destRel;
        destOf(f.root, f.relpath, tag, destRel);
        if (tag == QLatin1String("game") && gameAbs.isEmpty())
            return QObject::tr("This mod needs the game folder for %1.").arg(preview.gameId);
        const QString dst = (tag == QLatin1String("game") ? gameAbs : pu) + QLatin1Char('/') + destRel;
        if (onProgress) {
            const int pct = total > 0 ? int(done * 100 / total) : 0;
            onProgress(pct, QObject::tr("Downloading %1 / %2  (%3)")
                                .arg(index)
                                .arg(man.files.size())
                                .arg(QFileInfo(f.relpath).fileName()));
        }

        const bool existed = QFileInfo::exists(dst);
        if (existed && fileMatches(dst, f.md5, f.sha256)) {
            if (onProgress)
                onProgress(total > 0 ? int(done * 100 / total) : 0,
                           QObject::tr("Up to date: %1").arg(QFileInfo(f.relpath).fileName()));
            done += qMax(qint64(1), f.size);
            continue;
        }

        const QString part = dst + QLatin1String(".part");
        QString error;
        bool ok = false;
        for (int attempt = 1; attempt <= 10 && !ok; ++attempt) {
            QFile::remove(part);
            if (attempt > 1) {
                if (onProgress) {
                    onProgress(total > 0 ? int(done * 100 / total) : 0,
                               QObject::tr("Retry %1/10: %2").arg(attempt).arg(QFileInfo(f.relpath).fileName()));
                }
                QThread::msleep(qMin(12000, 1500 * attempt));
            }
            ok = Downloader::downloadToFile(f.url, part, error, nullptr, timeoutForSize(f.size));
            if (ok && (!f.md5.isEmpty() || !f.sha256.isEmpty()) && !fileMatches(part, f.md5, f.sha256)) {
                ok = false;
                error = QObject::tr("Checksum mismatch: %1").arg(QFileInfo(f.relpath).fileName());
                QFile::remove(part);
            }
        }
        if (!ok) {
            QFile::remove(part);
            FailedDownload miss;
            miss.name = QFileInfo(f.relpath).fileName();
            miss.url = f.url;
            miss.destPath = dst;
            miss.error = error.isEmpty() ? QObject::tr("Failed to download %1").arg(f.relpath) : error;
            failedItems << miss;
            done += qMax(qint64(1), f.size);
            continue;
        }

        if (existed) {
            if (!backupIfNeeded(tag, destRel, dst, error)) {
                QFile::remove(part);
                return error;
            }
            if (tag == QLatin1String("game"))
                replacedGame << destRel;
            else
                replacedPu << destRel;
        } else {
            if (tag == QLatin1String("game"))
                addedGame << destRel;
            else
                addedPu << destRel;
        }

        QDir().mkpath(QFileInfo(dst).absolutePath());
        QFile::remove(dst);
        if (!QFile::rename(part, dst)) {
            if (!copyFileOverwrite(part, dst, error)) {
                QFile::remove(part);
                FailedDownload miss;
                miss.name = QFileInfo(f.relpath).fileName();
                miss.url = f.url;
                miss.destPath = dst;
                miss.error = error;
                failedItems << miss;
                done += qMax(qint64(1), f.size);
                continue;
            }
            QFile::remove(part);
        }
        done += qMax(qint64(1), f.size);
    }

    for (const RetireFile &r : man.retire) {
        QString tag, destRel;
        destOf(r.root, r.relpath, tag, destRel);
        const QString dst = (tag == QLatin1String("game") ? gameAbs : pu) + QLatin1Char('/') + destRel;
        if (!QFileInfo::exists(dst))
            continue;
        QString error;
        if (!backupIfNeeded(tag, destRel, dst, error))
            return error;
        if (tag == QLatin1String("game"))
            replacedGame << destRel;
        else
            replacedPu << destRel;
        QFile::remove(dst);
    }

    QSet<QString> newKeys;
    for (const RemoteFile &f : man.files)
        newKeys.insert(fileKey(f.root, f.relpath));
    for (const QString &key : prevKeys) {
        if (newKeys.contains(key))
            continue;
        const QString dst = prevDest.value(key);
        if (dst.isEmpty() || !QFileInfo::exists(dst))
            continue;
        QString error;
        const bool gameFile = dst.startsWith(gameAbs) && !gameAbs.isEmpty();
        const QString tag = gameFile ? QStringLiteral("game") : QStringLiteral("plutonium");
        const QString destRel = QDir(gameFile ? gameAbs : pu).relativeFilePath(dst);
        backupIfNeeded(tag, destRel, dst, error);
        QFile::remove(dst);
    }

    QJsonObject root;
    root.insert(QStringLiteral("id"), modId);
    root.insert(QStringLiteral("displayName"), preview.name);
    root.insert(QStringLiteral("game"), gameCode);
    root.insert(QStringLiteral("kind"), QStringLiteral("github"));
    root.insert(QStringLiteral("created"), QDateTime::currentDateTime().toString(Qt::ISODate));
    root.insert(QStringLiteral("archive"), man.sourceUrl);
    root.insert(QStringLiteral("backupDir"), ckpt + "/backup");
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
    QJsonObject state;
    state.insert(QStringLiteral("schemaVersion"), 1);
    state.insert(QStringLiteral("sourceUrl"), man.sourceUrl);
    state.insert(QStringLiteral("releaseTag"), man.releaseTag);
    state.insert(QStringLiteral("releaseVersion"), man.releaseVersion);
    state.insert(QStringLiteral("shortHash"), man.shortHash);
    state.insert(QStringLiteral("updated"), QDateTime::currentDateTime().toString(Qt::ISODate));
    QJsonArray tracked;
    for (const RemoteFile &f : man.files) {
        QString tag, destRel;
        destOf(f.root, f.relpath, tag, destRel);
        QJsonObject o;
        o.insert(QStringLiteral("relpath"), f.relpath);
        o.insert(QStringLiteral("root"), f.root);
        o.insert(QStringLiteral("destRoot"), tag);
        o.insert(QStringLiteral("destRel"), destRel);
        o.insert(QStringLiteral("md5"), f.md5);
        o.insert(QStringLiteral("sha256"), f.sha256);
        o.insert(QStringLiteral("url"), f.url);
        o.insert(QStringLiteral("size"), f.size);
        tracked.append(o);
    }
    state.insert(QStringLiteral("files"), tracked);
    QJsonArray failedArr;
    for (const FailedDownload &miss : failedItems) {
        QJsonObject o;
        o.insert(QStringLiteral("name"), miss.name);
        o.insert(QStringLiteral("url"), miss.url);
        o.insert(QStringLiteral("destPath"), miss.destPath);
        o.insert(QStringLiteral("error"), miss.error);
        failedArr.append(o);
    }
    state.insert(QStringLiteral("failed"), failedArr);
    QDir().mkpath(QFileInfo(statePath).absolutePath());
    QFile sf(statePath);
    if (sf.open(QIODevice::WriteOnly | QIODevice::Truncate))
        sf.write(QJsonDocument(state).toJson(QJsonDocument::Indented));

    QDir().mkpath(ckpt);
    QFile mf(ckpt + "/manifest.json");
    if (!mf.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return QObject::tr("Nao foi possivel gravar o checkpoint em %1").arg(ckpt);
    mf.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    if (onProgress)
        onProgress(100, QObject::tr("Done"));
    if (failedOut)
        *failedOut = failedItems;
    if (!failedItems.isEmpty())
        return QStringLiteral("__PARTIAL__");
    return QString();
}

} // namespace GithubModInstaller
