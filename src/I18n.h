#pragma once
#include <QObject>
#include <QString>
#include <QStringList>

class I18nHub : public QObject
{
    Q_OBJECT
public:
    static I18nHub &instance();
signals:
    void languageChanged();
};

namespace I18n
{
    QStringList codes();
    QString displayName(const QString &code);
    QString detectSystem();
    QString current();
    void apply(const QString &code);
}
