#pragma once
#include <QWidget>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFutureWatcher>

class QComboBox;
class QListWidget;
class QLineEdit;
class QPushButton;
class QLabel;
class QProgressBar;
class AppSettings;

class ModsPage : public QWidget
{
    Q_OBJECT
public:
    explicit ModsPage(AppSettings &settings, QWidget *parent = nullptr);
    QString selectedMod() const;
    void refreshList();
    void selectGame(const QString &gameId);

private slots:
    void onGameSelected(const QString &gameName);
    void onRefreshOrDeselect(bool clearSelection);
    void onBrowseModFile();
    void onInstallMod();
    void onDeleteMod();

private:
    void setBusy(bool busy);
    void runSevenZipBootstrap();
    void installStandardOrSmart(const QString &archivePath);
    void handleModFile(const QString &path);
    bool tryCllInstall(const QString &path);
protected:
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;
public:
    void retranslate();
private:

    AppSettings &m_settings;
    QComboBox *m_gameCombo = nullptr;
    QLabel *m_titleLabel = nullptr;
    QListWidget *m_modList = nullptr;
    QLineEdit *m_modPathEdit = nullptr;
    QPushButton *m_installBtn = nullptr;
    QPushButton *m_deleteBtn = nullptr;
    QProgressBar *m_progress = nullptr;
    QLabel *m_progressLabel = nullptr;
    enum class Job { None, Install, Remove };
    Job m_job = Job::None;

    QFutureWatcher<QString> m_installWatcher; // empty = ok; otherwise an error string
    QFutureWatcher<int> m_sevenZipWatcher;
};
