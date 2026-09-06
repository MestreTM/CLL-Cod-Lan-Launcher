#include "ModsPage.h"
#include "AppSettings.h"
#include "ArchiveTool.h"
#include "CllInstaller.h"
#include "GameCatalog.h"
#include "Dialogs.h"
#include "Downloader.h"
#include "SmartModInstaller.h"
#include "Storage.h"

#include <QComboBox>
#include <QSet>
#include <algorithm>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QProgressBar>
#include <QPushButton>
#include <QVBoxLayout>
#include <QtConcurrent/QtConcurrent>
#include <QMetaObject>
#include <QMimeData>
#include <QUrl>

ModsPage::ModsPage(AppSettings &settings, QWidget *parent)
    : QWidget(parent)
    , m_settings(settings)
{
    // Game picker lives on the library card header.
    // Install sits on a separate card below.
    setAcceptDrops(true);
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(28, 22, 28, 22);
    root->setSpacing(14);

    auto *listCard = new QFrame(this);
    listCard->setObjectName("Card");
    auto *listLayout = new QVBoxLayout(listCard);
    listLayout->setContentsMargins(18, 15, 18, 16);
    listLayout->setSpacing(12);

    auto *head = new QHBoxLayout();
    head->setSpacing(8);
    auto *headCol = new QVBoxLayout();
    headCol->setSpacing(2);
    auto *listKicker = new QLabel(tr("BIBLIOTECA"), listCard);
    listKicker->setObjectName("modsListKicker");
    listKicker->setObjectName("CardKicker");
    auto *listTitle = new QLabel(tr("Mods instalados"), listCard);
    listTitle->setObjectName("modsListTitle");
    listTitle->setObjectName("CardTitle");
    headCol->addWidget(listKicker);
    headCol->addWidget(listTitle);
    head->addLayout(headCol, 1);

    m_gameCombo = new QComboBox(listCard);
    m_gameCombo->addItems({"World at War", "Black ops", "Black ops II", "Modern Warfare 3"});
    m_gameCombo->setCurrentText("Black ops II");
    m_gameCombo->setMinimumWidth(200);
    auto *refreshBtn = new QPushButton(tr("Atualizar"), listCard);
    refreshBtn->setObjectName("modsRefresh");
    refreshBtn->setProperty("cssClass", "ghost");
    auto *deselectBtn = new QPushButton(tr("Limpar selecao"), listCard);
    deselectBtn->setObjectName("modsDeselect");
    deselectBtn->setProperty("cssClass", "ghost");
    head->addWidget(m_gameCombo);
    head->addWidget(refreshBtn);
    head->addWidget(deselectBtn);
    listLayout->addLayout(head);

    m_titleLabel = new QLabel(tr("Selecione um jogo para ver os mods"), listCard);
    m_titleLabel->setObjectName("MutedHint");
    listLayout->addWidget(m_titleLabel);

    m_modList = new QListWidget(listCard);
    listLayout->addWidget(m_modList, 1);

    auto *listFoot = new QHBoxLayout();
    auto *listHint = new QLabel(tr("O mod selecionado e usado ao iniciar o jogo."), listCard);
    listHint->setObjectName("modsListHint");
    listHint->setObjectName("MutedHint");
    m_deleteBtn = new QPushButton(tr("Excluir mod"), listCard);
    auto *deleteBtn = m_deleteBtn;
    deleteBtn->setObjectName("modsDelete");
    deleteBtn->setProperty("cssClass", "danger");
    listFoot->addWidget(listHint);
    listFoot->addStretch();
    listFoot->addWidget(deleteBtn);
    listLayout->addLayout(listFoot);
    root->addWidget(listCard, 1);

    auto *installCard = new QFrame(this);
    installCard->setObjectName("Card");
    auto *installLayout = new QVBoxLayout(installCard);
    installLayout->setContentsMargins(18, 15, 18, 16);
    installLayout->setSpacing(10);
    auto *installKicker = new QLabel(tr("INSTALAR"), installCard);
    installKicker->setObjectName("modsInstallKicker");
    installKicker->setObjectName("CardKicker");
    auto *installTitle = new QLabel(tr("Adicionar mod ou mapa"), installCard);
    installTitle->setObjectName("modsInstallTitle");
    installTitle->setObjectName("CardTitle");
    auto *installDesc = new QLabel(
        tr("Zip, Rar, 7z, exe ou .cll — arraste um pack CLL aqui."), installCard);
    installDesc->setObjectName("modsInstallDesc");
    installLayout->addWidget(installKicker);
    installLayout->addWidget(installTitle);
    installLayout->addWidget(installDesc);
    auto *row = new QHBoxLayout();
    row->setSpacing(8);
    m_modPathEdit = new QLineEdit(installCard);
    m_modPathEdit->setPlaceholderText(tr("Caminho do arquivo do mod"));
    auto *browseBtn = new QPushButton(tr("Procurar"), installCard);
    browseBtn->setObjectName("modsBrowse");
    m_installBtn = new QPushButton(tr("Instalar mod"), installCard);
    m_installBtn->setProperty("cssClass", "primary");
    m_installBtn->setMinimumWidth(140);
    m_installBtn->setCursor(Qt::PointingHandCursor);
    row->addWidget(m_modPathEdit, 1);
    row->addWidget(browseBtn);
    row->addWidget(m_installBtn);
    installLayout->addLayout(row);
    m_progressLabel = new QLabel(installCard);
    m_progressLabel->setObjectName("MutedHint");
    m_progress = new QProgressBar(installCard);
    m_progress->setRange(0, 100);
    m_progress->setValue(0);
    m_progress->setVisible(false);
    m_progressLabel->setVisible(false);
    installLayout->addWidget(m_progressLabel);
    installLayout->addWidget(m_progress);
    root->addWidget(installCard);

    connect(m_gameCombo, &QComboBox::currentTextChanged, this, &ModsPage::onGameSelected);
    connect(refreshBtn, &QPushButton::clicked, this, [this]() { onRefreshOrDeselect(false); });
    m_modList->setCursor(Qt::PointingHandCursor);
    connect(deselectBtn, &QPushButton::clicked, this, [this]() { onRefreshOrDeselect(true); });
    connect(browseBtn, &QPushButton::clicked, this, &ModsPage::onBrowseModFile);
    connect(m_installBtn, &QPushButton::clicked, this, &ModsPage::onInstallMod);
    connect(deleteBtn, &QPushButton::clicked, this, &ModsPage::onDeleteMod);

    connect(&m_installWatcher, &QFutureWatcher<QString>::finished, this, [this]() {
        setBusy(false);
        m_progress->setVisible(false);
        m_progressLabel->setVisible(false);
        const QString err = m_installWatcher.result();
        const Job job = m_job;
        m_job = Job::None;
        if (err.isEmpty()) {
            refreshList();
            if (job == Job::Remove)
                QMessageBox::information(this, tr("Mods"), tr("Mod removido."));
            else
                QMessageBox::information(this, tr("Mods"), tr("Mod instalado com sucesso."));
        } else {
            Dialogs::error(this, err);
        }
    });

    connect(&m_sevenZipWatcher, &QFutureWatcher<int>::finished, this, [this]() {
        setBusy(false);
        m_progress->setVisible(false);
        m_progressLabel->setVisible(false);
        switch (m_sevenZipWatcher.result()) {
        case 0: Dialogs::info(this, Dialogs::Msg::SevenZipSuccess, m_settings); break;
        case 1: Dialogs::info(this, Dialogs::Msg::SevenZipFailDownload, m_settings); break;
        default: Dialogs::info(this, Dialogs::Msg::SevenZipFailInstall, m_settings); break;
        }
    });

    onGameSelected(m_gameCombo->currentText());
}

QString ModsPage::selectedMod() const
{
    auto *item = m_modList->currentItem();
    if (!item)
        return QString();
    const QString id = item->data(Qt::UserRole).toString();
    return id.isEmpty() ? item->text() : id;
}

void ModsPage::refreshList()
{
    if (m_settings.modId.isEmpty())
        return;
    const QString path = Storage::subdir(m_settings.plutoniumInstance, m_settings.modId, "mods");
    QStringList folders = Storage::listModFolders(path);
    const QString sid = Storage::gameStorageId(m_settings.modId);
    const QString pu = m_settings.plutoniumInstance;
    const QStringList ckpts = SmartModInstaller::checkpointIds(pu, sid);

    QStringList hidden;
    for (const QString &id : ckpts) {
        for (const QString &owned : SmartModInstaller::checkpointOwnedModFolders(pu, sid, id)) {
            if (owned.compare(id, Qt::CaseInsensitive) != 0)
                hidden << owned;
        }
    }

    QSet<QString> hiddenSet;
    for (const QString &h : hidden)
        hiddenSet.insert(h.toLower());

    struct Row { QString id; QString label; };
    QList<Row> rows;
    QSet<QString> seen;
    for (const QString &id : ckpts) {
        if (hiddenSet.contains(id.toLower()))
            continue;
        const QString label = SmartModInstaller::checkpointDisplayName(pu, sid, id);
        rows.append({id, label.isEmpty() ? id : label});
        seen.insert(id.toLower());
    }
    for (const QString &folder : folders) {
        bool hide = seen.contains(folder.toLower());
        for (const QString &h : hidden) {
            if (folder.compare(h, Qt::CaseInsensitive) == 0)
                hide = true;
        }
        if (hide)
            continue;
        rows.append({folder, folder});
        seen.insert(folder.toLower());
    }
    std::sort(rows.begin(), rows.end(), [](const Row &a, const Row &b) {
        return a.label.compare(b.label, Qt::CaseInsensitive) < 0;
    });
    m_modList->clear();
    for (const Row &r : rows) {
        auto *item = new QListWidgetItem(r.label);
        item->setData(Qt::UserRole, r.id);
        m_modList->addItem(item);
    }
}

void ModsPage::onGameSelected(const QString &gameName)
{
    if (gameName.isEmpty())
        return;
    m_settings.modId = gameName;
    m_titleLabel->setText(tr("Mods disponiveis para %1").arg(gameName));
    refreshList();
}

void ModsPage::selectGame(const QString &gameId)
{
    if (gameId.isEmpty())
        return;
    const int idx = m_gameCombo->findText(gameId);
    if (idx >= 0)
        m_gameCombo->setCurrentIndex(idx);
    onGameSelected(m_gameCombo->currentText());
}

void ModsPage::onRefreshOrDeselect(bool clearSelection)
{
    refreshList();
    if (clearSelection)
        m_modList->setCurrentRow(-1);
}

void ModsPage::onBrowseModFile()
{
    const QString file = QFileDialog::getOpenFileName(
        this, tr("Procurar"), QString(),
        tr("Mod files (*.zip *.rar *.7z *.exe *.cll)"));
    if (!file.isEmpty())
        handleModFile(file);
}

void ModsPage::setBusy(bool busy)
{
    m_installBtn->setDisabled(busy);
    if (m_deleteBtn)
        m_deleteBtn->setDisabled(busy);
    if (m_modList)
        m_modList->setDisabled(busy);
    if (busy && m_job == Job::Remove)
        m_installBtn->setText(tr("Instalar mod"));
    else
        m_installBtn->setText(busy ? tr("Instalando...") : tr("Instalar mod"));
    if (m_deleteBtn && busy && m_job == Job::Remove)
        m_deleteBtn->setText(tr("Removendo..."));
    else if (m_deleteBtn)
        m_deleteBtn->setText(tr("Excluir mod"));
}

void ModsPage::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasUrls())
        event->acceptProposedAction();
}

void ModsPage::dropEvent(QDropEvent *event)
{
    const auto urls = event->mimeData()->urls();
    if (urls.isEmpty())
        return;
    const QString path = urls.first().toLocalFile();
    if (!path.isEmpty())
        handleModFile(path);
}

void ModsPage::handleModFile(const QString &path)
{
    m_modPathEdit->setText(path);
    if (!CllInstaller::isSupportedArchive(path))
        return;
    if (CllInstaller::isCllPack(path) || CllInstaller::archiveMentionsInstaller(path))
        tryCllInstall(path);
}

bool ModsPage::tryCllInstall(const QString &path)
{
    if (!ArchiveTool::hasSevenZip()) {
        if (Dialogs::confirmDownload7z(this))
            runSevenZipBootstrap();
        return true;
    }
    const auto man = CllInstaller::peekArchive(path);
    if (!man.valid) {
        if (CllInstaller::isCllPack(path)) {
            Dialogs::error(this, man.error.isEmpty()
                                     ? tr("A .cll file must contain cll_installer.json.")
                                     : man.error);
            return true;
        }
        return false;
    }
    if (!man.gameId.isEmpty())
        selectGame(man.gameId);
    if (!CllInstaller::confirmAndShow(this, man))
        return true;

    const QString pu = m_settings.plutoniumInstance;
    const QString gameFolder = m_settings.gameFolder(man.gameId);
    m_job = Job::Install;
    setBusy(true);
    m_progress->setVisible(true);
    m_progressLabel->setVisible(true);
    m_progress->setRange(0, 100);
    m_progress->setValue(0);
    m_progressLabel->setText(tr("Extracting pack..."));
    auto future = QtConcurrent::run([=]() -> QString {
        const QString tempDir = QDir::temp().filePath("LanLauncher_cll_extract");
        QDir(tempDir).removeRecursively();
        QDir().mkpath(tempDir);
        QString extractError;
        const bool extracted = ArchiveTool::extractToDirectory(
            path, tempDir, &extractError,
            [this](int pct) {
                QMetaObject::invokeMethod(this, [this, pct]() {
                    m_progress->setRange(0, 100);
                    m_progress->setValue(int(pct * 0.45));
                    m_progressLabel->setText(tr("Extracting pack... %1%").arg(pct));
                }, Qt::QueuedConnection);
            });
        if (!extracted) {
            QDir(tempDir).removeRecursively();
            return extractError.isEmpty() ? QObject::tr("Failed to extract the pack.") : extractError;
        }
        QMetaObject::invokeMethod(this, [this]() {
            m_progress->setValue(45);
            m_progressLabel->setText(tr("Copying files..."));
        }, Qt::QueuedConnection);
        const QString err = CllInstaller::apply(
            man, tempDir, pu, gameFolder, path,
            [this](int pct, const QString &label) {
                QMetaObject::invokeMethod(this, [this, pct, label]() {
                    m_progress->setRange(0, 100);
                    m_progress->setValue(45 + int(pct * 0.55));
                    m_progressLabel->setText(label);
                }, Qt::QueuedConnection);
            });
        QDir(tempDir).removeRecursively();
        return err;
    });
    m_installWatcher.setFuture(future);
    return true;
}

void ModsPage::onInstallMod()
{
    // The visible combo is the source of truth — do not rely on a signal
    // that may not have fired on first open.
    const QString game = m_gameCombo->currentText();
    if (game.isEmpty()) {
        Dialogs::error(this, tr("Selecione um jogo na lista acima antes de instalar o mod."));
        return;
    }
    m_settings.modId = game;

    const QString archivePath = m_modPathEdit->text().trimmed();
    if (archivePath.isEmpty() || !QFileInfo::exists(archivePath)) {
        Dialogs::error(this, tr("Escolha o arquivo do mod (zip, rar, 7z, exe ou cll) em Procurar."));
        return;
    }
    if (!ArchiveTool::hasSevenZip()) {
        if (Dialogs::confirmDownload7z(this))
            runSevenZipBootstrap();
        return;
    }
    if (CllInstaller::isCllPack(archivePath) || CllInstaller::archiveMentionsInstaller(archivePath)) {
        tryCllInstall(archivePath);
        return;
    }
    installStandardOrSmart(archivePath);
}

void ModsPage::installStandardOrSmart(const QString &archivePath)
{
    const QString modsPath = Storage::subdir(m_settings.plutoniumInstance, m_settings.modId, "mods");
    const QString pu = m_settings.plutoniumInstance;
    const QString gameFolder = m_settings.gameFolder(m_settings.modId);

    if (pu.isEmpty()) {
        Dialogs::error(this, tr("Configure a pasta do Plutonium em Configuracoes antes de instalar mods."));
        return;
    }

    QMessageBox box(this);
    box.setWindowTitle(tr("Instalar mod"));
    box.setText(tr("Sera criado um checkpoint desta instalacao.\n\n"
                   "Se o pacote tiver storage/ e steam/:\n"
                   "• storage/ → pasta Plutonium\n"
                   "• steam/ → pasta do jogo\n\n"
                   "Ao excluir o mod, os arquivos novos saem e os que foram\n"
                   "substituidos voltam ao original. O backup do checkpoint\n"
                   "e apagado em seguida."));
    auto *okBtn = box.addButton(tr("Instalar"), QMessageBox::AcceptRole);
    box.addButton(tr("Cancelar"), QMessageBox::RejectRole);
    box.exec();
    if (box.clickedButton() != okBtn)
        return;

    m_job = Job::Install;
    setBusy(true);
    m_progress->setVisible(true);
    m_progressLabel->setVisible(true);
    m_progress->setRange(0, 0);
    m_progressLabel->setText(tr("Extraindo pacote..."));

    auto future = QtConcurrent::run([=]() -> QString {
        const QString tempDir = QDir::temp().filePath("LanLauncher_mod_extract");
        QDir(tempDir).removeRecursively();
        QDir().mkpath(tempDir);

        QString extractError;
        if (!ArchiveTool::extractToDirectory(archivePath, tempDir, &extractError)) {
            QDir(tempDir).removeRecursively();
            return extractError.isEmpty() ? QObject::tr("Falha ao extrair o arquivo.") : extractError;
        }

        auto plan = SmartModInstaller::analyzeExtractedRoot(tempDir);
        if (plan.isSmart) {
            QString warning;
            QString modHint = plan.suggestedModId;
            if (modHint.isEmpty())
                modHint = QFileInfo(archivePath).completeBaseName();
            const QString err = SmartModInstaller::applyPlan(
                plan, tempDir, pu, gameFolder, modHint, archivePath, &warning);
            QDir(tempDir).removeRecursively();
            return err;
        }

        QDir(tempDir).removeRecursively();
        if (modsPath.isEmpty())
            return QObject::tr("Selecione um jogo na lista de mods.");
        QDir().mkpath(modsPath);
        const auto r = ArchiveTool::extractArchive(modsPath, archivePath);
        if (r == ArchiveTool::ExtractResult::Success) {
            const QString gameSid = Storage::gameStorageId(m_settings.modId);
            QString folderName = QFileInfo(archivePath).completeBaseName();
            folderName.replace(' ', '_');
            QString dest = modsPath + "/" + folderName;
            if (!QDir(dest).exists()) {
                // extract may have created a folder with the original zip name
                dest = modsPath + "/" + QFileInfo(archivePath).completeBaseName();
            }
            if (QDir(dest).exists())
                SmartModInstaller::recordStandardInstall(pu, gameSid, dest, QFileInfo(dest).fileName(), archivePath);
            return QString();
        }
        switch (r) {
        case ArchiveTool::ExtractResult::NotStandardModFormat:
            return QObject::tr("Este pacote nao e um mod padrao nem um kit storage/steam.");
        case ArchiveTool::ExtractResult::UnsupportedExtension:
            return QObject::tr("Extensao nao suportada. Use zip, rar, 7z ou exe.");
        case ArchiveTool::ExtractResult::SevenZipMissing:
            return QObject::tr("7-Zip nao encontrado.");
        default:
            return QObject::tr("Falha ao instalar o mod no formato padrao.");
        }
    });
    m_installWatcher.setFuture(future);
}

void ModsPage::onDeleteMod()
{
    const QString path = Storage::subdir(m_settings.plutoniumInstance, m_settings.modId, "mods");
    const QString modToDelete = selectedMod();
    if (modToDelete.isEmpty()) {
        Dialogs::info(this, Dialogs::Msg::NoMod, m_settings);
        return;
    }

    const QString sid = Storage::gameStorageId(m_settings.modId);
    const bool hasCkpt = SmartModInstaller::hasCheckpoint(m_settings.plutoniumInstance, sid, modToDelete);
    QMessageBox box(this);
    box.setWindowTitle(tr("Excluir mod"));
    box.setText(hasCkpt
                    ? tr("Remover \"%1\" e restaurar os arquivos originais do checkpoint?\n"
                         "Os arquivos que este mod adicionou serao apagados.\n"
                         "Os que foram substituidos voltam ao estado anterior.\n"
                         "O backup do checkpoint sera excluido em seguida.")
                          .arg(modToDelete)
                    : tr("Remover a pasta do mod \"%1\"? Nao ha checkpoint para restaurar originais.")
                          .arg(modToDelete));
    auto *yes = box.addButton(tr("Excluir e restaurar"), QMessageBox::AcceptRole);
    if (!hasCkpt)
        yes->setText(tr("Excluir"));
    box.addButton(tr("Cancelar"), QMessageBox::RejectRole);
    box.exec();
    if (box.clickedButton() != yes)
        return;

    const QString pu = m_settings.plutoniumInstance;
    const QString gameFolder = m_settings.gameFolder(m_settings.modId);
    m_job = Job::Remove;
    setBusy(true);
    m_progress->setVisible(true);
    m_progressLabel->setVisible(true);
    m_progress->setRange(0, 0);
    m_progressLabel->setText(tr("Removing mod..."));
    auto future = QtConcurrent::run([=]() -> QString {
        const QString err = SmartModInstaller::rollback(pu, gameFolder, sid, modToDelete);
        if (!err.isEmpty())
            return err;
        if (!path.isEmpty())
            QDir(path + "/" + modToDelete).removeRecursively();
        return QString();
    });
    m_installWatcher.setFuture(future);
}

void ModsPage::runSevenZipBootstrap()
{
    setBusy(true);
    m_progress->setVisible(true);
    m_progressLabel->setVisible(true);
    m_progress->setRange(0, 100);
    m_progress->setValue(0);
    m_progressLabel->setText(tr("Baixando 7-Zip..."));

    auto future = QtConcurrent::run([this]() -> int {
        const QString zipPath = QDir::temp().filePath("LanLauncherQt_7z.zip");
        QString error;
        if (!Downloader::downloadToFile(
                "https://raw.githubusercontent.com/JugAndDoubleTap/LanLauncher/main/7z.zip",
                zipPath, error,
                [this](qint64 got, qint64 total) {
                    QMetaObject::invokeMethod(this, [this, got, total]() {
                        if (total > 0) {
                            m_progress->setRange(0, 100);
                            m_progress->setValue(int(got * 100 / total));
                        }
                        m_progressLabel->setText(tr("Baixando 7-Zip... %1 / %2 KB")
                                                     .arg(got / 1024).arg(total > 0 ? total / 1024 : 0));
                    }, Qt::QueuedConnection);
                })) {
            return 1;
        }
        QMetaObject::invokeMethod(this, [this]() {
            m_progress->setRange(0, 0);
            m_progressLabel->setText(tr("Extraindo 7-Zip..."));
        }, Qt::QueuedConnection);
        const bool ok = ArchiveTool::extractBootstrapZip(zipPath, QCoreApplication::applicationDirPath());
        QFile::remove(zipPath);
        return ok ? 0 : 2;
    });
    m_sevenZipWatcher.setFuture(future);
}

void ModsPage::retranslate()
{
    if (auto *w = findChild<QLabel*>("modsListKicker")) w->setText(tr("BIBLIOTECA"));
    if (auto *w = findChild<QLabel*>("modsListTitle")) w->setText(tr("Mods instalados"));
    if (auto *w = findChild<QPushButton*>("modsRefresh")) w->setText(tr("Atualizar"));
    if (auto *w = findChild<QPushButton*>("modsDeselect")) w->setText(tr("Limpar selecao"));
    if (auto *w = findChild<QLabel*>("modsListHint")) w->setText(tr("O mod selecionado e usado ao iniciar o jogo."));
    if (auto *w = findChild<QPushButton*>("modsDelete")) w->setText(tr("Excluir mod"));
    if (auto *w = findChild<QLabel*>("modsInstallKicker")) w->setText(tr("INSTALAR"));
    if (auto *w = findChild<QLabel*>("modsInstallTitle")) w->setText(tr("Adicionar mod ou mapa"));
    if (auto *w = findChild<QLabel*>("modsInstallDesc"))
        w->setText(tr("Zip, Rar, 7z, exe ou .cll — arraste um pack CLL aqui."));
    if (auto *w = findChild<QPushButton*>("modsBrowse")) w->setText(tr("Procurar"));
    if (m_installBtn) m_installBtn->setText(tr("Instalar mod"));
    if (m_modPathEdit) m_modPathEdit->setPlaceholderText(tr("Caminho do arquivo do mod"));
    if (m_gameCombo && !m_gameCombo->currentText().isEmpty())
        m_titleLabel->setText(tr("Mods disponiveis para %1").arg(m_gameCombo->currentText()));
    else if (m_titleLabel)
        m_titleLabel->setText(tr("Selecione um jogo para ver os mods"));
    if (m_deleteBtn)
        m_deleteBtn->setText(tr("Excluir mod"));
}
