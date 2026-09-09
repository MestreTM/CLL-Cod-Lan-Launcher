#pragma once
#include <QHash>
#include <QTimer>
#include <QWidget>

#include "../HomeCatalog.h"

class QLineEdit;
class QPushButton;
class QVBoxLayout;
class QHBoxLayout;
class QScrollArea;
class QLabel;
class QNetworkAccessManager;
class AppSettings;
class HeroSlider;

// Home: slider de destaques + mods agrupados por jogo, alimentada por
// cll_home.json. Mesma linguagem visual das outras paginas (dark.qss).
class HomePage : public QWidget
{
    Q_OBJECT
public:
    explicit HomePage(AppSettings &settings, QWidget *parent = nullptr);

    void reload();                                // rele o cll_home.json
    void selectGame(const QString &gameId);       // "" / "all" = todos
    QString selectedGame() const { return m_gameFilter; }
    void retranslate();

signals:
    void installRequested(const QString &modId);  // ligar no CllInstaller / GithubModInstaller
    void detailsRequested(const QString &modId);
    void gameRequested(const QString &gameId);    // clique em "Ver todos"

protected:
    void resizeEvent(QResizeEvent *event) override;

private:
    void buildUi();
    void rebuildFilters();
    void rebuildContent();
    void relayoutGrids();
    void openDetails(const QString &modId);
    void requestRemote(const QString &url, const QString &modId);
    QPixmap resolvedCover(const QString &ref) const;
    void applyRemoteCover(const QString &modId, const QPixmap &pm);
    void applyRemoteUrl(const QString &url, const QPixmap &pm);
    QList<HomeCatalog::Mod> filteredMods(const QString &gameId) const;

    AppSettings &m_settings;
    HomeCatalog::Catalog m_catalog;

    QLineEdit *m_search = nullptr;
    QWidget *m_filterBar = nullptr;
    QHBoxLayout *m_filterLayout = nullptr;
    QList<QPushButton *> m_filterButtons;
    HeroSlider *m_hero = nullptr;
    QScrollArea *m_scroll = nullptr;
    QWidget *m_content = nullptr;
    QVBoxLayout *m_contentLayout = nullptr;
    QLabel *m_status = nullptr;
    QPushButton *m_importBtn = nullptr;

    QString m_gameFilter = QStringLiteral("all");
    QString m_query;
    QTimer m_searchDebounce;
    QNetworkAccessManager *m_net = nullptr;
    QHash<QString, QPixmap> m_remote; // url -> pixmap ja baixado
};
