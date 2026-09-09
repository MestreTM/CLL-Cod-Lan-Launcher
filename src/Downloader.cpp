#include "Downloader.h"

#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>
#include <QUrl>

namespace Downloader {

bool downloadToFile(const QString &url, const QString &destPath, QString &error,
                     const std::function<void(qint64, qint64)> &onProgress,
                     int timeoutMs)
{
    QDir().mkpath(QFileInfo(destPath).absolutePath());

    QFile file(destPath);
    if (!file.open(QIODevice::WriteOnly)) {
        error = QObject::tr("Nao foi possivel criar o arquivo: %1").arg(destPath);
        return false;
    }

    QNetworkAccessManager manager;
    QNetworkRequest request{QUrl(url)};
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("CodLanLaucher/1.1.0"));

    QNetworkReply *reply = manager.get(request);

    QEventLoop loop;
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    QObject::connect(reply, &QNetworkReply::readyRead, [&]() {
        file.write(reply->readAll());
    });
    if (onProgress) {
        QObject::connect(reply, &QNetworkReply::downloadProgress, [&](qint64 got, qint64 total) {
            onProgress(got, total);
        });
    }

    // safety timeout (2 minutes) so a dead network cannot hang forever
    QTimer timeoutTimer;
    timeoutTimer.setSingleShot(true);
    QObject::connect(&timeoutTimer, &QTimer::timeout, &loop, &QEventLoop::quit);
    timeoutTimer.start(timeoutMs > 0 ? timeoutMs : 120000);

    loop.exec();

    file.write(reply->readAll());
    file.close();

    const bool ok = reply->error() == QNetworkReply::NoError && timeoutTimer.isActive();
    if (!ok) {
        error = reply->error() != QNetworkReply::NoError
                    ? reply->errorString()
                    : QObject::tr("Tempo esgotado ao baixar %1").arg(url);
        file.remove();
    }
    reply->deleteLater();
    return ok;
}

QByteArray downloadBytes(const QString &url, QString &error, int timeoutMs)
{
    QNetworkAccessManager manager;
    QNetworkRequest request{QUrl(url)};
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("CodLanLaucher/1.1.0"));
    QNetworkReply *reply = manager.get(request);

    QEventLoop loop;
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    QTimer timeoutTimer;
    timeoutTimer.setSingleShot(true);
    QObject::connect(&timeoutTimer, &QTimer::timeout, &loop, &QEventLoop::quit);
    timeoutTimer.start(timeoutMs > 0 ? timeoutMs : 60000);
    loop.exec();

    QByteArray data;
    const bool ok = reply->error() == QNetworkReply::NoError && timeoutTimer.isActive();
    if (ok)
        data = reply->readAll();
    else
        error = reply->error() != QNetworkReply::NoError
                    ? reply->errorString()
                    : QObject::tr("Tempo esgotado ao baixar %1").arg(url);
    reply->deleteLater();
    return data;
}

} // namespace Downloader
