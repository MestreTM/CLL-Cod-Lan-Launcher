#pragma once
#include <QMainWindow>
#include <QTimer>

#include "AppSettings.h"

class QStackedWidget;
class QPushButton;
class QLabel;
class QWidget;
class PlayPage;
class ModsPage;
class ServerPage;
class SettingsPage;
class AboutPage;

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    Q_INVOKABLE void debugShowPage(int index) { showTool(index); }

private slots:
    void onLaunchGame(const QString &gameId, bool multiplayer);
    void onStopGame(const QString &gameId);
    void onLaunchServer(const QString &configSelection, const QString &port);
    void onStopServer();
    void onSaveSettings();
    void onPollRunningProcess();

private:
    enum ToolIndex { ToolPlay = 0, ToolMods, ToolServer, ToolSettings, ToolAbout };

    QWidget *buildSidebar();
    QWidget *buildHeader();
    void selectGame(int gameIndex);
    void showTool(int toolIndex);
    void syncNav();
    void applyHeader(int toolIndex);
    void retranslate();

    AppSettings m_settings;
    QStackedWidget *m_stack = nullptr;
    PlayPage *m_playPage = nullptr;
    ModsPage *m_modsPage = nullptr;
    ServerPage *m_serverPage = nullptr;
    SettingsPage *m_settingsPage = nullptr;
    AboutPage *m_aboutPage = nullptr;

    QWidget *m_header = nullptr;
    QLabel *m_headerTitle = nullptr;
    QLabel *m_headerSub = nullptr;
    QPushButton *m_saveBtn = nullptr;
    QLabel *m_sidebarBy = nullptr;
    QLabel *m_sidebarGames = nullptr;
    QLabel *m_sidebarTools = nullptr;

    QList<QPushButton *> m_gameButtons;
    QList<QPushButton *> m_toolButtons;
    int m_gameIndex = 2; // BO2
    int m_toolIndex = ToolPlay;

    qint64 m_runningPid = 0;
    QString m_runningGameId;
    qint64 m_serverPid = 0;
    QTimer m_processPollTimer;
};
