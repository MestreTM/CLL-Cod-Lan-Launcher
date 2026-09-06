#include "I18n.h"

#include <QCoreApplication>
#include <QFile>
#include <QIODevice>
#include <QHash>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocale>
#include <QTranslator>

namespace {

QString g_current = QStringLiteral("pt_BR");
QTranslator *g_tr = nullptr;

class MapTranslator : public QTranslator
{
public:
    explicit MapTranslator(const QHash<QString, QString> &map, QObject *parent = nullptr)
        : QTranslator(parent), m_map(map) {}

    QString translate(const char *, const char *sourceText,
                      const char *, int) const override
    {
        if (!sourceText)
            return QString();
        return m_map.value(QString::fromUtf8(sourceText));
    }

private:
    QHash<QString, QString> m_map;
};

QHash<QString, QString> parseJsonFile(QFile &f)
{
    QHash<QString, QString> map;
    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    if (!doc.isObject())
        return map;
    const QJsonObject obj = doc.object();
    for (auto it = obj.begin(); it != obj.end(); ++it)
        map.insert(it.key(), it.value().toString());
    return map;
}

QHash<QString, QString> loadMap(const QString &code)
{
    QFile res(QStringLiteral(":/i18n/%1.json").arg(code));
    if (res.open(QIODevice::ReadOnly))
        return parseJsonFile(res);
    return {};
}

} // namespace

I18nHub &I18nHub::instance()
{
    static I18nHub hub;
    return hub;
}

namespace I18n {

QStringList codes()
{
    return {QStringLiteral("en"), QStringLiteral("pt_BR"),
            QStringLiteral("es"), QStringLiteral("ru")};
}

QString displayName(const QString &code)
{
    if (code == QLatin1String("en"))
        return QStringLiteral("English");
    if (code == QLatin1String("es"))
        return QStringLiteral("Espanol");
    if (code == QLatin1String("ru"))
        return QString::fromUtf8("Русский");
    return QString::fromUtf8("Português (Brasil)");
}

QString detectSystem()
{
    const QString loc = QLocale::system().name();
    if (loc.startsWith(QLatin1String("pt")))
        return QStringLiteral("pt_BR");
    if (loc.startsWith(QLatin1String("es")))
        return QStringLiteral("es");
    if (loc.startsWith(QLatin1String("ru")))
        return QStringLiteral("ru");
    return QStringLiteral("en");
}

QString current()
{
    return g_current;
}

void apply(const QString &code)
{
    QString use = code;
    if (!codes().contains(use))
        use = detectSystem();
    g_current = use;

    if (g_tr) {
        QCoreApplication::removeTranslator(g_tr);
        delete g_tr;
        g_tr = nullptr;
    }
    g_tr = new MapTranslator(loadMap(use), qApp);
    QCoreApplication::installTranslator(g_tr);
    emit I18nHub::instance().languageChanged();
}

} // namespace I18n
