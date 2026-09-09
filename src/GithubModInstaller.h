#pragma once

#include "ModPreview.h"

#include <QString>
#include <QStringList>
#include <QList>
#include <functional>

class QWidget;

namespace GithubModInstaller
{
    struct RemoteFile {
        QString relpath;
        QString root;
        qint64 size = 0;
        QString md5;
        QString sha256;
        QString url;
    };

    struct RetireFile {
        QString relpath;
        QString root;
        QString note;
    };

    struct Bundle {
        QString assetName;
        QString url;
        qint64 size = 0;
        QString sha256;
        bool valid() const { return !url.isEmpty(); }
    };

    struct FailedDownload {
        QString name;
        QString url;
        QString destPath;
        QString error;
    };

    struct Manifest {
        bool valid = false;
        QString error;
        QString sourceUrl;
        QString releaseTag;
        QString releaseVersion;
        QString layoutId;
        QString shortHash;
        QString gameCode;
        QString gameId;
        bool hasExecutables = false;
        QStringList executableNames;
        QList<RemoteFile> files;
        QList<RetireFile> retire;
        Bundle bundle;
    };

    bool looksLikeUrl(const QString &text);
    QString resolveManifestUrl(const QString &input, QString *error = nullptr);

    Manifest parseBytes(const QByteArray &json, const QString &sourceUrl = QString());
    Manifest fetchManifest(const QString &input, QString &error);

    // Downloads manifest.json then mod.json and builds the same preview used by zip packs.
    Manifest peek(const QString &input,
                  const QString &fallbackGameId,
                  const QString &fallbackGameCode,
                  const QString &plutoniumRoot,
                  const QString &gameRoot,
                  ModPreview::Preview *previewOut,
                  QString &error);

    using ProgressFn = std::function<void(int percent, const QString &label)>;

    QString runUpdateCheck(ModPreview::Preview &preview, const ProgressFn &onProgress = nullptr);

    QString apply(const Manifest &man,
                  const ModPreview::Preview &preview,
                  const QString &plutoniumRoot,
                  const QString &gameRoot,
                  const ProgressFn &onProgress = nullptr,
                  QList<FailedDownload> *failedOut = nullptr);
}
