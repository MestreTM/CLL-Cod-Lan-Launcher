#pragma once
#include <QList>
#include <QPixmap>
#include <QString>
#include <QStringList>

// Home catalog from https://mestretm.github.io/CLL-Cod-Lan-Launcher/cll_home.json
namespace HomeCatalog
{

struct Mod
{
    QString id;
    QString name;
    QString game;        // id do jogo: t4 / t5 / t6 / iw5
    QString author;
    QString summary;     // uma linha (card e slider)
    QString description; // texto completo (dialogo)
    QString image;       // "media/t6_bg_mp.jpg", ":/media/..." ou http(s)
    QString version;
    QString size;
    QString kind;        // "cll" | "github"
    QString url;
    QStringList tags;
    qint64 downloads = 0;
};

struct Game
{
    QString id;
    QString code;   // T4 / T5 / T6 / IW5
    QString name;
    QString icon;
    QString logo;
    QString art;
};

struct Catalog
{
    QString title;
    QList<Game> games;
    QStringList featured; // ids de mods, na ordem do slider
    QList<Mod> mods;
    bool ok = false;
    QString error;
};

QString catalogUrl();
QString defaultPath();

Catalog load(const QString &path = QString());

// Caminho absoluto / recurso qrc para uma referencia do JSON.
// "media/x.jpg" -> <projectRoot>/media/x.jpg ou :/media/x.jpg
// "http(s)://..." e devolvido como veio (baixado pela HomePage).
QString resolveAsset(const QString &ref);
bool isRemote(const QString &ref);

// Pixmap com cache em memoria (apenas local/qrc).
QPixmap pixmap(const QString &ref);

const Mod *findMod(const Catalog &c, const QString &id);
const Game *findGame(const Catalog &c, const QString &id);

QString formatDownloads(qint64 n);

} // namespace HomeCatalog
