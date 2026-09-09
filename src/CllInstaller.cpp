#include "CllInstaller.h"
#include "ArchiveTool.h"
#include "GameCatalog.h"
#include "SmartModInstaller.h"

#include <QDateTime>
#include <QDialog>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QPushButton>
#include <QTextEdit>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QCheckBox>
#include <QFrame>
#include <QHash>
#include <QScrollArea>
#include <QStorageInfo>

namespace {

QString sanitizeId(QString s)
{
    s = s.trimmed();
    s.replace(' ', '_');
    s.replace('/', '_');
    s.replace('\\', '_');
    return s.isEmpty() ? QStringLiteral("cll_mod") : s;
}

QString findInstallerFile(const QString &root)
{
    const QDir d(root);
    if (QFileInfo::exists(root + "/cll_installer.json"))
        return root + "/cll_installer.json";
    const auto entries = d.entryInfoList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QFileInfo &fi : entries) {
        if (fi.isFile() && fi.fileName().compare(QLatin1String("cll_installer.json"), Qt::CaseInsensitive) == 0)
            return fi.absoluteFilePath();
    }
    for (const QFileInfo &fi : entries) {
        if (!fi.isDir())
            continue;
        const QString hit = findInstallerFile(fi.absoluteFilePath());
        if (!hit.isEmpty())
            return hit;
    }
    return QString();
}

void collectFiles(const QString &root, const QString &rel, QStringList &out)
{
    const QString abs = rel.isEmpty() ? root : root + "/" + rel;
    QDir dir(abs);
    if (!dir.exists())
        return;
    for (const QFileInfo &fi : dir.entryInfoList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot)) {
        const QString child = rel.isEmpty() ? fi.fileName() : rel + "/" + fi.fileName();
        if (fi.isDir())
            collectFiles(root, child, out);
        else
            out << child;
    }
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

} // namespace

namespace CllInstaller {

bool isSupportedArchive(const QString &path)
{
    const QString ext = QFileInfo(path).suffix().toLower();
    return ext == QLatin1String("zip") || ext == QLatin1String("rar")
        || ext == QLatin1String("7z") || ext == QLatin1String("exe")
        || ext == QLatin1String("cll");
}

bool isCllPack(const QString &path)
{
    return QFileInfo(path).suffix().compare(QLatin1String("cll"), Qt::CaseInsensitive) == 0;
}

QString normalizeGameCode(const QString &raw)
{
    QString s = raw.trimmed().toLower();
    s.replace(QLatin1String("game - "), QString());
    s.replace(QLatin1String("game-"), QString());
    s.replace(QLatin1String("game:"), QString());
    s = s.trimmed();
    if (s == QLatin1String("waw") || s == QLatin1String("world at war"))
        return QStringLiteral("t4");
    if (s == QLatin1String("bo1") || s == QLatin1String("black ops") || s == QLatin1String("black ops 1"))
        return QStringLiteral("t5");
    if (s == QLatin1String("bo2") || s == QLatin1String("black ops 2") || s == QLatin1String("black ops ii"))
        return QStringLiteral("t6");
    if (s == QLatin1String("mw3") || s == QLatin1String("modern warfare 3"))
        return QStringLiteral("iw5");
    if (s == QLatin1String("t4") || s == QLatin1String("t5") || s == QLatin1String("t6") || s == QLatin1String("iw5"))
        return s;
    const auto g = GameCatalog::byCode(s);
    if (!g.code.isEmpty())
        return g.code;
    const auto byId = GameCatalog::byId(raw.trimmed());
    if (!byId.code.isEmpty())
        return byId.code;
    return s;
}

Manifest parseJsonBytes(const QByteArray &json)
{
    Manifest m;
    QJsonParseError pe;
    const QJsonDocument doc = QJsonDocument::fromJson(json, &pe);
    if (!doc.isObject()) {
        m.error = QObject::tr("cll_installer.json is not valid JSON.");
        if (pe.error != QJsonParseError::NoError)
            m.error += QStringLiteral(" ") + pe.errorString();
        return m;
    }
    const QJsonObject o = doc.object();
    m.name = o.value(QStringLiteral("name")).toString().trimmed();
    m.version = o.value(QStringLiteral("version")).toString().trimmed();
    m.author = o.value(QStringLiteral("author")).toString().trimmed();
    if (m.author.isEmpty())
        m.author = o.value(QStringLiteral("Author")).toString().trimmed();
    m.description = o.value(QStringLiteral("description")).toString().trimmed();

    QString gameRaw = o.value(QStringLiteral("game")).toString();
    if (gameRaw.isEmpty()) {
        for (auto it = o.begin(); it != o.end(); ++it) {
            if (it.key().startsWith(QLatin1String("game"), Qt::CaseInsensitive)
                && it.key().contains(QLatin1Char('-'))) {
                gameRaw = it.key();
                break;
            }
        }
    }
    m.gameCode = normalizeGameCode(gameRaw);
    {
        const QStringList known = {"t4", "t5", "t6", "iw5"};
        if (!known.contains(m.gameCode)) {
            const auto byId = GameCatalog::byId(gameRaw.trimmed());
            if (known.contains(byId.code) && gameRaw.trimmed().compare(byId.id, Qt::CaseInsensitive) == 0)
                m.gameCode = byId.code;
            else
                m.gameCode.clear();
        }
    }
    m.gameId = m.gameCode.isEmpty() ? QString() : GameCatalog::byCode(m.gameCode).id;

    auto addRule = [&](QString from, QString target, QString dest) {
        from.replace('\\', '/');
        dest.replace('\\', '/');
        target = target.trimmed().toLower();
        if (target == QLatin1String("pu") || target == QLatin1String("plutonium"))
            target = QStringLiteral("pu_folder");
        if (target == QLatin1String("game") || target == QLatin1String("steam"))
            target = QStringLiteral("game_folder");
        if (from.isEmpty() || (target != QLatin1String("pu_folder") && target != QLatin1String("game_folder")))
            return;
        if (dest.isEmpty())
            dest = from;
        m.folders.push_back({from, dest, target});
    };

    const QJsonValue folders = o.value(QStringLiteral("folders"));
    if (folders.isArray()) {
        for (const QJsonValue &v : folders.toArray()) {
            const QJsonObject f = v.toObject();
            addRule(f.value(QStringLiteral("from")).toString().trimmed(),
                    f.value(QStringLiteral("to")).toString(),
                    f.value(QStringLiteral("dest")).toString().trimmed());
        }
    } else if (folders.isObject()) {
        const QJsonObject fo = folders.toObject();
        for (auto it = fo.begin(); it != fo.end(); ++it)
            addRule(it.key(), it.value().toString(), QString());
    }

    for (const char *key : {"pu_folder", "game_folder"}) {
        const QJsonValue v = o.value(QLatin1String(key));
        if (v.isArray()) {
            for (const QJsonValue &item : v.toArray())
                addRule(item.toString().trimmed(), QLatin1String(key), QString());
        } else if (v.isString()) {
            addRule(v.toString().trimmed(), QLatin1String(key), QString());
        }
    }

    if (m.name.isEmpty())
        m.error = QObject::tr("cll_installer.json is missing \"name\".");
    else if (m.gameId.isEmpty())
        m.error = QObject::tr("cll_installer.json has an unknown game (use t4, t5, t6 or iw5).");
    else if (m.folders.isEmpty())
        m.error = QObject::tr("cll_installer.json has no folders to install.");
    else
        m.valid = true;
    return m;
}

Manifest parseJsonFile(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        Manifest m;
        m.error = QObject::tr("Could not read %1").arg(path);
        return m;
    }
    return parseJsonBytes(f.readAll());
}

bool archiveMentionsInstaller(const QString &archivePath)
{
    const QStringList entries = ArchiveTool::listEntries(archivePath);
    for (const QString &e : entries) {
        if (QFileInfo(e).fileName().compare(QLatin1String("cll_installer.json"), Qt::CaseInsensitive) == 0)
            return true;
    }
    return false;
}

Manifest peekArchive(const QString &archivePath)
{
    Manifest m;
    m.sourceArchive = archivePath;
    if (!QFileInfo::exists(archivePath)) {
        m.error = QObject::tr("File not found.");
        return m;
    }
    if (!archiveMentionsInstaller(archivePath)) {
        m.error = QObject::tr("This pack has no cll_installer.json.");
        return m;
    }
    const QString temp = QDir::temp().filePath(QStringLiteral("LanLauncher_cll_peek"));
    QDir(temp).removeRecursively();
    QDir().mkpath(temp);
    QString err;
    if (!ArchiveTool::extractPaths(archivePath, temp, {QStringLiteral("cll_installer.json")}, &err)) {
        if (!ArchiveTool::extractToDirectory(archivePath, temp, &err)) {
            QDir(temp).removeRecursively();
            m.error = err.isEmpty() ? QObject::tr("Could not read the pack.") : err;
            return m;
        }
    }
    const QString jsonPath = findInstallerFile(temp);
    if (jsonPath.isEmpty()) {
        QDir(temp).removeRecursively();
        m.error = QObject::tr("cll_installer.json was listed but not extracted.");
        return m;
    }
    m = parseJsonFile(jsonPath);
    m.sourceArchive = archivePath;
    QDir(temp).removeRecursively();
    return m;
}

bool confirmAndShow(QWidget *parent, const Manifest &man,
                    const QString &plutoniumRoot, const QString &gameRoot,
                    bool *makeBackup)
{
    QDialog dlg(parent);
    dlg.setWindowTitle(man.name.isEmpty() ? QObject::tr("CLL pack") : man.name);
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
    const QPixmap pix = GameCatalog::icon(man.gameCode, QSize(64, 64));
    if (!pix.isNull()) {
        icon->setPixmap(pix);
        icon->setScaledContents(true);
    } else {
        icon->setText(man.gameCode.toUpper());
        icon->setAlignment(Qt::AlignCenter);
        icon->setStyleSheet("background:#242636; border-radius:12px; color:#9184d9; font-weight:700;");
    }
    hero->addWidget(icon, 0, Qt::AlignTop);

    auto *head = new QVBoxLayout();
    head->setSpacing(4);
    auto *kicker = new QLabel(QObject::tr("CLL PACK"), &dlg);
    kicker->setObjectName("CllKicker");
    auto *title = new QLabel(man.name.isEmpty() ? QObject::tr("CLL pack") : man.name, &dlg);
    title->setObjectName("CllTitle");
    title->setWordWrap(true);
    head->addWidget(kicker);
    head->addWidget(title);

    auto *chips = new QHBoxLayout();
    chips->setSpacing(6);
    auto addChip = [&](const QString &text, bool accent) {
        if (text.isEmpty()) return;
        auto *c = new QLabel(text, &dlg);
        c->setObjectName(accent ? "CllChipAccent" : "CllChip");
        c->setAlignment(Qt::AlignCenter);
        chips->addWidget(c);
    };
    addChip(QObject::tr("CLL"), true);
    if (!man.version.isEmpty())
        addChip(QObject::tr("v%1").arg(man.version), false);
    if (!man.author.isEmpty())
        addChip(man.author, false);
    if (!man.gameId.isEmpty())
        addChip(man.gameId, false);
    chips->addStretch();
    head->addLayout(chips);
    hero->addLayout(head, 1);
    root->addLayout(hero);

    auto *desc = new QTextEdit(&dlg);
    desc->setReadOnly(true);
    desc->setFixedHeight(88);
    desc->setText(man.description.isEmpty() ? QObject::tr("No description.") : man.description);
    root->addWidget(desc);

    auto *mapHead = new QHBoxLayout();
    auto *mapTitle = new QLabel(QObject::tr("Install map"), &dlg);
    mapTitle->setObjectName("CllMeta");
    mapHead->addWidget(mapTitle);
    mapHead->addStretch();
    root->addLayout(mapHead);
    if (!man.folders.isEmpty()) {
        auto *moreBtn = new QPushButton(QObject::tr("See more"), &dlg);
        moreBtn->setObjectName("CllMore");
        moreBtn->setCursor(Qt::PointingHandCursor);
        root->addWidget(moreBtn);
        QObject::connect(moreBtn, &QPushButton::clicked, &dlg, [&man, &dlg]() {
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
            auto *mapHost = new QWidget;
            auto *mapLay = new QVBoxLayout(mapHost);
            mapLay->setContentsMargins(0, 0, 2, 0);
            mapLay->setSpacing(6);
            auto *sec = new QLabel(QObject::tr("Folders to copy"), mapHost);
            sec->setObjectName("CllDestTo");
            mapLay->addWidget(sec);
            auto *hint = new QLabel(QObject::tr("Each line is a folder in the pack and where it will be installed."), mapHost);
            hint->setObjectName("CllMeta");
            hint->setWordWrap(true);
            mapLay->addWidget(hint);
            for (const FolderRule &r : man.folders) {
                auto *card = new QFrame(mapHost);
                card->setObjectName("CllCard");
                auto *cl = new QHBoxLayout(card);
                cl->setContentsMargins(12, 8, 12, 8);
                const QString dest = r.dest.isEmpty() ? r.from : r.dest;
                const bool game = r.target == QLatin1String("game_folder");
                auto *from = new QLabel(r.from, card);
                from->setObjectName("CllDestFrom");
                from->setWordWrap(true);
                auto *arrow = new QLabel(QStringLiteral("→"), card);
                arrow->setObjectName("CllMeta");
                auto *to = new QLabel(
                    (game ? QObject::tr("Game") : QObject::tr("Plutonium")) + " / " + dest, card);
                to->setObjectName("CllDestTo");
                to->setWordWrap(true);
                cl->addWidget(from, 1);
                cl->addWidget(arrow, 0);
                cl->addWidget(to, 1);
                mapLay->addWidget(card);
            }
            mapLay->addStretch();
            scroll->setWidget(mapHost);
            vl->addWidget(scroll, 1);
            auto *closeBtn = new QPushButton(QObject::tr("Close"), &details);
            closeBtn->setObjectName("CllCancel");
            closeBtn->setCursor(Qt::PointingHandCursor);
            vl->addWidget(closeBtn, 0, Qt::AlignRight);
            QObject::connect(closeBtn, &QPushButton::clicked, &details, &QDialog::accept);
            details.exec();
        });
    }

    auto fmtBytes = [](qint64 b) -> QString {
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

    struct DiskAcc { QString path; QString label; qint64 payload = 0; };
    QHash<QString, DiskAcc> disks;
    auto addPayload = [&](const QString &path, const QString &label, qint64 n) {
        if (path.isEmpty())
            return;
        DiskAcc &d = disks[path];
        d.path = path;
        d.label = label;
        d.payload += n;
    };
    const auto listed = man.sourceArchive.isEmpty()
                            ? QList<ArchiveTool::ListedFile>()
                            : ArchiveTool::listDetailed(man.sourceArchive);
    for (const FolderRule &r : man.folders) {
        const bool game = r.target == QLatin1String("game_folder");
        qint64 bytes = 0;
        const QString prefix = r.from.endsWith(QLatin1Char('/')) ? r.from : (r.from + QLatin1Char('/'));
        for (const auto &e : listed) {
            if (e.isDir)
                continue;
            if (e.path == r.from || e.path.startsWith(prefix) || e.path.contains(QLatin1Char('/') + prefix)
                || e.path.endsWith(QLatin1Char('/') + r.from) || e.path.contains(QLatin1Char('/') + r.from + QLatin1Char('/')))
                bytes += e.size;
        }
        addPayload(game ? gameRoot : plutoniumRoot,
                   game ? QObject::tr("Game") : QObject::tr("Plutonium"),
                   bytes);
    }
    if (disks.isEmpty()) {
        addPayload(plutoniumRoot, QObject::tr("Plutonium"), 0);
        if (!gameRoot.isEmpty())
            addPayload(gameRoot, QObject::tr("Game"), 0);
    }

    auto *diskRow = new QHBoxLayout();
    for (const DiskAcc &d : disks) {
        const QStorageInfo st(d.path);
        const qint64 available = st.bytesAvailable();
        const qint64 total = st.bytesTotal();
        const qint64 used = (total > 0 && available >= 0) ? (total - available) : 0;
        auto *card = new QFrame(&dlg);
        card->setObjectName("CllCard");
        auto *vl = new QVBoxLayout(card);
        const QString drive = st.rootPath().isEmpty() ? d.path.left(2) : st.rootPath();
        const bool diskLow = available >= 0 && available < 25LL * 1024 * 1024 * 1024;
        auto *diskName = new QLabel(d.label + QStringLiteral("  ") + drive
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
        const qint64 need = d.payload;
        int usedPct = 0, modPct = 0;
        if (total > 0) {
            usedPct = int(qBound(qint64(0), used * 100 / total, qint64(100)));
            modPct = int(qBound(qint64(1), need * 100 / qMax(total, qint64(1)), qint64(100)));
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
        diskTxt->setText(QObject::tr("Livre %1 de %2").arg(fmtBytes(available), fmtBytes(total))
                         + QStringLiteral("<br>")
                         + QStringLiteral("<span style='color:#e89a3a'>%1 %2</span>  ·  <span style='color:#e89a3a'>%3 ~%4</span>")
                               .arg(QObject::tr("Mod"), fmtBytes(d.payload),
                                    QObject::tr("backup"), fmtBytes(d.payload / 4)));
        vl->addWidget(diskTxt);
        diskRow->addWidget(card);
    }
    root->addLayout(diskRow);

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
    if (dlg.exec() != QDialog::Accepted)
        return false;
    if (makeBackup)
        *makeBackup = backup->isChecked();
    return true;
}

QString apply(const Manifest &man,
              const QString &extractedRoot,
              const QString &plutoniumRoot,
              const QString &gameRoot,
              const QString &archivePath,
              const ProgressFn &onProgress,
              bool makeBackup)
{
    if (!man.valid)
        return man.error.isEmpty() ? QObject::tr("Invalid CLL manifest.") : man.error;
    if (plutoniumRoot.isEmpty())
        return QObject::tr("Set the Plutonium folder in Settings first.");

    const QString pu = QDir::cleanPath(QFileInfo(plutoniumRoot).absoluteFilePath());
    const QString gameAbs = gameRoot.isEmpty()
                                ? QString()
                                : QDir::cleanPath(QFileInfo(gameRoot).absoluteFilePath());

    const QString modId = sanitizeId(man.name);
    const QString ckpt = pu + "/.lanlauncher_checkpoints/" + man.gameCode + "/" + modId;
    QDir(ckpt).removeRecursively();
    QDir().mkpath(ckpt + "/backup/pu");
    QDir().mkpath(ckpt + "/backup/game");

    QString error;
    QStringList addedPu, replacedPu, addedGame, replacedGame;

    int totalFiles = 0;
    for (const FolderRule &pre : man.folders) {
        QStringList preFiles;
        collectFiles(QDir::cleanPath(extractedRoot + "/" + pre.from), QString(), preFiles);
        totalFiles += preFiles.size();
    }
    if (onProgress)
        onProgress(0, QObject::tr("Copying files..."));
    int doneFiles = 0;

    for (const FolderRule &rule : man.folders) {
        const QString srcDir = QDir::cleanPath(extractedRoot + "/" + rule.from);
        if (!QDir(srcDir).exists())
            return QObject::tr("Pack is missing folder \"%1\".").arg(rule.from);
        const bool toGame = rule.target == QLatin1String("game_folder");
        if (toGame && gameAbs.isEmpty())
            return QObject::tr("This mod needs the game folder for %1.").arg(man.gameId);

        const QString destRel = rule.dest.isEmpty() ? rule.from : rule.dest;
        const QString destRoot = toGame ? (gameAbs + "/" + destRel) : (pu + "/" + destRel);

        QStringList files;
        collectFiles(srcDir, QString(), files);
        for (const QString &rel : files) {
            const QString src = srcDir + "/" + rel;
            const QString dst = destRoot + "/" + rel;
            const bool existed = QFileInfo::exists(dst);
            if (existed) {
                if (makeBackup) {
                    const QString bak = ckpt + "/backup/" + (toGame ? "game/" : "pu/") + destRel + "/" + rel;
                    if (!copyFileOverwrite(dst, bak, error))
                        return error;
                }
                if (toGame)
                    replacedGame << destRel + "/" + rel;
                else
                    replacedPu << destRel + "/" + rel;
            } else {
                if (toGame)
                    addedGame << destRel + "/" + rel;
                else
                    addedPu << destRel + "/" + rel;
            }
            if (!copyFileOverwrite(src, dst, error))
                return error;
            ++doneFiles;
            if (onProgress && totalFiles > 0) {
                const int pct = int((doneFiles * 100.0) / totalFiles);
                onProgress(pct, QObject::tr("Copying %1 / %2").arg(doneFiles).arg(totalFiles));
            }
        }
    }

    QJsonObject root;
    root.insert(QStringLiteral("id"), modId);
    root.insert(QStringLiteral("displayName"), man.name);
    root.insert(QStringLiteral("author"), man.author);
    root.insert(QStringLiteral("game"), man.gameCode);
    root.insert(QStringLiteral("kind"), QStringLiteral("cll"));
    root.insert(QStringLiteral("created"), QDateTime::currentDateTime().toString(Qt::ISODate));
    root.insert(QStringLiteral("archive"), archivePath);
    root.insert(QStringLiteral("backupDir"), ckpt + "/backup");
    auto arr = [](const QStringList &rels, const QString &tag) {
        QJsonArray a;
        for (const QString &rel : rels) {
            QJsonObject o;
            o.insert(QStringLiteral("root"), tag);
            o.insert(QStringLiteral("rel"), rel);
            a.append(o);
        }
        return a;
    };
    QJsonArray added = arr(addedPu, QStringLiteral("pu"));
    for (const auto &v : arr(addedGame, QStringLiteral("game")))
        added.append(v);
    QJsonArray replaced = arr(replacedPu, QStringLiteral("pu"));
    for (const auto &v : arr(replacedGame, QStringLiteral("game")))
        replaced.append(v);
    QStringList modFolders;
    auto collectMods = [&](const QStringList &rels) {
        for (const QString &rel : rels) {
            QString r = rel;
            r.replace(QLatin1Char('\\'), QLatin1Char('/'));
            int cut = -1;
            if (r.startsWith(QStringLiteral("mods/")))
                cut = 5;
            else {
                const int i = r.indexOf(QStringLiteral("/mods/"));
                if (i >= 0)
                    cut = i + 6;
            }
            if (cut < 0)
                continue;
            const QString folder = r.mid(cut).section(QLatin1Char('/'), 0, 0);
            if (!folder.isEmpty() && !modFolders.contains(folder, Qt::CaseInsensitive))
                modFolders << folder;
        }
    };
    collectMods(addedPu);
    collectMods(replacedPu);
    QJsonArray foldersArr;
    for (const QString &f : modFolders)
        foldersArr.append(f);
    root.insert(QStringLiteral("modFolders"), foldersArr);
    root.insert(QStringLiteral("added"), added);
    root.insert(QStringLiteral("replaced"), replaced);
    QFile f(ckpt + "/manifest.json");
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return QObject::tr("Could not write the install checkpoint.");
    f.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    return QString();
}

} // namespace CllInstaller
