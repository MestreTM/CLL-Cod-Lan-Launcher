#include "Theme.h"

#include <QApplication>
#include <QFile>
#include <QTextStream>

namespace Theme {

QString accent()     { return QStringLiteral("#9184d9"); }
QString accentDark() { return QStringLiteral("#7a6cc9"); }
QString accentText() { return QStringLiteral("#14121f"); }
QString muted()      { return QStringLiteral("#8a8ba3"); }
QString line()       { return QStringLiteral("#2b2e42"); }

QStringList names()
{
    return {QStringLiteral("Nocturne")};
}

QString displayName(const QString &)
{
    return QStringLiteral("Escuro");
}

void apply(const QString &)
{
    QFile file(QStringLiteral(":/style/dark.qss"));
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return;

    QString qss = QTextStream(&file).readAll();
    // Compatibility: older stylesheets may still use these tokens.
    qss.replace(QStringLiteral("@ACCENT_DARK@"), accentDark());
    qss.replace(QStringLiteral("@ACCENT_TEXT@"), accentText());
    qss.replace(QStringLiteral("@ACCENT@"), accent());

    if (auto *app = qApp)
        app->setStyleSheet(qss);
}

} // namespace Theme
