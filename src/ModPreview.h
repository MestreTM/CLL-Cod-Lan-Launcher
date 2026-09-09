#pragma once

#include <QString>
#include <QStringList>
#include <QList>

class QWidget;

namespace ModPreview
{
    struct Mapping {
        QString fromRel;
        QString destRoot; // plutonium | game
        QString destRel;
        qint64 bytes = 0;
        int files = 0;
    };

    struct Conflict {
        QString destRel;
        QString destRoot;
        QStringList otherMods;
    };

    struct DiskUse {
        QString rootPath;
        QString label;
        qint64 available = -1;
        qint64 total = -1;
        qint64 used = 0;
        qint64 payload = 0;
        qint64 backupEstimate = 0;
    };

    struct Preview {
        QString name;
        QString version;
        QString author;
        QString description;
        QString gameCode;
        QString gameId;
        QString sourceKind; // cll | mod.json | smart
        QString extractedRoot;
        QList<Mapping> mappings;
        QList<Conflict> conflicts;
        QList<DiskUse> disks;
        qint64 totalPayload = 0;
        qint64 totalBackupEstimate = 0;
        bool makeBackup = true;
        bool replacesExisting = false;
        QString existingId;
        QString existingName;
        QString existingVersion;
        QString existingDescription;
        QString error;
        struct TrackedFile {
            QString name;
            QString destPath;
            QString root;
            QString relpath;
            QString md5;
            QString sha256;
        };
        QList<TrackedFile> tracked;
        QString compareStatePath;
        bool needsCompare = false;
        QString shortHash;
        QString bundleSha256;
        bool bundleMode = false;
        bool valid() const { return error.isEmpty() && !mappings.isEmpty(); }
    };

    Preview analyze(const QString &extractedRoot,
                    const QString &gameId,
                    const QString &gameCode,
                    const QString &plutoniumRoot,
                    const QString &gameRoot);

    Preview analyzeArchive(const QString &archivePath,
                           const QString &gameId,
                           const QString &gameCode,
                           const QString &plutoniumRoot,
                           const QString &gameRoot);

    bool confirm(QWidget *parent, Preview &preview);

    QString apply(const Preview &preview,
                  const QString &plutoniumRoot,
                  const QString &gameRoot,
                  const QString &archivePath);
}
