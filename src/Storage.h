#pragma once
#include <QString>
#include <QStringList>

namespace Storage
{
    // GAME_STORAGE = { "World at War": "t4", ... }
    QString gameStorageId(const QString &gameName);

    // StorageSubdir(plutoniuminstance, game_name, subdir)
    QString subdir(const QString &plutoniumInstance, const QString &gameName, const QString &subdir);

    // ListModFolders(path)
    QStringList listModFolders(const QString &path);

    // ListConfigFiles(path)
    QStringList listConfigFiles(const QString &path);
}
