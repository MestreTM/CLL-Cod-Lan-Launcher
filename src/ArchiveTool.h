#pragma once
#include <QString>
#include <QStringList>
#include <QList>
#include <functional>

namespace ArchiveTool
{
    enum class ExtractResult
    {
        Success,
        UnsupportedExtension,
        NotStandardModFormat,
        Failed,
        SevenZipMissing,
    };

    // Looks for 7z.exe next to the app (7z/ folder) or on PATH.
    QString findSevenZip();
    bool hasSevenZip();

    ExtractResult extractArchive(const QString &modFolder, const QString &archivePath);
    bool extractToDirectory(const QString &archivePath, const QString &destDir, QString *errorOut = nullptr,
                           const std::function<void(int percent)> &onProgress = nullptr);
    QStringList listEntries(const QString &archivePath);

    struct ListedFile {
        QString path;
        qint64 size = 0;
        bool isDir = false;
    };
    QList<ListedFile> listDetailed(const QString &archivePath);
    bool extractPaths(const QString &archivePath, const QString &destDir,
                      const QStringList &innerPaths, QString *errorOut = nullptr);

    // pu.dat = LLQTPKG1 header + XOR'd 7z. This is not real encryption.
    bool extractObfuscatedBundle(const QString &datPath, const QString &destDir, QString *errorOut = nullptr);

    bool extractBootstrapZip(const QString &zipPath, const QString &destDir);
}
