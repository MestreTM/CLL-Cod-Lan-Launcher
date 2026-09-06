#pragma once
#include <QString>
#include <QStringList>

// Single "Nocturne" theme: graphite-blue background plus one accent.
// names()/displayName() stay for old INI files (THEME is still read/written
// but no longer changes the look).
namespace Theme
{
    QString accent();      // #9184d9
    QString accentDark();  // estado pressionado / hover cheio
    QString accentText();  // texto sobre a cor de destaque
    QString muted();       // texto secundario
    QString line();        // linhas e bordas

    QStringList names();
    QString displayName(const QString &themeKey);
    void apply(const QString &themeName = QString());
}
