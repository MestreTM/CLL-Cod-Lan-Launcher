#pragma once
#include <QDialog>
#include <QFutureWatcher>

class QLabel;
class QProgressBar;
class QPlainTextEdit;
class QPushButton;

// Downloads pu.dat and extracts it into ./pu
class ImportKitDialog : public QDialog
{
    Q_OBJECT
public:
    explicit ImportKitDialog(QWidget *parent = nullptr);
    bool installedOk() const { return m_ok; }

    static QString destPu();
    static QString puDatUrl();

private slots:
    void startDownload();
    void onFinished();

private:
    QLabel *m_status = nullptr;
    QProgressBar *m_bar = nullptr;
    QPlainTextEdit *m_log = nullptr;
    QPushButton *m_startBtn = nullptr;
    QPushButton *m_closeBtn = nullptr;
    QFutureWatcher<QString> m_watcher;
    bool m_ok = false;
};
