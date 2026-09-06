#include "ImportKitDialog.h"
#include "AppSettings.h"
#include "ArchiveTool.h"
#include "Downloader.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QMetaObject>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QVBoxLayout>
#include <QtConcurrent/QtConcurrent>

QString ImportKitDialog::puDatUrl()
{
    return QStringLiteral("https://github.com/MestreTM/CLL-CodLanLauncher/releases/download/v0.1/pu.dat");
}

QString ImportKitDialog::destPu()
{
    return AppSettings::localPuDir();
}

ImportKitDialog::ImportKitDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Kit portatil"));
    resize(560, 360);

    auto *lay = new QVBoxLayout(this);
    auto *intro = new QLabel(
        tr("Baixa pu.dat e extrai em:\n%1").arg(destPu()), this);
    intro->setWordWrap(true);
    lay->addWidget(intro);

    m_status = new QLabel(tr("Pronto para baixar."), this);
    m_status->setWordWrap(true);
    lay->addWidget(m_status);

    m_bar = new QProgressBar(this);
    m_bar->setRange(0, 100);
    m_bar->setValue(0);
    lay->addWidget(m_bar);

    m_log = new QPlainTextEdit(this);
    m_log->setReadOnly(true);
    lay->addWidget(m_log, 1);

    auto *row = new QHBoxLayout();
    m_startBtn = new QPushButton(tr("Baixar pu.dat"), this);
    m_startBtn->setProperty("cssClass", "primary");
    m_closeBtn = new QPushButton(tr("Fechar"), this);
    row->addStretch();
    row->addWidget(m_startBtn);
    row->addWidget(m_closeBtn);
    lay->addLayout(row);

    connect(m_startBtn, &QPushButton::clicked, this, &ImportKitDialog::startDownload);
    connect(m_closeBtn, &QPushButton::clicked, this, &QDialog::accept);
    connect(&m_watcher, &QFutureWatcher<QString>::finished, this, &ImportKitDialog::onFinished);
}

void ImportKitDialog::startDownload()
{
    m_startBtn->setEnabled(false);
    m_closeBtn->setEnabled(false);
    m_ok = false;
    m_bar->setRange(0, 0);
    m_status->setText(tr("Baixando..."));
    m_log->appendPlainText(tr("URL: %1").arg(puDatUrl()));

    auto future = QtConcurrent::run([this]() -> QString {
        const QString puDir = destPu();
        const QString appDir = QCoreApplication::applicationDirPath();

        auto note = [this](qint64 got, qint64 total, const QString &label) {
            QMetaObject::invokeMethod(this, [this, got, total, label]() {
                if (total > 0) {
                    m_bar->setRange(0, 100);
                    m_bar->setValue(int(got * 100 / total));
                } else {
                    m_bar->setRange(0, 0);
                }
                m_status->setText(label);
                m_log->appendPlainText(label);
            }, Qt::QueuedConnection);
        };

        if (!ArchiveTool::hasSevenZip()) {
            note(0, 0, tr("Baixando 7-Zip..."));
            const QString zipPath = QDir::temp().filePath("LanLauncherQt_7z.zip");
            QString err;
            if (!Downloader::downloadToFile(
                    QStringLiteral("https://raw.githubusercontent.com/JugAndDoubleTap/LanLauncher/main/7z.zip"),
                    zipPath, err,
                    [note](qint64 g, qint64 t) {
                        note(g, t, QObject::tr("7-Zip %1 / %2 KB").arg(g / 1024).arg(t > 0 ? t / 1024 : 0));
                    })) {
                return tr("Falha ao baixar o 7-Zip: %1").arg(err);
            }
            if (!ArchiveTool::extractBootstrapZip(zipPath, appDir)) {
                QFile::remove(zipPath);
                return tr("Falha ao instalar o 7-Zip.");
            }
            QFile::remove(zipPath);
        }

        const QString datPath = QDir::temp().filePath("LanLauncher_pu.dat");
        QString err;
        note(0, 0, tr("Baixando pu.dat..."));
        if (!Downloader::downloadToFile(
                puDatUrl(), datPath, err,
                [note](qint64 g, qint64 t) {
                    note(g, t, QObject::tr("pu.dat %1 / %2 MB")
                                  .arg(g / (1024 * 1024))
                                  .arg(t > 0 ? t / (1024 * 1024) : 0));
                },
                30 * 60 * 1000)) {
            QFile::remove(datPath);
            return tr("Nao foi possivel baixar pu.dat:\n%1").arg(err);
        }

        note(0, 0, tr("Extraindo para pu/..."));
        QDir().mkpath(puDir);
        QString extractErr;
        if (!ArchiveTool::extractObfuscatedBundle(datPath, puDir, &extractErr)) {
            QFile::remove(datPath);
            return extractErr.isEmpty() ? tr("Falha ao extrair pu.dat") : extractErr;
        }
        QFile::remove(datPath);
        if (!QFileInfo::exists(AppSettings::localPuBootstrapper()))
            return tr("Extraido, mas o bootstrapper nao apareceu em pu/bin.");
        return QString();
    });
    m_watcher.setFuture(future);
}

void ImportKitDialog::onFinished()
{
    m_closeBtn->setEnabled(true);
    const QString err = m_watcher.result();
    m_bar->setRange(0, 100);
    if (err.isEmpty()) {
        m_ok = true;
        m_bar->setValue(100);
        m_status->setText(tr("Kit instalado em %1").arg(destPu()));
        m_log->appendPlainText(tr("OK."));
        m_startBtn->setEnabled(false);
    } else {
        m_ok = false;
        m_bar->setValue(0);
        m_status->setText(err);
        m_log->appendPlainText(tr("ERRO: %1").arg(err));
        m_startBtn->setEnabled(true);
    }
}
