#include "ServerPage.h"
#include "AppSettings.h"
#include "../Checkables.h"
#include "Dialogs.h"
#include "Downloader.h"
#include "MakeConfigDialog.h"
#include "Storage.h"

#include <QAbstractSocket>
#include <QApplication>
#include <QClipboard>
#include <QComboBox>
#include <QDialog>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFrame>
#include <QHBoxLayout>
#include <QHostAddress>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QNetworkInterface>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QMetaObject>
#include <QPushButton>
#include <QRadioButton>
#include <QShowEvent>
#include <QStyle>
#include <QTextStream>
#include <QVBoxLayout>
#include <QtConcurrent/QtConcurrent>

namespace {

QList<QPair<QString, QString>> localIpv4Addresses()
{
    QList<QPair<QString, QString>> out;
    out.append({QStringLiteral("127.0.0.1"), QObject::tr("127.0.0.1  —  local (este PC)")});

    const auto ifaces = QNetworkInterface::allInterfaces();
    for (const QNetworkInterface &iface : ifaces) {
        const auto flags = iface.flags();
        if (!(flags & QNetworkInterface::IsUp) || !(flags & QNetworkInterface::IsRunning))
            continue;
        if (flags & QNetworkInterface::IsLoopBack)
            continue;
        for (const QNetworkAddressEntry &entry : iface.addressEntries()) {
            const QHostAddress ip = entry.ip();
            if (ip.protocol() != QAbstractSocket::IPv4Protocol)
                continue;
            if (ip.isNull() || ip.isLoopback())
                continue;
            const QString addr = ip.toString();
            if (addr.startsWith(QLatin1String("169.254.")))
                continue; // APIPA
            out.append({addr, QObject::tr("%1  —  %2").arg(addr, iface.humanReadableName())});
        }
    }
    return out;
}

const QStringList kBo2ConfigFiles = {
    "conf.cfg", "ctf.cfg", "dem.cfg", "dm.cfg", "dom.cfg", "gun.cfg", "hq.cfg",
    "koth.cfg", "oic.cfg", "oneflag.cfg", "sas.cfg", "sd.cfg", "shrp.cfg", "tdm.cfg",
    "zm_classic_prison.cfg", "zm_classic_processing.cfg", "zm_classic_rooftop.cfg",
    "zm_classic_tomb.cfg", "zm_classic_transit.cfg", "zm_cleansed_diner.cfg",
    "zm_cleansed_street.cfg", "zm_grief_cellblock.cfg", "zm_grief_farm.cfg",
    "zm_grief_street.cfg", "zm_grief_town.cfg", "zm_grief_transit.cfg",
    "zm_standard_farm.cfg", "zm_standard_nuked.cfg", "zm_standard_town.cfg",
    "zm_standard_transit.cfg",
};

const QString kBo2ConfigsBaseUrl =
    "https://raw.githubusercontent.com/xerxes-at/T6ServerConfigs/refs/heads/master/"
    "localappdata/Plutonium/storage/t6/gamesettings/";

const QString kT5ConfigsBaseUrl =
    "https://raw.githubusercontent.com/xerxes-at/T5ServerConfig/refs/heads/master/"
    "localappdata/Plutonium/storage/t5/";

const QStringList kT5ConfigFiles = { "dedicated.cfg", "dedicated_sp.cfg" };

bool t5BaseReady(const QString &pu)
{
    return QFileInfo::exists(pu + "/storage/t5/dedicated.cfg")
        && QFileInfo::exists(pu + "/storage/t5/dedicated_sp.cfg");
}

const QString kT4ConfigsBaseUrl =
    "https://raw.githubusercontent.com/xerxes-at/T4ServerConfigs/refs/heads/main/main/";

const QStringList kT4ConfigFiles = { "server.cfg", "server_zm.cfg", "server_coop.cfg" };

bool t4BaseReady(const QString &pu)
{
    return QFileInfo::exists(pu + "/storage/t4/main/server.cfg")
        && QFileInfo::exists(pu + "/storage/t4/main/server_zm.cfg");
}

} // namespace

ServerPage::ServerPage(AppSettings &settings, QWidget *parent)
    : QWidget(parent)
    , m_settings(settings)
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(28, 22, 28, 22);
    root->setSpacing(14);

    auto *contextRow = new QHBoxLayout();
    contextRow->setSpacing(8);
    m_gameCombo = new QComboBox(this);
    m_gameCombo->addItems({"World at War", "Black ops", "Black ops II"});
    m_gameCombo->setCurrentText("Black ops II");
    m_gameCombo->setMinimumWidth(210);
    auto *refreshBtn = new QPushButton(tr("Atualizar"), this);
    refreshBtn->setObjectName("srvRefresh");
    refreshBtn->setProperty("cssClass", "ghost");
    auto *deselectBtn = new QPushButton(tr("Limpar selecao"), this);
    deselectBtn->setObjectName("srvDeselect");
    deselectBtn->setProperty("cssClass", "ghost");
    m_titleLabel = new QLabel(tr("Selecione um jogo para ver as configs"), this);
    m_titleLabel->setObjectName("MutedHint");
    contextRow->addWidget(m_gameCombo);
    contextRow->addWidget(refreshBtn);
    contextRow->addWidget(deselectBtn);
    contextRow->addStretch();
    contextRow->addWidget(m_titleLabel);
    root->addLayout(contextRow);

    auto *middleRow = new QHBoxLayout();
    middleRow->setSpacing(14);

    // Options card (narrow fixed column)
    auto *optsCard = new QFrame(this);
    optsCard->setObjectName("Card");
    optsCard->setFixedWidth(300);
    auto *optsLayout = new QVBoxLayout(optsCard);
    optsLayout->setContentsMargins(18, 15, 18, 16);
    optsLayout->setSpacing(8);
    auto *optsKicker = new QLabel(tr("PARTIDA"), optsCard);
    optsKicker->setObjectName("srvOptsKicker");
    optsKicker->setObjectName("CardKicker");
    optsLayout->addWidget(optsKicker);
    auto *optsTitle = new QLabel(tr("Como hospedar"), optsCard);
    optsTitle->setObjectName("srvOptsTitle");
    optsTitle->setObjectName("CardTitle");
    optsLayout->addWidget(optsTitle);
    optsLayout->addSpacing(2);

    auto *modeLabel = new QLabel(tr("Modo"), optsCard);
    modeLabel->setObjectName("srvModeLabel");
    modeLabel->setObjectName("FieldLabel");
    optsLayout->addWidget(modeLabel);
    auto *modeRow = new QHBoxLayout();
    modeRow->setSpacing(16);
    m_zmRadio = new Ui::RadioButton(tr("ZM"), optsCard);
    m_mpRadio = new Ui::RadioButton(tr("MP"), optsCard);
    m_zmRadio->setChecked(true);
    modeRow->addWidget(m_zmRadio);
    modeRow->addWidget(m_mpRadio);
    modeRow->addStretch();
    optsLayout->addLayout(modeRow);
    optsLayout->addSpacing(4);

    auto *portLabel = new QLabel(tr("Porta"), optsCard);
    portLabel->setObjectName("srvPortLabel");
    portLabel->setObjectName("FieldLabel");
    optsLayout->addWidget(portLabel);
    m_portEdit = new QLineEdit("5000", optsCard);
    m_portEdit->setMaximumWidth(120);
    optsLayout->addWidget(m_portEdit);
    optsLayout->addSpacing(4);

    auto *ipLabel = new QLabel(tr("Endereco IP"), optsCard);
    ipLabel->setObjectName("srvIpLabel");
    ipLabel->setObjectName("FieldLabel");
    optsLayout->addWidget(ipLabel);
    m_ipCombo = new QComboBox(optsCard);
    optsLayout->addWidget(m_ipCombo);
    auto *refreshIpBtn = new QPushButton(tr("Atualizar IPs"), optsCard);
    refreshIpBtn->setObjectName("srvRefreshIp");
    refreshIpBtn->setProperty("cssClass", "ghost");
    optsLayout->addWidget(refreshIpBtn, 0, Qt::AlignLeft);
    optsLayout->addStretch();
    m_connectHelpBtn = new QPushButton(tr("Como se conectar"), optsCard);
    optsLayout->addWidget(m_connectHelpBtn);
    middleRow->addWidget(optsCard);

    // Config list card
    auto *listCard = new QFrame(this);
    listCard->setObjectName("Card");
    auto *listLayout = new QVBoxLayout(listCard);
    listLayout->setContentsMargins(18, 15, 18, 16);
    listLayout->setSpacing(10);
    auto *listKicker = new QLabel(tr("CONFIGS"), listCard);
    listKicker->setObjectName("srvListKicker");
    listKicker->setObjectName("CardKicker");
    listLayout->addWidget(listKicker);
    auto *listTitle = new QLabel(tr("Disponiveis nesta instalacao"), listCard);
    listTitle->setObjectName("srvListTitle");
    listTitle->setObjectName("CardTitle");
    listLayout->addWidget(listTitle);
    m_configList = new QListWidget(listCard);
    listLayout->addWidget(m_configList, 1);
    auto *cfgRow = new QHBoxLayout();
    cfgRow->setSpacing(8);
    m_makeConfigBtn = new QPushButton(tr("Criar config"), listCard);
    m_editConfigBtn = new QPushButton(tr("Editar"), listCard);
    auto *deleteBtn = new QPushButton(tr("Excluir"), listCard);
    deleteBtn->setProperty("cssClass", "danger");
    cfgRow->addWidget(m_makeConfigBtn);
    cfgRow->addWidget(m_editConfigBtn);
    cfgRow->addStretch();
    cfgRow->addWidget(deleteBtn);
    listLayout->addLayout(cfgRow);
    m_progressLabel = new QLabel(listCard);
    m_progressLabel->setObjectName("MutedHint");
    m_progress = new QProgressBar(listCard);
    m_progress->setRange(0, 100);
    m_progress->setValue(0);
    m_progress->setVisible(false);
    m_progressLabel->setVisible(false);
    listLayout->addWidget(m_progressLabel);
    listLayout->addWidget(m_progress);
    middleRow->addWidget(listCard, 1);

    root->addLayout(middleRow, 1);

    auto *btnRow = new QHBoxLayout();
    auto *launchHint = new QLabel(tr("Escolha uma config e inicie o servidor."), this);
    launchHint->setObjectName("MutedHint");
    m_launchBtn = new QPushButton(tr("Iniciar servidor"), this);
    m_launchBtn->setProperty("cssClass", "primary");
    m_launchBtn->setMinimumSize(170, 40);
    m_launchBtn->setCursor(Qt::PointingHandCursor);
    btnRow->addWidget(launchHint);
    btnRow->addStretch();
    btnRow->addWidget(m_launchBtn);
    root->addLayout(btnRow);

    refreshIpList();

    connect(m_gameCombo, &QComboBox::currentTextChanged, this, &ServerPage::onGameSelected);
    connect(refreshBtn, &QPushButton::clicked, this, &ServerPage::onRefreshOrDeselect);
    connect(deselectBtn, &QPushButton::clicked, this, &ServerPage::onRefreshOrDeselect);
    connect(deleteBtn, &QPushButton::clicked, this, &ServerPage::onDeleteConfig);
    connect(m_makeConfigBtn, &QPushButton::clicked, this, &ServerPage::onMakeConfig);
    connect(m_editConfigBtn, &QPushButton::clicked, this, &ServerPage::onEditConfig);
    connect(m_connectHelpBtn, &QPushButton::clicked, this, &ServerPage::onHowToConnect);
    connect(m_launchBtn, &QPushButton::clicked, this, &ServerPage::onLaunchServer);
    connect(refreshIpBtn, &QPushButton::clicked, this, &ServerPage::refreshIpList);

    connect(&m_gameSettingsWatcher, &QFutureWatcher<bool>::finished, this, [this]() {
        m_makeConfigBtn->setDisabled(false);
        if (m_progress) m_progress->setVisible(false);
        if (m_progressLabel) m_progressLabel->setVisible(false);
        if (m_gameSettingsWatcher.result()) {
            if (m_downloadKind == QLatin1String("t5"))
                Dialogs::info(this, Dialogs::Msg::DownloadedT5Configs, m_settings);
            else if (m_downloadKind == QLatin1String("t4"))
                Dialogs::info(this, Dialogs::Msg::DownloadedT4Configs, m_settings);
            else
                Dialogs::info(this, Dialogs::Msg::DownloadedMainConfigs, m_settings);
        } else {
            Dialogs::error(this, m_downloadKind == QLatin1String("t5")
                                     ? tr("Nao foi possivel baixar as configs do Black Ops (T5).")
                                     : m_downloadKind == QLatin1String("t4")
                                     ? tr("Nao foi possivel baixar as configs do World at War (T4).")
                                     : tr("Nao foi possivel baixar as configuracoes principais do Black Ops II."));
        }
    });

    onGameSelected(m_gameCombo->currentText());
}

void ServerPage::selectGame(const QString &gameId)
{
    if (gameId.isEmpty() || !m_gameCombo)
        return;
    const int idx = m_gameCombo->findText(gameId);
    if (idx >= 0)
        m_gameCombo->setCurrentIndex(idx);
    onGameSelected(m_gameCombo->currentText());
}

void ServerPage::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    refreshIpList();
}

void ServerPage::refreshIpList()
{
    const QString previous = selectedIp();
    m_ipCombo->clear();
    const auto addrs = localIpv4Addresses();
    int restore = 0;
    for (int i = 0; i < addrs.size(); ++i) {
        m_ipCombo->addItem(addrs[i].second, addrs[i].first);
        if (addrs[i].first == previous)
            restore = i;
    }
    // Prefer the first non-loopback NIC IP when nothing was selected
    if (previous.isEmpty() && m_ipCombo->count() > 1)
        m_ipCombo->setCurrentIndex(1);
    else
        m_ipCombo->setCurrentIndex(restore);
}

QString ServerPage::selectedIp() const
{
    if (!m_ipCombo)
        return QStringLiteral("127.0.0.1");
    const QVariant v = m_ipCombo->currentData();
    return v.isValid() ? v.toString() : QStringLiteral("127.0.0.1");
}

QString ServerPage::selectedConfigPath() const
{
    auto *item = m_configList->currentItem();
    if (!item)
        return QString();
    const QString dir = Storage::subdir(m_settings.plutoniumInstance, m_settings.serverId, "main");
    if (dir.isEmpty())
        return QString();
    return dir + "/" + item->text();
}

void ServerPage::refreshList()
{
    if (m_settings.serverId.isEmpty())
        return;
    const QString path = Storage::subdir(m_settings.plutoniumInstance, m_settings.serverId, "main");
    m_configList->clear();
    m_configList->addItems(Storage::listConfigFiles(path));
}

void ServerPage::onGameSelected(const QString &gameName)
{
    if (gameName.isEmpty())
        return;
    m_settings.serverId = gameName;
    m_titleLabel->setText(tr("Configs disponiveis para %1").arg(gameName));
    refreshList();
}

void ServerPage::onRefreshOrDeselect()
{
    if (m_gameCombo->currentText().isEmpty())
        return;
    m_settings.serverId = m_gameCombo->currentText();
    refreshList();
}

void ServerPage::onDeleteConfig()
{
    const QString path = Storage::subdir(m_settings.plutoniumInstance, m_settings.serverId, "main");
    if (path.isEmpty())
        return;

    auto *item = m_configList->currentItem();
    if (!item) {
        Dialogs::info(this, Dialogs::Msg::NoCfg, m_settings);
        return;
    }

    QFile::remove(path + "/" + item->text());
    refreshList();
}

void ServerPage::onMakeConfig()
{
    const QString serverId = m_gameCombo->currentText();
    if (serverId.isEmpty())
        return;
    m_settings.serverId = serverId;

    if (m_mpRadio->isChecked()
        && serverId != "Black ops"
        && serverId != "World at War"
        && serverId != "Black ops II") {
        Dialogs::info(this, Dialogs::Msg::MultiConfigUnsupported, m_settings);
        return;
    }
    if (serverId == "Black ops II" &&
        !QDir(m_settings.plutoniumInstance + "/storage/t6/gamesettings").exists()) {
        if (Dialogs::confirmDownloadGameSettings(this))
            downloadBo2GameSettings();
        return;
    }
    if (serverId == "Black ops" && !t5BaseReady(m_settings.plutoniumInstance)) {
        if (Dialogs::confirmDownloadT5Settings(this))
            downloadT5BaseConfigs();
        return;
    }
    if (serverId == "World at War" && !t4BaseReady(m_settings.plutoniumInstance)) {
        if (Dialogs::confirmDownloadT4Settings(this))
            downloadT4BaseConfigs();
        return;
    }

    auto *dlg = new MakeConfigDialog(m_settings, serverId, m_mpRadio->isChecked(), this);
    connect(dlg, &MakeConfigDialog::configCreated, this, &ServerPage::refreshList);
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->exec();
}

void ServerPage::onEditConfig()
{
    const QString path = selectedConfigPath();
    if (path.isEmpty()) {
        Dialogs::info(this, Dialogs::Msg::NoCfg, m_settings);
        return;
    }
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        Dialogs::error(this, tr("Nao foi possivel abrir %1").arg(path));
        return;
    }
    const QString original = QString::fromUtf8(file.readAll());
    file.close();

    QDialog dlg(this);
    dlg.setWindowTitle(tr("Editar config — %1").arg(QFileInfo(path).fileName()));
    dlg.resize(640, 480);
    auto *vl = new QVBoxLayout(&dlg);
    auto *hint = new QLabel(tr("Arquivo: %1").arg(path), &dlg);
    hint->setObjectName("MutedHint");
    hint->setWordWrap(true);
    auto *edit = new QPlainTextEdit(&dlg);
    edit->setPlainText(original);
    edit->setLineWrapMode(QPlainTextEdit::NoWrap);
    vl->addWidget(hint);
    vl->addWidget(edit, 1);
    auto *row = new QHBoxLayout();
    auto *cancel = new QPushButton(tr("Cancelar"), &dlg);
    auto *save = new QPushButton(tr("Salvar"), &dlg);
    save->setProperty("cssClass", "primary");
    row->addStretch();
    row->addWidget(cancel);
    row->addWidget(save);
    vl->addLayout(row);
    connect(cancel, &QPushButton::clicked, &dlg, &QDialog::reject);
    connect(save, &QPushButton::clicked, &dlg, &QDialog::accept);
    if (dlg.exec() != QDialog::Accepted)
        return;
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        Dialogs::error(this, tr("Nao foi possivel salvar %1").arg(path));
        return;
    }
    QTextStream out(&file);
    out << edit->toPlainText();
}

void ServerPage::onHowToConnect()
{
    const QString ip = selectedIp();
    const QString port = m_portEdit->text().trimmed().isEmpty() ? QStringLiteral("5000")
                                                                : m_portEdit->text().trimmed();
    const QString cmd = QStringLiteral("connect %1:%2").arg(ip, port);

    QDialog dlg(this);
    dlg.setWindowTitle(tr("Como se conectar"));
    dlg.setMinimumWidth(480);
    auto *vl = new QVBoxLayout(&dlg);
    auto *title = new QLabel(tr("Entrar no servidor LAN"), &dlg);
    title->setObjectName("WizardLead");
    auto *body = new QLabel(&dlg);
    body->setWordWrap(true);
    body->setTextFormat(Qt::RichText);
    body->setText(tr(
        "<ol style='margin-left:18px; line-height:1.6'>"
        "<li>Dentro do jogo, aperte a tecla <b>'</b> (aspas simples).<br/>"
        "Ela fica em geral <b>abaixo do ESC</b> e abre o console.</li>"
        "<li>Digite o comando e pressione Enter:</li>"
        "</ol>"));
    auto *cmdBox = new QLineEdit(cmd, &dlg);
    cmdBox->setReadOnly(true);
    cmdBox->setObjectName("MonoField");
    auto *note = new QLabel(tr("Use 127.0.0.1 se voce esta no mesmo PC do servidor. "
                               "Use o IP da rede (ex.: 192.168.x.x) para outros PCs na LAN."), &dlg);
    note->setWordWrap(true);
    note->setObjectName("MutedHint");
    vl->addWidget(title);
    vl->addWidget(body);
    vl->addWidget(cmdBox);
    vl->addWidget(note);
    auto *row = new QHBoxLayout();
    auto *copy = new QPushButton(tr("Copiar comando"), &dlg);
    copy->setProperty("cssClass", "primary");
    auto *close = new QPushButton(tr("Fechar"), &dlg);
    row->addStretch();
    row->addWidget(copy);
    row->addWidget(close);
    vl->addLayout(row);
    connect(copy, &QPushButton::clicked, &dlg, [cmd, copy]() {
        QApplication::clipboard()->setText(cmd);
        copy->setText(QObject::tr("Copiado"));
    });
    connect(close, &QPushButton::clicked, &dlg, &QDialog::accept);
    dlg.exec();
}

void ServerPage::downloadBo2GameSettings()
{
    m_downloadKind = QStringLiteral("t6");
    m_makeConfigBtn->setDisabled(true);
    if (m_progress) {
        m_progress->setVisible(true);
        m_progress->setRange(0, 100);
        m_progress->setValue(0);
    }
    if (m_progressLabel) {
        m_progressLabel->setVisible(true);
        m_progressLabel->setText(tr("Downloading base configs..."));
    }
    const QString destDir = m_settings.plutoniumInstance + "/storage/t6/gamesettings";
    const int total = kBo2ConfigFiles.size();

    auto future = QtConcurrent::run([this, destDir, total]() -> bool {
        QDir().mkpath(destDir);
        int i = 0;
        for (const QString &file : kBo2ConfigFiles) {
            QMetaObject::invokeMethod(this, [this, i, total, file]() {
                if (m_progress) {
                    m_progress->setRange(0, 100);
                    m_progress->setValue(total > 0 ? int(i * 100 / total) : 0);
                }
                if (m_progressLabel)
                    m_progressLabel->setText(tr("Downloading %1 (%2/%3)").arg(file).arg(i + 1).arg(total));
            }, Qt::QueuedConnection);
            QString error;
            if (!Downloader::downloadToFile(kBo2ConfigsBaseUrl + file, destDir + "/" + file, error))
                return false;
            ++i;
        }
        return true;
    });
    m_gameSettingsWatcher.setFuture(future);
}


void ServerPage::downloadT5BaseConfigs()
{
    m_downloadKind = QStringLiteral("t5");
    m_makeConfigBtn->setDisabled(true);
    if (m_progress) {
        m_progress->setVisible(true);
        m_progress->setRange(0, 100);
        m_progress->setValue(0);
    }
    if (m_progressLabel) {
        m_progressLabel->setVisible(true);
        m_progressLabel->setText(tr("Downloading base configs..."));
    }
    const QString destDir = m_settings.plutoniumInstance + "/storage/t5";
    const int total = kT5ConfigFiles.size();
    auto future = QtConcurrent::run([this, destDir, total]() -> bool {
        QDir().mkpath(destDir);
        int i = 0;
        for (const QString &file : kT5ConfigFiles) {
            QMetaObject::invokeMethod(this, [this, i, total, file]() {
                if (m_progress) {
                    m_progress->setRange(0, 100);
                    m_progress->setValue(total > 0 ? int(i * 100 / total) : 0);
                }
                if (m_progressLabel)
                    m_progressLabel->setText(tr("Downloading %1 (%2/%3)").arg(file).arg(i + 1).arg(total));
            }, Qt::QueuedConnection);
            QString error;
            if (!Downloader::downloadToFile(kT5ConfigsBaseUrl + file, destDir + "/" + file, error))
                return false;
            ++i;
        }
        return true;
    });
    m_gameSettingsWatcher.setFuture(future);
}

void ServerPage::downloadT4BaseConfigs()
{
    m_downloadKind = QStringLiteral("t4");
    m_makeConfigBtn->setDisabled(true);
    if (m_progress) {
        m_progress->setVisible(true);
        m_progress->setRange(0, 100);
        m_progress->setValue(0);
    }
    if (m_progressLabel) {
        m_progressLabel->setVisible(true);
        m_progressLabel->setText(tr("Downloading base configs..."));
    }
    const QString destDir = m_settings.plutoniumInstance + "/storage/t4/main";
    const int total = kT4ConfigFiles.size();
    auto future = QtConcurrent::run([this, destDir, total]() -> bool {
        QDir().mkpath(destDir);
        int i = 0;
        for (const QString &file : kT4ConfigFiles) {
            QMetaObject::invokeMethod(this, [this, i, total, file]() {
                if (m_progress) {
                    m_progress->setRange(0, 100);
                    m_progress->setValue(total > 0 ? int(i * 100 / total) : 0);
                }
                if (m_progressLabel)
                    m_progressLabel->setText(tr("Downloading %1 (%2/%3)").arg(file).arg(i + 1).arg(total));
            }, Qt::QueuedConnection);
            QString error;
            if (!Downloader::downloadToFile(kT4ConfigsBaseUrl + file, destDir + "/" + file, error))
                return false;
            ++i;
        }
        return true;
    });
    m_gameSettingsWatcher.setFuture(future);
}

void ServerPage::onLaunchServer()
{
    if (m_running) {
        emit stopServerRequested();
        return;
    }

    const QString serverId = m_gameCombo->currentText();
    if (serverId.isEmpty())
        return;
    m_settings.serverId = serverId;
    m_settings.serverMultiplayerSelected = m_mpRadio->isChecked();

    if (serverId == "World at War")
        m_settings.modeId = m_mpRadio->isChecked() ? "t4mp" : "t4sp";
    else if (serverId == "Black ops")
        m_settings.modeId = m_mpRadio->isChecked() ? "t5mp" : "t5sp";
    else if (serverId == "Black ops II")
        m_settings.modeId = m_mpRadio->isChecked() ? "t6mp" : "t6zm";
    else if (serverId == "Modern Warfare 3")
        m_settings.modeId = "iw5mp";

    auto *item = m_configList->currentItem();
    if (!item) {
        Dialogs::info(this, Dialogs::Msg::NoCfg, m_settings);
        return;
    }

    emit launchServerRequested(item->text(), m_portEdit->text());
}

void ServerPage::setRunning(bool running)
{
    m_running = running;
    if (!m_launchBtn)
        return;
    if (running) {
        m_launchBtn->setText(tr("Encerrar servidor"));
        m_launchBtn->setProperty("cssClass", "danger");
    } else {
        m_launchBtn->setText(tr("Iniciar servidor"));
        m_launchBtn->setProperty("cssClass", "primary");
    }
    m_launchBtn->style()->unpolish(m_launchBtn);
    m_launchBtn->style()->polish(m_launchBtn);
}

void ServerPage::retranslate()
{
    if (auto *w = findChild<QPushButton*>("srvRefresh")) w->setText(tr("Atualizar"));
    if (auto *w = findChild<QPushButton*>("srvDeselect")) w->setText(tr("Limpar selecao"));
    if (auto *w = findChild<QLabel*>("srvOptsKicker")) w->setText(tr("PARTIDA"));
    if (auto *w = findChild<QLabel*>("srvOptsTitle")) w->setText(tr("Como hospedar"));
    if (auto *w = findChild<QLabel*>("srvModeLabel")) w->setText(tr("Modo"));
    if (auto *w = findChild<QLabel*>("srvPortLabel")) w->setText(tr("Porta"));
    if (auto *w = findChild<QLabel*>("srvIpLabel")) w->setText(tr("Endereco IP"));
    if (auto *w = findChild<QPushButton*>("srvRefreshIp")) w->setText(tr("Atualizar IPs"));
    if (auto *w = findChild<QLabel*>("srvListKicker")) w->setText(tr("CONFIGS"));
    if (auto *w = findChild<QLabel*>("srvListTitle")) w->setText(tr("Disponiveis nesta instalacao"));
    if (m_connectHelpBtn) m_connectHelpBtn->setText(tr("Como se conectar"));
    if (m_makeConfigBtn) m_makeConfigBtn->setText(tr("Criar config"));
    if (m_editConfigBtn) m_editConfigBtn->setText(tr("Editar"));
    if (m_zmRadio) m_zmRadio->setText(tr("ZM"));
    if (m_mpRadio) m_mpRadio->setText(tr("MP"));
    if (m_launchBtn)
        m_launchBtn->setText(m_running ? tr("Encerrar servidor") : tr("Iniciar servidor"));
    if (m_titleLabel) {
        if (!m_settings.serverId.isEmpty())
            m_titleLabel->setText(tr("Configs disponiveis para %1").arg(m_settings.serverId));
        else
            m_titleLabel->setText(tr("Selecione um jogo para ver as configs"));
    }
    refreshIpList();
}
