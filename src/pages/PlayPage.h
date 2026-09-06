#pragma once
#include <QWidget>
#include <QPixmap>

class QLabel;
class QPushButton;
class QRadioButton;
class QLineEdit;
class AppSettings;

class PlayPage : public QWidget
{
    Q_OBJECT
public:
    explicit PlayPage(AppSettings &settings, QWidget *parent = nullptr);

    void setGame(const QString &gameId);
    QString currentGameId() const { return m_gameId; }
    void setRunning(bool running);
    bool isMultiplayer() const;

signals:
    void launchRequested(const QString &gameId, bool multiplayer);
    void stopRequested(const QString &gameId);

protected:
    void resizeEvent(QResizeEvent *event) override;
    void paintEvent(QPaintEvent *event) override;

private:
    void refreshArt();
public:
    void retranslate();
private:

    AppSettings &m_settings;
    QString m_gameId;
    QPixmap m_bg;
    QLabel *m_code = nullptr;
    QLabel *m_logo = nullptr;
    QLabel *m_title = nullptr;
    QLabel *m_sub = nullptr;
    QLabel *m_path = nullptr;
    QRadioButton *m_spzm = nullptr;
    QRadioButton *m_mp = nullptr;
    QPushButton *m_play = nullptr;
    bool m_running = false;
};
