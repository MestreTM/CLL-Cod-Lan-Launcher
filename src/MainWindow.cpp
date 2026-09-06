#include "MainWindow.h"
#include "AnimatedLogo.h"
#include "Dialogs.h"
#include "GameCatalog.h"
#include "GameLauncher.h"
#include "SetupWizard.h"
#include "Theme.h"
#include "I18n.h"

#include "pages/PlayPage.h"
#include "pages/ModsPage.h"
#include "pages/ServerPage.h"
#include "pages/SettingsPage.h"
#include "pages/AboutPage.h"

#include <QApplication>
#include <QDesktopServices>
#include <QIcon>
#include <QLabel>
#include <QPushButton>
#include <QStackedWidget>
#include <QUrl>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFrame>
#include <QtConcurrent/QtConcurrent>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    m_settings = AppSettings::loadForStartup();
    I18n::apply(m_settings.language);
    Theme::apply();

    if (!m_settings.setupCompleted) {
        SetupWizard wizard(m_settings, nullptr);
        wizard.exec();
        Theme::apply();
    }

    setWindowTitle(tr("Cod Lan Launcher"));
    setWindowIcon(QIcon(":/icons/app.svg"));
    resize(1180, 720);
    setMinimumSize(960, 600);

    auto *central = new QWidget(this);
    auto *outer = new QHBoxLayout(central);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);
    outer->addWidget(buildSidebar());

    m_stack = new QStackedWidget(this);
    m_playPage = new PlayPage(m_settings, this);
    m_modsPage = new ModsPage(m_settings, this);
    m_serverPage = new ServerPage(m_settings, this);
    m_settingsPage = new SettingsPage(m_settings, this);
    m_aboutPage = new AboutPage(this);
    m_stack->addWidget(m_playPage);
    m_stack->addWidget(m_modsPage);
    m_stack->addWidget(m_serverPage);
    m_stack->addWidget(m_settingsPage);
    m_stack->addWidget(m_aboutPage);

    // Content column: page header plus stack. The header hides
    // on the play page, which is edge-to-edge art.
    auto *right = new QWidget(this);
    auto *rl = new QVBoxLayout(right);
    rl->setContentsMargins(0, 0, 0, 0);
    rl->setSpacing(0);
    rl->addWidget(buildHeader());
    rl->addWidget(m_stack, 1);

    outer->addWidget(right, 1);
    setCentralWidget(central);

    connect(m_playPage, &PlayPage::launchRequested, this, &MainWindow::onLaunchGame);
    connect(m_playPage, &PlayPage::stopRequested, this, &MainWindow::onStopGame);
    connect(m_settingsPage, &SettingsPage::plutoniumFolderChanged, this, [this]() {
        m_modsPage->refreshList();
        m_serverPage->refreshList();
    });
    connect(m_serverPage, &ServerPage::launchServerRequested, this, &MainWindow::onLaunchServer);
    connect(m_serverPage, &ServerPage::stopServerRequested, this, &MainWindow::onStopServer);
    connect(&m_processPollTimer, &QTimer::timeout, this, &MainWindow::onPollRunningProcess);
    m_processPollTimer.setInterval(2000);

    selectGame(2);
    connect(&I18nHub::instance(), &I18nHub::languageChanged, this, &MainWindow::retranslate);
}

void MainWindow::retranslate()
{
    setWindowTitle(tr("Cod Lan Launcher"));
    if (m_sidebarBy)
        m_sidebarBy->setText(tr("por MestreTM"));
    if (m_sidebarGames)
        m_sidebarGames->setText(tr("JOGOS"));
    if (m_sidebarTools)
        m_sidebarTools->setText(tr("FERRAMENTAS"));
    if (m_saveBtn)
        m_saveBtn->setText(tr("Salvar"));
    const QStringList tools = {
        tr("Mods"), tr("Servidor LAN (beta)"), tr("Configuracoes"), tr("Sobre")
    };
    for (int i = 0; i < m_toolButtons.size() && i < tools.size(); ++i)
        m_toolButtons[i]->setText("  " + tools[i]);
    applyHeader(m_toolIndex);
    if (m_playPage) m_playPage->retranslate();
    if (m_modsPage) m_modsPage->retranslate();
    if (m_serverPage) m_serverPage->retranslate();
    if (m_settingsPage) m_settingsPage->retranslate();
    if (m_aboutPage) m_aboutPage->retranslate();
}

QWidget *MainWindow::buildSidebar()
{
    auto *sidebar = new QWidget(this);
    sidebar->setObjectName("Sidebar");
    sidebar->setFixedWidth(252);
    auto *layout = new QVBoxLayout(sidebar);
    layout->setContentsMargins(12, 16, 12, 12);
    layout->setSpacing(3);

    auto *brand = new QHBoxLayout();
    brand->setSpacing(10);
    auto *logo = new AnimatedLogo(sidebar);
    logo->setLogoSize(38);
    auto *titles = new QVBoxLayout();
    titles->setSpacing(0);
    auto *name = new QLabel(tr("Cod Lan Launcher"), sidebar);
    name->setObjectName("SidebarTitle");
    m_sidebarBy = new QLabel(tr("por MestreTM"), sidebar);
    m_sidebarBy->setObjectName("SidebarSubtitle");
    titles->addWidget(name);
    titles->addWidget(m_sidebarBy);
    brand->addWidget(logo);
    brand->addLayout(titles, 1);
    layout->addLayout(brand);
    layout->addSpacing(12);

    m_sidebarGames = new QLabel(tr("JOGOS"), sidebar);
    m_sidebarGames->setObjectName("NavSection");
    layout->addWidget(m_sidebarGames);

    const auto games = GameCatalog::all();
    for (int i = 0; i < games.size(); ++i) {
        const auto &g = games[i];
        auto *btn = new QPushButton(sidebar);
        btn->setObjectName("GameNav");
        btn->setCheckable(true);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setToolTip(g.title);
        auto *hl = new QHBoxLayout(btn);
        hl->setContentsMargins(8, 5, 8, 5);
        hl->setSpacing(10);
        auto *ic = new QLabel(btn);
        ic->setFixedSize(32, 32);
        ic->setPixmap(GameCatalog::icon(g.code, QSize(32, 32)));
        ic->setScaledContents(true);
        ic->setAttribute(Qt::WA_TransparentForMouseEvents);
        // Sidebar uses the game name; the PNG logo is only on the hero.
        auto *txtCol = new QVBoxLayout();
        txtCol->setSpacing(0);
        auto *tag = new QLabel(g.shortLabel.toUpper(), btn);
        tag->setObjectName("GameNavTag");
        tag->setAttribute(Qt::WA_TransparentForMouseEvents);
        auto *nameLbl = new QLabel(g.title, btn);
        nameLbl->setObjectName("GameNavName");
        nameLbl->setAttribute(Qt::WA_TransparentForMouseEvents);
        txtCol->addWidget(tag);
        txtCol->addWidget(nameLbl);
        hl->addWidget(ic);
        hl->addLayout(txtCol, 1);
        btn->setMinimumHeight(46);
        layout->addWidget(btn);
        m_gameButtons << btn;
        connect(btn, &QPushButton::clicked, this, [this, i]() { selectGame(i); });
    }

    layout->addSpacing(8);
    m_sidebarTools = new QLabel(tr("FERRAMENTAS"), sidebar);
    m_sidebarTools->setObjectName("NavSection");
    layout->addWidget(m_sidebarTools);

    struct Tool { QString text; QString icon; };
    const QList<Tool> tools = {
        {tr("Mods"), ":/icons/mods.svg"},
        {tr("Servidor LAN (beta)"), ":/icons/server.svg"},
        {tr("Configuracoes"), ":/icons/misc.svg"},
        {tr("Sobre"), ":/icons/app.svg"},
    };
    for (int i = 0; i < tools.size(); ++i) {
        auto *btn = new QPushButton(QIcon(tools[i].icon), "  " + tools[i].text, sidebar);
        btn->setObjectName("SidebarButton");
        btn->setCheckable(true);
        btn->setIconSize(QSize(16, 16));
        btn->setCursor(Qt::PointingHandCursor);
        layout->addWidget(btn);
        m_toolButtons << btn;
        connect(btn, &QPushButton::clicked, this, [this, i]() { showTool(i + 1); });
    }

    layout->addStretch();

    auto *rule = new QFrame(sidebar);
    rule->setObjectName("NavRule");
    rule->setFrameShape(QFrame::NoFrame);
    rule->setFixedHeight(1);
    layout->addWidget(rule);
    layout->addSpacing(8);

    auto *foot = new QWidget(sidebar);
    foot->setObjectName("SidebarFooter");
    auto *fl = new QHBoxLayout(foot);
    fl->setContentsMargins(2, 0, 2, 0);
    fl->setSpacing(6);
    auto *ver = new QLabel("v" + m_settings.versionNum, foot);
    ver->setObjectName("VersionBadge");
    auto *repo = new QPushButton(QStringLiteral("GitHub"), foot);
    repo->setObjectName("RepoLink");
    repo->setCursor(Qt::PointingHandCursor);
    repo->setToolTip(QStringLiteral("https://github.com/MestreTM/CLL-CodLanLauncher"));
    QObject::connect(repo, &QPushButton::clicked, foot, []() {
        QDesktopServices::openUrl(QUrl(QStringLiteral("https://github.com/MestreTM/CLL-CodLanLauncher")));
    });
    fl->addWidget(ver);
    fl->addWidget(repo);
    fl->addStretch();
    layout->addWidget(foot);

    return sidebar;
}

QWidget *MainWindow::buildHeader()
{
    m_header = new QWidget(this);
    m_header->setObjectName("HeaderBar");
    auto *hl = new QHBoxLayout(m_header);
    hl->setContentsMargins(28, 16, 20, 16);
    hl->setSpacing(12);

    auto *col = new QVBoxLayout();
    col->setSpacing(2);
    m_headerTitle = new QLabel(m_header);
    m_headerTitle->setObjectName("PageTitle");
    m_headerSub = new QLabel(m_header);
    m_headerSub->setObjectName("PageSubtitle");
    col->addWidget(m_headerTitle);
    col->addWidget(m_headerSub);
    hl->addLayout(col, 1);

    m_saveBtn = new QPushButton(tr("Salvar"), m_header);
    m_saveBtn->setProperty("cssClass", "ghost");
    m_saveBtn->setCursor(Qt::PointingHandCursor);
    hl->addWidget(m_saveBtn);

    connect(m_saveBtn, &QPushButton::clicked, this, &MainWindow::onSaveSettings);
    return m_header;
}

void MainWindow::applyHeader(int toolIndex)
{
    if (!m_header)
        return;
    if (toolIndex == ToolPlay) {
        m_header->hide();
        return;
    }
    struct Info { QString title; QString sub; bool save; };
    Info info;
    switch (toolIndex) {
    case ToolMods:
        info = {tr("Mods"), tr("Instale pacotes e mapas customizados por jogo."), false};
        break;
    case ToolServer:
        info = {tr("Servidor LAN (beta)"), tr("Hospede uma partida na sua rede local."), false};
        break;
    case ToolSettings:
        info = {tr("Configuracoes"), tr("Apelido, pastas do Plutonium e dos jogos."), true};
        break;
    default:
        info = {tr("Sobre"), tr("Versao, creditos e licencas."), false};
        break;
    }
    m_headerTitle->setText(info.title);
    m_headerSub->setText(info.sub);
    m_saveBtn->setVisible(info.save);
    m_header->show();
}

void MainWindow::selectGame(int gameIndex)
{
    m_gameIndex = gameIndex;
    m_toolIndex = ToolPlay;
    const auto g = GameCatalog::all().at(gameIndex);
    m_playPage->setGame(g.id);
    m_playPage->setRunning(m_runningPid > 0 && m_runningGameId == g.id);
    m_stack->setCurrentWidget(m_playPage);
    applyHeader(ToolPlay);
    syncNav();
}

void MainWindow::showTool(int toolIndex)
{
    m_toolIndex = toolIndex;
    const QString gameId = GameCatalog::all().at(m_gameIndex).id;
    if (toolIndex == ToolMods)
        m_modsPage->selectGame(gameId);
    else if (toolIndex == ToolServer)
        m_serverPage->selectGame(gameId);
    m_stack->setCurrentIndex(toolIndex);
    applyHeader(toolIndex);
    syncNav();
}

void MainWindow::syncNav()
{
    for (int i = 0; i < m_gameButtons.size(); ++i)
        m_gameButtons[i]->setChecked(m_toolIndex == ToolPlay && i == m_gameIndex);
    for (int i = 0; i < m_toolButtons.size(); ++i)
        m_toolButtons[i]->setChecked(m_toolIndex == i + 1);
}

void MainWindow::onLaunchGame(const QString &gameId, bool multiplayer)
{
    if (m_runningPid > 0)
        return;
    if (gameId == "World at War") m_settings.modeId = multiplayer ? "t4mp" : "t4sp";
    else if (gameId == "Black ops") m_settings.modeId = multiplayer ? "t5mp" : "t5sp";
    else if (gameId == "Black ops II") m_settings.modeId = multiplayer ? "t6mp" : "t6zm";
    else if (gameId == "Modern Warfare 3") m_settings.modeId = "iw5mp";
    m_settings.gameId = gameId;
    m_settings.saveToIni();

    const GameLauncher::Result result = GameLauncher::launch(m_settings, m_modsPage->selectedMod());
    if (result.hasError) {
        Dialogs::info(this, result.errorMsg, m_settings);
        return;
    }
    m_runningPid = result.pid;
    m_runningGameId = gameId;
    m_playPage->setRunning(true);
    m_processPollTimer.start();
}

void MainWindow::onStopGame(const QString &)
{
    if (m_runningPid > 0)
        GameLauncher::terminatePid(m_runningPid);
    m_runningPid = 0;
    m_runningGameId.clear();
    m_playPage->setRunning(false);
    m_processPollTimer.stop();
}

void MainWindow::onPollRunningProcess()
{
    if (m_runningPid > 0 && !GameLauncher::isPidRunning(m_runningPid)) {
        m_runningPid = 0;
        m_runningGameId.clear();
        m_playPage->setRunning(false);
    }
    if (m_serverPid > 0 && !GameLauncher::isPidRunning(m_serverPid)) {
        m_serverPid = 0;
        m_serverPage->setRunning(false);
    }
    if (m_runningPid <= 0 && m_serverPid <= 0)
        m_processPollTimer.stop();
}

void MainWindow::onLaunchServer(const QString &configSelection, const QString &port)
{
    if (m_serverPid > 0)
        return;
    m_settings.saveToIni();
    const GameLauncher::Result result = GameLauncher::launchServer(
        m_settings, m_modsPage->selectedMod(), configSelection, port);
    if (result.needsBo2GameSettings) {
        if (Dialogs::confirmDownloadGameSettings(this))
            m_serverPage->downloadBo2GameSettings();
        return;
    }
    if (result.hasError) {
        Dialogs::info(this, result.errorMsg, m_settings);
        return;
    }
    m_serverPid = result.pid;
    m_serverPage->setRunning(true);
    m_processPollTimer.start();
}

void MainWindow::onStopServer()
{
    if (m_serverPid > 0)
        GameLauncher::terminatePid(m_serverPid);
    m_serverPid = 0;
    m_serverPage->setRunning(false);
    if (m_runningPid <= 0)
        m_processPollTimer.stop();
}

void MainWindow::onSaveSettings() { m_settings.saveToIni(); }

