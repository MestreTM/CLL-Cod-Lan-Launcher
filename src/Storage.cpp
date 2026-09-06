#include "Storage.h"

#include <QDir>
#include <QFileInfo>

QString Storage::gameStorageId(const QString &gameName)
{
    if (gameName == "World at War")     return "t4";
    if (gameName == "Black ops")        return "t5";
    if (gameName == "Black ops II")     return "t6";
    if (gameName == "Modern Warfare 3") return "iw5";
    return QString();
}

QString Storage::subdir(const QString &plutoniumInstance, const QString &gameName, const QString &subdirName)
{
    const QString gameStorage = gameStorageId(gameName);
    if (gameStorage.isEmpty() || plutoniumInstance.isEmpty())
        return QString();
    return QDir(plutoniumInstance).filePath("storage/" + gameStorage + "/" + subdirName);
}

QStringList Storage::listModFolders(const QString &path)
{
    if (path.isEmpty())
        return {};
    QDir dir(path);
    if (!dir.exists())
        return {};
    QStringList out;
    for (const QFileInfo &fi : dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name))
        out << fi.fileName();
    return out;
}

QStringList Storage::listConfigFiles(const QString &path)
{
    if (path.isEmpty())
        return {};
    QDir dir(path);
    if (!dir.exists())
        return {};
    QStringList out;
    for (const QFileInfo &fi : dir.entryInfoList(QStringList() << "*.cfg", QDir::Files, QDir::Name))
        out << fi.fileName();
    return out;
}
