#pragma once
#include <QDialog>
#include <QFutureWatcher>
#include "AppSettings.h"

class QStackedWidget;
class QLineEdit;
class QLabel;
class QPushButton;
class QComboBox;
class QCheckBox;
class QWidget;
class QProgressBar;
class QRadioButton;
class QFrame;

class SetupWizard : public QDialog
{
    Q_OBJECT
public:
    explicit SetupWizard(AppSettings &settings, QWidget *parent = nullptr);
    bool completed() const { return m_completed; }

private slots:
    void onNext();
    void onBack();
    void onSkip();
    void onBrowseGame(const QString &gameId);
    void onRescan();
    void startKitInstall();
    void onKitFinished();

private:
    void buildUi();
    QWidget *buildWelcomePage();
    QWidget *buildNickPage();
    QWidget *buildGamesPage();
    QWidget *buildDonePage();
    void updateNavButtons();
    void applyDetectedGames();
    void finish(bool markDone);
    void setStepDots(int index);
    void refreshInstalledDetect();
    bool usingPortableKit() const;
    void applyPlutoniumFallback();
    void retranslate();
    void onLanguageChanged();

    AppSettings &m_settings;
    QStackedWidget *m_stack = nullptr;
    QPushButton *m_backBtn = nullptr;
    QPushButton *m_nextBtn = nullptr;
    QPushButton *m_skipBtn = nullptr;
    QLabel *m_headerTitle = nullptr;
    QLabel *m_headerSub = nullptr;
    QLabel *m_stepLabel = nullptr;
    QComboBox *m_langCombo = nullptr;
    QLabel *m_langLabel = nullptr;
    QLabel *m_welcomeLead = nullptr;
    QLabel *m_welcomeDesc = nullptr;
    QLabel *m_langHint = nullptr;
    QPushButton *m_browseInstalled = nullptr;
    QLabel *m_nickLead = nullptr;
    QLabel *m_nickDesc = nullptr;
    QLabel *m_gamesLead = nullptr;
    QPushButton *m_rescanBtn = nullptr;
    QLabel *m_doneLead = nullptr;
    QLabel *m_doneDesc = nullptr;
    QWidget *m_dots = nullptr;
    QList<QLabel *> m_dotLabels;

    QLineEdit *m_nickEdit = nullptr;
    struct GameRow {
        QString id;
        QLineEdit *edit = nullptr;
        QCheckBox *enabled = nullptr;
    };
    QList<GameRow> m_gameRows;
    QLabel *m_detectStatus = nullptr;
    bool m_completed = false;

    QProgressBar *m_kitProgress = nullptr;
    QLabel *m_kitStatus = nullptr;
    QRadioButton *m_radioPortable = nullptr;
    QRadioButton *m_radioInstalled = nullptr;
    QFrame *m_installedBox = nullptr;
    QLineEdit *m_installedPath = nullptr;
    QLabel *m_installedStatus = nullptr;
    QFutureWatcher<QString> m_kitWatcher;
    bool m_kitBusy = false;
    bool m_kitOk = false;
};
