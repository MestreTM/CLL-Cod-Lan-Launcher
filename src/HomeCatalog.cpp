#include "HomeCatalog.h"
#include "AppSettings.h"
#include "Downloader.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocale>
#include <QObject>

namespace {

QStringList toStringList(const QJsonValue &v)
{
    QStringList out;
    for (const QJsonValue &item : v.toArray())
        out << item.toString();
    return out;
}

QHash<QString, QPixmap> &cache()
{
    static QHash<QString, QPixmap> c;
    return c;
}

} // namespace

namespace HomeCatalog {

QString catalogUrl()
{
    return QStringLiteral("https://mestretm.github.io/CLL-Cod-Lan-Launcher/cll_home.json");
}

QString defaultPath()
{
    return catalogUrl();
}

Catalog load(const QString &path)
{
    Catalog c;
    const QString url = (path.startsWith(QLatin1String("http://"), Qt::CaseInsensitive)
                         || path.startsWith(QLatin1String("https://"), Qt::CaseInsensitive))
                            ? path
                            : catalogUrl();

    QString netError;
    const QByteArray raw = Downloader::downloadBytes(url, netError, 30000);
    if (raw.isEmpty()) {
        c.error = netError.isEmpty()
                      ? QObject::tr("Could not load the page.")
                      : QObject::tr("Could not load the page. %1").arg(netError);
        return c;
    }

    QJsonParseError err {};
    const QJsonDocument doc = QJsonDocument::fromJson(raw, &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) {
        c.error = QObject::tr("cll_home.json invalido: %1").arg(err.errorString());
        return c;
    }

    const QJsonObject root = doc.object();
    c.title = root.value("title").toString();
    c.featured = toStringList(root.value("featured"));

    for (const QJsonValue &v : root.value("games").toArray()) {
        const QJsonObject o = v.toObject();
        Game g;
        g.id = o.value("id").toString();
        g.code = o.value("code").toString();
        g.name = o.value("name").toString();
        g.icon = o.value("icon").toString();
        g.logo = o.value("logo").toString();
        g.art = o.value("art").toString();
        if (!g.id.isEmpty())
            c.games << g;
    }

    for (const QJsonValue &v : root.value("mods").toArray()) {
        const QJsonObject o = v.toObject();
        Mod m;
        m.id = o.value("id").toString();
        m.name = o.value("name").toString();
        m.game = o.value("game").toString();
        m.author = o.value("author").toString();
        m.summary = o.value("summary").toString();
        m.description = o.value("description").toString();
        if (m.description.isEmpty())
            m.description = m.summary;
        m.image = o.value("image").toString();
        m.version = o.value("version").toString();
        m.size = o.value("size").toString();
        m.kind = o.value("kind").toString(QStringLiteral("cll"));
        m.url = o.value("url").toString();
        m.tags = toStringList(o.value("tags"));
        m.downloads = qint64(o.value("downloads").toDouble());
        if (!m.id.isEmpty())
            c.mods << m;
    }

    c.ok = true;
    return c;
}

bool isRemote(const QString &ref)
{
    return ref.startsWith("http://", Qt::CaseInsensitive) || ref.startsWith("https://", Qt::CaseInsensitive);
}

QString resolveAsset(const QString &ref)
{
    if (ref.isEmpty() || isRemote(ref) || ref.startsWith(':'))
        return ref;

    const QFileInfo fi(ref);
    if (fi.isAbsolute())
        return ref;

    const QString local = QDir(AppSettings::projectRoot()).filePath(ref);
    if (QFileInfo::exists(local))
        return local;

    QString rel = ref;
    if (rel.startsWith("./"))
        rel.remove(0, 2);
    const QString qrc = ":/" + rel;
    if (QFileInfo::exists(qrc))
        return qrc;

    return local; // deixa o caminho local para a mensagem de erro fazer sentido
}

QPixmap pixmap(const QString &ref)
{
    if (ref.isEmpty() || isRemote(ref))
        return QPixmap();

    const QString path = resolveAsset(ref);
    auto it = cache().constFind(path);
    if (it != cache().constEnd())
        return it.value();

    QPixmap pm;
    pm.load(path);
    cache().insert(path, pm);
    return pm;
}

const Mod *findMod(const Catalog &c, const QString &id)
{
    for (const Mod &m : c.mods)
        if (m.id == id)
            return &m;
    return nullptr;
}

const Game *findGame(const Catalog &c, const QString &id)
{
    for (const Game &g : c.games)
        if (g.id == id)
            return &g;
    return nullptr;
}

QString formatDownloads(qint64 n)
{
    QLocale loc;
    if (n >= 1000000)
        return loc.toString(n / 1000000.0, 'f', 1) + "M";
    if (n >= 1000)
        return loc.toString(n / 1000.0, 'f', 1) + "K";
    return QString::number(n);
}

} // namespace HomeCatalog
