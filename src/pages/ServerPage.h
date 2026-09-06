#pragma once
#include <QWidget>
#include <QFutureWatcher>

class QComboBox;
class QListWidget;
class QLineEdit;
class QRadioButton;
class QLabel;
class QPushButton;
class QProgressBar;
class AppSettings;

class ServerPage : public QWidget
{
    Q_OBJECT
public:
    explicit ServerPage(AppSettings &settings, QWidget *parent = nullptr);

    void refreshList();
    void downloadBo2GameSettings();
    void downloadT5BaseConfigs();
    void downloadT4BaseConfigs();
    void setRunning(bool running);
    void selectGame(const QString &gameId);

signals:
    void launchServerRequested(const QString &configSelection, const QString &port);
    void stopServerRequested();

protected:
    void showEvent(QShowEvent *event) override;

private slots:
    void onGameSelected(const QString &gameName);
    void onRefreshOrDeselect();
    void onDeleteConfig();
    void onMakeConfig();
    void onEditConfig();
    void onHowToConnect();
    void onLaunchServer();

private:
    void refreshIpList();
    QString selectedIp() const;
    QString selectedConfigPath() const;
public:
    void retranslate();
private:

    AppSettings &m_settings;
    QComboBox *m_gameCombo = nullptr;
    QLabel *m_titleLabel = nullptr;
    QListWidget *m_configList = nullptr;
    QRadioButton *m_zmRadio = nullptr;
    QRadioButton *m_mpRadio = nullptr;
    QLineEdit *m_portEdit = nullptr;
    QComboBox *m_ipCombo = nullptr;
    QPushButton *m_makeConfigBtn = nullptr;
    QPushButton *m_editConfigBtn = nullptr;
    QPushButton *m_connectHelpBtn = nullptr;
    QPushButton *m_launchBtn = nullptr;
    QProgressBar *m_progress = nullptr;
    QLabel *m_progressLabel = nullptr;
    bool m_running = false;

    QFutureWatcher<bool> m_gameSettingsWatcher;
    QString m_downloadKind;
};
