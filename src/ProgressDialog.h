#pragma once
#include <QDialog>

class QLabel;
class QProgressBar;
class QPushButton;

// Simple progress dialog for downloads and extracts.
class ProgressDialog : public QDialog
{
    Q_OBJECT
public:
    explicit ProgressDialog(const QString &title, QWidget *parent = nullptr);

    void setStatus(const QString &text);
    void setProgress(qint64 received, qint64 total); // total<=0 => indeterminado
    void setIndeterminate(bool on);
    void setFinished(bool success, const QString &message);

signals:
    void cancelRequested();

private:
    QLabel *m_status = nullptr;
    QProgressBar *m_bar = nullptr;
    QPushButton *m_closeBtn = nullptr;
};
