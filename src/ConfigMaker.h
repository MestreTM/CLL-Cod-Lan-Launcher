#pragma once
#include <QString>

class AppSettings;

namespace ConfigMaker
{
    bool generateConfig(const AppSettings &settings, const QString &serverId,
                         QString configName, const QString &selectedMap,
                         bool multiplayerMode, bool prer4516,
                         const QString &gameType, bool wagerMatch, QString &error);
}
