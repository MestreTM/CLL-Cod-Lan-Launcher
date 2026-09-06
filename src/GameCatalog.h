#pragma once
#include <QPixmap>
#include <QString>
#include <QStringList>
#include <QSize>

// Catalog of the four games plus art (t6_icon.jpg, t6_logo.png, t6_bg_mp.jpg).
namespace GameCatalog
{
    struct Game {
        QString code;       // t4 t5 t6 iw5
        QString id;         // "World at War" ...
        QString shortLabel; // T4
        QString title;      // World at War
        QString subtitle;
        bool hasZmSp = true;
    };

    QList<Game> all();
    Game byCode(const QString &code);
    Game byId(const QString &id);

    // Folder next to the exe first, then Qt resources.
    QString userMediaDir();
    // Per-mode backgrounds: <code>_bg_mp.jpg / <code>_bg_zm.jpg (then <code>_bg.jpg)
    QString iconPath(const QString &code);
    QString backgroundPath(const QString &code, const QString &variant = QString());
    QString logoPath(const QString &code);
    QPixmap icon(const QString &code, const QSize &size = QSize(48, 48));
    QPixmap background(const QString &code, const QSize &size = QSize(1600, 900),
                       const QString &variant = QString());
    QPixmap logo(const QString &code, const QSize &size = QSize(640, 140));
}
