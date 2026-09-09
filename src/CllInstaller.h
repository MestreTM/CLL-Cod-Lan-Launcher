#pragma once
#include <QString>
#include <QStringList>
#include <QList>
#include <functional>

class QWidget;

namespace CllInstaller
{
    struct FolderRule {
        QString from;
        QString dest;
        QString target; // pu_folder | game_folder
    };

    struct Manifest {
        bool valid = false;
        QString name;
        QString version;
        QString author;
        QString description;
        QString gameCode;
        QString gameId;
        QList<FolderRule> folders;
        QString error;
        QString sourceArchive;
    };

    bool isSupportedArchive(const QString &path);
    bool isCllPack(const QString &path);

    QString normalizeGameCode(const QString &raw);
    Manifest parseJsonBytes(const QByteArray &json);
    Manifest parseJsonFile(const QString &path);

    bool archiveMentionsInstaller(const QString &archivePath);
    Manifest peekArchive(const QString &archivePath);

    bool confirmAndShow(QWidget *parent, const Manifest &man,
                        const QString &plutoniumRoot = QString(),
                        const QString &gameRoot = QString(),
                        bool *makeBackup = nullptr);
    using ProgressFn = std::function<void(int percent, const QString &label)>;

    QString apply(const Manifest &man,
                  const QString &extractedRoot,
                  const QString &plutoniumRoot,
                  const QString &gameRoot,
                  const QString &archivePath,
                  const ProgressFn &onProgress = nullptr,
                  bool makeBackup = true);
}
