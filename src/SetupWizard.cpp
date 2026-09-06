#include "SetupWizard.h"
#include "AnimatedLogo.h"
#include "AppSettings.h"
#include "ArchiveTool.h"
#include "Downloader.h"
#include "GameCatalog.h"
#include "SteamDetector.h"
#include "Theme.h"
#include "I18n.h"
#include "Checkables.h"

#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFrame>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QLineEdit>
#include <QMap>
#include <QMetaObject>
#include <QProgressBar>
#include <QPushButton>
#include <QRadioButton>
#include <QStandardPaths>
#include <QStackedWidget>
#include <QTimer>
#include <QVBoxLayout>
#include <QtConcurrent/QtConcurrent>

namespace {
const char kPuDatUrl[] = "https://github.com/MestreTM/CLL-CodLanLauncher/releases/download/v0.1/pu.dat";
}

SetupWizard::SetupWizard(AppSettings &settings, QWidget *parent)
    : QDialog(parent)
    , m_settings(settings)
{
    setWindowTitle(tr("Cod Lan Launcher — configuracao"));
    setWindowIcon(QIcon(":/icons/app.svg"));
    setModal(true);
    resize(720, 560);
    setObjectName("SetupWizard");
    buildUi();
    onRescan();
    connect(&m_kitWatcher, &QFutureWatcher<QString>::finished, this, &SetupWizard::onKitFinished);
    refreshInstalledDetect();
    connect(&I18nHub::instance(), &I18nHub::languageChanged, this, &SetupWizard::retranslate);
}

void SetupWizard::buildUi()
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    auto *header = new QFrame(this);
    header->setObjectName("WizardHeader");
    auto *headerLay = new QHBoxLayout(header);
    headerLay->setContentsMargins(24, 18, 24, 18);
    auto *logo = new AnimatedLogo(header);
    logo->setLogoSize(56);
    auto *col = new QVBoxLayout();
    m_headerTitle = new QLabel(tr("Bem-vindo ao Cod Lan Launcher"), header);
    m_headerTitle->setObjectName("WizardTitle");
    m_headerSub = new QLabel(tr("setup inicial"), header);
    m_headerSub->setObjectName("WizardSub");
    m_stepLabel = new QLabel(tr("Passo 1 de 4"), header);
    m_stepLabel->setObjectName("WizardSub");
    col->addWidget(m_headerTitle);
    col->addWidget(m_headerSub);
    col->addWidget(m_stepLabel);
    headerLay->addWidget(logo);
    headerLay->addSpacing(14);
    headerLay->addLayout(col, 1);

    m_dots = new QWidget(header);
    auto *dotsLay = new QHBoxLayout(m_dots);
    dotsLay->setContentsMargins(0, 0, 0, 0);
    dotsLay->setSpacing(8);
    for (int i = 0; i < 4; ++i) {
        auto *d = new QLabel(m_dots);
        d->setFixedSize(10, 10);
        d->setObjectName("StepDot");
        m_dotLabels << d;
        dotsLay->addWidget(d);
    }
    headerLay->addWidget(m_dots, 0, Qt::AlignTop);
    root->addWidget(header);

    m_stack = new QStackedWidget(this);
    m_stack->addWidget(buildWelcomePage());
    m_stack->addWidget(buildNickPage());
    m_stack->addWidget(buildGamesPage());
    m_stack->addWidget(buildDonePage());
    root->addWidget(m_stack, 1);

    auto *footer = new QWidget(this);
    footer->setObjectName("WizardFooter");
    auto *footerLay = new QHBoxLayout(footer);
    footerLay->setContentsMargins(24, 14, 24, 18);
    m_skipBtn = new QPushButton(tr("Pular"), footer);
    m_backBtn = new QPushButton(tr("Voltar"), footer);
    m_nextBtn = new QPushButton(tr("Continuar"), footer);
    m_nextBtn->setProperty("cssClass", "primary");
    m_nextBtn->setMinimumWidth(140);
    footerLay->addWidget(m_skipBtn);
    footerLay->addStretch();
    footerLay->addWidget(m_backBtn);
    footerLay->addWidget(m_nextBtn);
    root->addWidget(footer);

    connect(m_nextBtn, &QPushButton::clicked, this, &SetupWizard::onNext);
    connect(m_backBtn, &QPushButton::clicked, this, &SetupWizard::onBack);
    connect(m_skipBtn, &QPushButton::clicked, this, &SetupWizard::onSkip);
    updateNavButtons();
}

void SetupWizard::setStepDots(int index)
{
    const QString on = QStringLiteral("background:%1; border-radius:5px;").arg(Theme::accent());
    const QString off = QStringLiteral("background:#2e3149; border-radius:5px;");
    for (int i = 0; i < m_dotLabels.size(); ++i) {
        m_dotLabels[i]->setProperty("active", i <= index);
        m_dotLabels[i]->setStyleSheet(i <= index ? on : off);
    }
}

QWidget *SetupWizard::buildWelcomePage()
{
    auto *page = new QWidget(this);
    auto *lay = new QVBoxLayout(page);
    lay->setContentsMargins(40, 20, 40, 16);
    lay->setSpacing(10);

    m_langLabel = new QLabel(tr("Idioma"), page);
    m_langLabel->setObjectName("WizardLead");
    lay->addWidget(m_langLabel);
    m_langHint = new QLabel(tr("Escolha o idioma do programa."), page);
    m_langHint->setObjectName("WizardCardDesc");
    m_langHint->setWordWrap(true);
    lay->addWidget(m_langHint);
    m_langCombo = new QComboBox(page);
    m_langCombo->setMinimumHeight(36);
    for (const QString &code : I18n::codes())
        m_langCombo->addItem(I18n::displayName(code), code);
    int li = m_langCombo->findData(m_settings.language.isEmpty() ? I18n::detectSystem() : m_settings.language);
    m_langCombo->setCurrentIndex(li >= 0 ? li : 0);
    lay->addWidget(m_langCombo);
    connect(m_langCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &SetupWizard::onLanguageChanged);
    lay->addSpacing(8);

    m_welcomeLead = new QLabel(tr("Client Plutonium"), page);
    m_welcomeLead->setObjectName("WizardLead");
    lay->addWidget(m_welcomeLead);
    m_welcomeDesc = new QLabel(
        tr("Baixe o kit portatil (pasta pu ao lado do programa) ou use a instalacao oficial."),
        page);
    m_welcomeDesc->setWordWrap(true);
    m_welcomeDesc->setObjectName("WizardCardDesc");
    lay->addWidget(m_welcomeDesc);

    m_radioPortable = new Ui::RadioButton(tr("Baixar Plutonium Portable"), page);
    m_radioInstalled = new Ui::RadioButton(tr("Usar o Plutonium ja instalado neste PC"), page);
    lay->addWidget(m_radioPortable);
    lay->addWidget(m_radioInstalled);

    m_installedBox = new QFrame(page);
    auto *inst = m_installedBox;
    inst->setObjectName("WizardCard");
    auto *il = new QVBoxLayout(inst);
    auto *row = new QHBoxLayout();
    m_installedPath = new QLineEdit(inst);
    m_installedPath->setPlaceholderText(tr("%LOCALAPPDATA%\\Plutonium"));
    m_browseInstalled = new QPushButton(tr("Procurar"), inst);
    row->addWidget(m_installedPath, 1);
    row->addWidget(m_browseInstalled);
    il->addLayout(row);
    m_installedStatus = new QLabel(inst);
    m_installedStatus->setWordWrap(true);
    m_installedStatus->setObjectName("WizardCardDesc");
    il->addWidget(m_installedStatus);
    lay->addWidget(inst);

    auto *box = new QFrame(page);
    box->setObjectName("WizardCard");
    auto *bl = new QVBoxLayout(box);
    m_kitStatus = new QLabel(tr("O download so comeca se voce escolher o kit portatil."), box);
    m_kitStatus->setWordWrap(true);
    m_kitStatus->setObjectName("WizardCardDesc");
    m_kitProgress = new QProgressBar(box);
    m_kitProgress->setRange(0, 100);
    m_kitProgress->setValue(0);
    m_kitProgress->setVisible(false);
    bl->addWidget(m_kitStatus);
    bl->addWidget(m_kitProgress);
    lay->addWidget(box);
    lay->addStretch();

    connect(m_browseInstalled, &QPushButton::clicked, this, [this]() {
        const QString dir = QFileDialog::getExistingDirectory(
            this, tr("Pasta do Plutonium"), m_installedPath->text());
        if (!dir.isEmpty()) {
            m_installedPath->setText(dir);
            m_radioInstalled->setChecked(true);
            refreshInstalledDetect();
        }
    });
    connect(m_installedPath, &QLineEdit::textChanged, this, [this]() { refreshInstalledDetect(); });
    connect(m_radioPortable, &QRadioButton::toggled, this, [this](bool on) {
        if (on) {
            m_kitStatus->setText(tr("Ao continuar, o kit sera baixado para ./pu."));
            m_kitProgress->setVisible(false);
        }
        updateNavButtons();
    });
    connect(m_radioInstalled, &QRadioButton::toggled, this, [this](bool on) {
        if (m_installedBox)
            m_installedBox->setVisible(on);
        if (on)
            refreshInstalledDetect();
        updateNavButtons();
    });
    if (m_installedBox)
        m_installedBox->setVisible(m_radioInstalled && m_radioInstalled->isChecked());
    return page;
}

bool SetupWizard::usingPortableKit() const
{
    return m_radioPortable && m_radioPortable->isChecked();
}

void SetupWizard::refreshInstalledDetect()
{
    if (!m_installedPath)
        return;
    QString path = m_installedPath->text().trimmed();
    if (path.isEmpty()) {
        path = AppSettings::officialPlutoniumDir();
        m_installedPath->setText(path);
    }
    const bool ok = AppSettings::isPlutoniumRoot(path);
    m_installedStatus->setText(
        (ok ? tr("Instalacao valida. ") : tr("Nao e uma pasta Plutonium valida. "))
        + AppSettings::plutoniumRootSummary(path));
    if (!m_radioPortable->isChecked() && !m_radioInstalled->isChecked()) {
        if (ok)
            m_radioInstalled->setChecked(true);
        else
            m_radioPortable->setChecked(true);
    }
}

void SetupWizard::startKitInstall()
{
    if (m_kitBusy)
        return;
    if (QFileInfo::exists(AppSettings::localPuBootstrapper())) {
        m_kitOk = true;
        m_settings.plutoniumInstance = AppSettings::localPuDir();
        m_kitStatus->setText(tr("Kit ja esta em %1").arg(AppSettings::localPuDir()));
        m_kitProgress->setVisible(true);
        m_kitProgress->setRange(0, 100);
        m_kitProgress->setValue(100);
        updateNavButtons();
        return;
    }

    m_kitBusy = true;
    m_kitOk = false;
    m_kitProgress->setVisible(true);
    m_kitProgress->setRange(0, 0);
    m_kitStatus->setText(tr("Baixando pu.dat..."));
    updateNavButtons();

    auto future = QtConcurrent::run([this]() -> QString {
        const QString puDir = AppSettings::localPuDir();
        const QString appDir = QCoreApplication::applicationDirPath();

        auto progress = [this](qint64 got, qint64 total, const QString &label) {
            QMetaObject::invokeMethod(this, [this, got, total, label]() {
                if (total > 0) {
                    m_kitProgress->setRange(0, 100);
                    m_kitProgress->setValue(int(got * 100 / total));
                } else {
                    m_kitProgress->setRange(0, 0);
                }
                m_kitStatus->setText(label);
            }, Qt::QueuedConnection);
        };

        if (!ArchiveTool::hasSevenZip()) {
            progress(0, 0, QObject::tr("Baixando 7-Zip (necessario para extrair)..."));
            const QString zipPath = QDir::temp().filePath("LanLauncherQt_7z.zip");
            QString err;
            if (!Downloader::downloadToFile(
                    QStringLiteral("https://raw.githubusercontent.com/JugAndDoubleTap/LanLauncher/main/7z.zip"),
                    zipPath, err,
                    [this, progress](qint64 g, qint64 t) {
                        progress(g, t, QObject::tr("Baixando 7-Zip... %1 / %2 KB")
                                          .arg(g / 1024).arg(t > 0 ? t / 1024 : 0));
                    })) {
                return QObject::tr("Falha ao baixar o 7-Zip: %1").arg(err);
            }
            if (!ArchiveTool::extractBootstrapZip(zipPath, appDir)) {
                QFile::remove(zipPath);
                return QObject::tr("Falha ao instalar o 7-Zip.");
            }
            QFile::remove(zipPath);
        }

        const QString datPath = QDir::temp().filePath("LanLauncher_pu.dat");
        QString err;
        progress(0, 0, QObject::tr("Baixando kit pu.dat..."));
        if (!Downloader::downloadToFile(
                QString::fromUtf8(kPuDatUrl), datPath, err,
                [this, progress](qint64 g, qint64 t) {
                    progress(g, t, QObject::tr("Baixando pu.dat... %1 / %2 MB")
                                      .arg(g / (1024 * 1024))
                                      .arg(t > 0 ? t / (1024 * 1024) : 0));
                },
                30 * 60 * 1000)) {
            QFile::remove(datPath);
            return QObject::tr("Nao foi possivel baixar o kit.\n%1\n\n"
                               "Use o Plutonium oficial em:\n%2")
                .arg(err, AppSettings::officialPlutoniumDir());
        }

        progress(0, 0, QObject::tr("Extraindo kit para pu/..."));
        QString extractErr;
        QDir().mkpath(puDir);
        if (!ArchiveTool::extractObfuscatedBundle(datPath, puDir, &extractErr)) {
            QFile::remove(datPath);
            return extractErr.isEmpty()
                       ? QObject::tr("Falha ao extrair pu.dat")
                       : extractErr;
        }
        QFile::remove(datPath);

        if (!QFileInfo::exists(AppSettings::localPuBootstrapper())) {
            return QObject::tr("O kit foi extraido, mas o bootstrapper nao apareceu em pu/bin.");
        }
        return QString();
    });
    m_kitWatcher.setFuture(future);
}

void SetupWizard::applyPlutoniumFallback()
{
    m_settings.plutoniumInstance = AppSettings::officialPlutoniumDir();
}

void SetupWizard::onKitFinished()
{
    m_kitBusy = false;
    const QString err = m_kitWatcher.result();
    m_kitProgress->setRange(0, 100);
    if (err.isEmpty()) {
        m_kitOk = true;
        m_settings.plutoniumInstance = AppSettings::localPuDir();
        m_kitProgress->setValue(100);
        m_kitStatus->setText(tr("Kit instalado em %1").arg(AppSettings::localPuDir()));
        m_stack->setCurrentIndex(1);
        updateNavButtons();
        return;
    } else {
        m_kitOk = false;
        m_kitProgress->setValue(0);
        applyPlutoniumFallback();
        m_kitStatus->setText(
            tr("%1\n\nO launcher vai usar:\n%2\n\nBaixe o Plutonium se essa pasta ainda nao existir.")
                .arg(err, m_settings.plutoniumInstance));
    }
    updateNavButtons();
}

QWidget *SetupWizard::buildNickPage()
{
    auto *page = new QWidget(this);
    auto *lay = new QVBoxLayout(page);
    lay->setContentsMargins(40, 32, 40, 20);
    m_nickLead = new QLabel(tr("Defina seu Nickname"), page);
    m_nickLead->setObjectName("WizardLead");
    lay->addWidget(m_nickLead);
    m_nickDesc = nullptr;
    m_nickEdit = new QLineEdit(m_settings.username, page);
    m_nickEdit->setPlaceholderText(tr("Ex.: MestreTM"));
    m_nickEdit->setMinimumHeight(42);
    lay->addWidget(m_nickEdit);
    lay->addStretch();
    return page;
}

QWidget *SetupWizard::buildGamesPage()
{
    auto *page = new QWidget(this);
    auto *lay = new QVBoxLayout(page);
    lay->setContentsMargins(40, 20, 40, 12);
    m_gamesLead = new QLabel(tr("Seus jogos"), page);
    m_gamesLead->setObjectName("WizardLead");
    lay->addWidget(m_gamesLead);
    m_detectStatus = new QLabel(page);
    m_detectStatus->setWordWrap(true);
    m_detectStatus->setObjectName("WizardCardDesc");
    lay->addWidget(m_detectStatus);
    m_rescanBtn = new QPushButton(tr("Detectar novamente"), page);
    connect(m_rescanBtn, &QPushButton::clicked, this, &SetupWizard::onRescan);
    lay->addWidget(m_rescanBtn, 0, Qt::AlignLeft);

    for (const auto &g : GameCatalog::all()) {
        auto *row = new QFrame(page);
        row->setObjectName("WizardCard");
        auto *rl = new QVBoxLayout(row);
        auto *top = new QHBoxLayout();
        auto *cb = new Ui::CheckBox(g.shortLabel + "  ·  " + g.title, row);
        auto *browse = new QPushButton(tr("Procurar"), row);
        browse->setObjectName("wizGameBrowse");
        top->addWidget(cb, 1);
        top->addWidget(browse);
        rl->addLayout(top);
        auto *edit = new QLineEdit(row);
        edit->setPlaceholderText(tr("Pasta de instalacao"));
        rl->addWidget(edit);
        lay->addWidget(row);
        m_gameRows.push_back({g.id, edit, cb});
        connect(browse, &QPushButton::clicked, this, [this, id = g.id]() { onBrowseGame(id); });
        connect(cb, &QCheckBox::toggled, edit, &QLineEdit::setEnabled);
    }
    lay->addStretch();
    return page;
}

QWidget *SetupWizard::buildDonePage()
{
    auto *page = new QWidget(this);
    auto *lay = new QVBoxLayout(page);
    lay->setContentsMargins(40, 32, 40, 20);
    m_doneLead = new QLabel(tr("Pronto para jogar"), page);
    m_doneLead->setObjectName("WizardLead");
    lay->addWidget(m_doneLead);
    m_doneDesc = new QLabel(
        tr("Na barra da esquerda escolha o jogo.\n"
           "Na aba Mods voce instala pacotes e mapas customizados.\n"
           "Na aba Servidor LAN (beta) voce hospeda uma partida na rede local e edita as configs."),
        page);
    m_doneDesc->setWordWrap(true);
    m_doneDesc->setObjectName("WizardCardDesc");
    lay->addWidget(m_doneDesc);
    lay->addStretch();
    return page;
}

void SetupWizard::updateNavButtons()
{
    const int i = m_stack->currentIndex();
    const int n = m_stack->count();
    m_backBtn->setEnabled(i > 0);
    m_skipBtn->setVisible(i < n - 1);
    if (i == 0 && m_kitBusy) {
        m_nextBtn->setEnabled(false);
        m_nextBtn->setText(tr("Aguarde..."));
        m_skipBtn->setEnabled(false);
    } else {
        m_nextBtn->setEnabled(true);
        m_skipBtn->setEnabled(true);
        if (i == n - 1)
            m_nextBtn->setText(tr("Concluir"));
        else if (i == 0 && usingPortableKit() && !m_kitOk)
            m_nextBtn->setText(tr("Baixar Plutonium Portable"));
        else
            m_nextBtn->setText(tr("Continuar"));
    }
    const int step = qBound(1, i + 1, 4);
    m_stepLabel->setText(tr("Passo %1 de 4").arg(step));
    setStepDots(i);
}

void SetupWizard::onNext()
{
    const int i = m_stack->currentIndex();
    if (i == 0 && m_kitBusy)
        return;
    if (i == 0) {
        if (usingPortableKit()) {
            if (!m_kitOk) {
                startKitInstall();
                return;
            }
            m_settings.plutoniumInstance = AppSettings::localPuDir();
        } else {
            const QString path = m_installedPath ? m_installedPath->text().trimmed()
                                                 : AppSettings::officialPlutoniumDir();
            if (!AppSettings::isPlutoniumRoot(path)) {
                refreshInstalledDetect();
                m_kitStatus->setText(tr("Essa pasta nao tem plutonium-bootstrapper-win32.exe. "
                                        "Escolha a pasta correta ou baixe o kit portatil."));
                return;
            }
            m_settings.plutoniumInstance = AppSettings::resolvePath(path);
        }
    } else if (i == 1)
        m_settings.username = m_nickEdit->text().trimmed();
    else if (i == 2)
        applyDetectedGames();
    if (i >= m_stack->count() - 1) {
        finish(true);
        return;
    }
    m_stack->setCurrentIndex(i + 1);
    updateNavButtons();
}

void SetupWizard::onBack()
{
    if (m_stack->currentIndex() > 0)
        m_stack->setCurrentIndex(m_stack->currentIndex() - 1);
    updateNavButtons();
}

void SetupWizard::onSkip()
{
    if (m_kitBusy)
        return;
    if (usingPortableKit() && m_kitOk)
        m_settings.plutoniumInstance = AppSettings::localPuDir();
    else if (m_installedPath && AppSettings::isPlutoniumRoot(m_installedPath->text().trimmed()))
        m_settings.plutoniumInstance = AppSettings::resolvePath(m_installedPath->text().trimmed());
    else
        applyPlutoniumFallback();
    finish(true);
}

void SetupWizard::finish(bool markDone)
{
    if (m_nickEdit)
        m_settings.username = m_nickEdit->text().trimmed();
    applyDetectedGames();
    if (usingPortableKit() && m_kitOk)
        m_settings.plutoniumInstance = AppSettings::localPuDir();
    else if (m_installedPath && AppSettings::isPlutoniumRoot(m_installedPath->text().trimmed()))
        m_settings.plutoniumInstance = AppSettings::resolvePath(m_installedPath->text().trimmed());
    else if (m_settings.plutoniumInstance.isEmpty())
        applyPlutoniumFallback();
    if (markDone)
        m_settings.setupCompleted = true;
    m_settings.saveToIni();
    m_completed = true;
    accept();
}

void SetupWizard::onBrowseGame(const QString &gameId)
{
    for (GameRow &row : m_gameRows) {
        if (row.id != gameId)
            continue;
        const QString dir = QFileDialog::getExistingDirectory(this, tr("Pasta do jogo"), row.edit->text());
        if (!dir.isEmpty()) {
            row.edit->setText(dir);
            row.enabled->setChecked(true);
        }
        break;
    }
}

void SetupWizard::onRescan()
{
    const auto hits = SteamDetector::detectInstalledGames();
    QMap<QString, QString> map;
    for (const auto &h : hits)
        map.insert(h.gameId, h.path);
    int found = 0;
    for (GameRow &row : m_gameRows) {
        if (map.contains(row.id)) {
            row.edit->setText(map.value(row.id));
            row.enabled->setChecked(true);
            ++found;
        } else if (row.edit->text().isEmpty()) {
            row.enabled->setChecked(false);
        }
    }
    const QString steam = SteamDetector::steamInstallPath();
    if (steam.isEmpty())
        m_detectStatus->setText(tr("Steam nao encontrado. Use Procurar nas pastas."));
    else
        m_detectStatus->setText(tr("Steam: %1  ·  %2 jogo(s) encontrado(s).").arg(steam).arg(found));
}

void SetupWizard::applyDetectedGames()
{
    for (const GameRow &row : m_gameRows) {
        if (!row.enabled->isChecked())
            continue;
        const QString path = row.edit->text().trimmed();
        if (path.isEmpty())
            continue;
        if (row.id == "World at War") m_settings.waw = path;
        else if (row.id == "Black ops") m_settings.bo1 = path;
        else if (row.id == "Black ops II") m_settings.bo2 = path;
        else if (row.id == "Modern Warfare 3") m_settings.mw3 = path;
    }
}

void SetupWizard::onLanguageChanged()
{
    if (!m_langCombo)
        return;
    const QString code = m_langCombo->currentData().toString();
    if (code.isEmpty())
        return;
    m_settings.language = code;
    I18n::apply(code);
    retranslate();
}

void SetupWizard::retranslate()
{
    setWindowTitle(tr("Cod Lan Launcher — configuracao"));
    if (m_headerTitle)
        m_headerTitle->setText(tr("Bem-vindo ao Cod Lan Launcher"));
    if (m_headerSub)
        m_headerSub->setText(tr("setup inicial"));
    if (m_langLabel)
        m_langLabel->setText(tr("Idioma"));
    if (m_langHint)
        m_langHint->setText(tr("Escolha o idioma do programa."));
    if (m_welcomeLead)
        m_welcomeLead->setText(tr("Client Plutonium"));
    if (m_welcomeDesc)
        m_welcomeDesc->setText(tr("Baixe o kit portatil (pasta pu ao lado do programa) ou use a instalacao oficial."));
    if (m_radioPortable)
        m_radioPortable->setText(tr("Baixar Plutonium Portable"));
    if (m_radioInstalled)
        m_radioInstalled->setText(tr("Usar o Plutonium ja instalado neste PC"));
    if (m_browseInstalled)
        m_browseInstalled->setText(tr("Procurar"));
    if (m_installedPath)
        m_installedPath->setPlaceholderText(tr("%LOCALAPPDATA%\\Plutonium"));
    if (m_kitStatus) {
        if (usingPortableKit())
            m_kitStatus->setText(tr("Ao continuar, o kit sera baixado para ./pu."));
        else
            m_kitStatus->setText(tr("O download so comeca se voce escolher o kit portatil."));
    }
    refreshInstalledDetect();
    if (m_nickLead)
        m_nickLead->setText(tr("Defina seu Nickname"));
    if (m_nickEdit)
        m_nickEdit->setPlaceholderText(tr("Ex.: MestreTM"));
    if (m_gamesLead)
        m_gamesLead->setText(tr("Seus jogos"));
    if (m_rescanBtn)
        m_rescanBtn->setText(tr("Detectar novamente"));
    for (auto *b : findChildren<QPushButton*>("wizGameBrowse"))
        b->setText(tr("Procurar"));
    for (GameRow &row : m_gameRows) {
        if (row.edit)
            row.edit->setPlaceholderText(tr("Pasta de instalacao"));
    }
    if (m_detectStatus && !m_detectStatus->text().isEmpty())
        onRescan();
    if (m_doneLead)
        m_doneLead->setText(tr("Pronto para jogar"));
    if (m_doneDesc)
        m_doneDesc->setText(tr("Na barra da esquerda escolha o jogo.\n"
                               "Na aba Mods voce instala pacotes e mapas customizados.\n"
                               "Na aba Servidor LAN (beta) voce hospeda uma partida na rede local e edita as configs."));
    if (m_skipBtn)
        m_skipBtn->setText(tr("Pular"));
    if (m_backBtn)
        m_backBtn->setText(tr("Voltar"));
    if (m_langCombo) {
        const QString cur = m_langCombo->currentData().toString();
        m_langCombo->blockSignals(true);
        m_langCombo->clear();
        for (const QString &code : I18n::codes())
            m_langCombo->addItem(I18n::displayName(code), code);
        const int i = m_langCombo->findData(cur);
        m_langCombo->setCurrentIndex(i >= 0 ? i : 0);
        m_langCombo->blockSignals(false);
    }
    updateNavButtons();
}
