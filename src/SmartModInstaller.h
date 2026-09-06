#pragma once
#include <QString>
#include <QStringList>

// Mixed packs (e.g. Zombies Declassified):
//   storage/t6/...  -> Plutonium folder
//   steam/...       -> game folder
//
// Each install writes a checkpoint under
//   <plutonium>/.lanlauncher_checkpoints/<t6>/<modId>/
// with new files, replaced files, and copies of the originals.
// Removing the mod rolls back and deletes that checkpoint.
namespace SmartModInstaller
{
    struct Plan {
        bool isSmart = false;
        bool hasStorage = false;
        bool hasSteam = false;
        QString storageGameId;
        QString storageRoot;
        QString steamRoot;
        QStringList storageRelativePaths;
        QStringList steamRelativePaths;
        QString summary;
        QString suggestedModId;
    };

    Plan analyzeExtractedRoot(const QString &extractedRoot);

    QString applyPlan(const Plan &plan,
                      const QString &extractedRoot,
                      const QString &plutoniumRoot,
                      const QString &gameRoot,
                      const QString &modId,
                      const QString &archivePath,
                      QString *warningOut = nullptr);

    // Standard mod (fs_game folder at storage/<game>/mods/<name>).
    QString recordStandardInstall(const QString &plutoniumRoot,
                                  const QString &gameStorageId,
                                  const QString &modFolderAbs,
                                  const QString &modId,
                                  const QString &archivePath);

    bool hasCheckpoint(const QString &plutoniumRoot, const QString &gameStorageId, const QString &modId);
    QStringList checkpointIds(const QString &plutoniumRoot, const QString &gameStorageId);
    QString checkpointDisplayName(const QString &plutoniumRoot, const QString &gameStorageId, const QString &modId);
    QStringList checkpointOwnedModFolders(const QString &plutoniumRoot, const QString &gameStorageId, const QString &modId);

    // Restore replaced files, delete added ones, remove the checkpoint folder.
    QString rollback(const QString &plutoniumRoot,
                     const QString &gameRoot,
                     const QString &gameStorageId,
                     const QString &modId);
}
