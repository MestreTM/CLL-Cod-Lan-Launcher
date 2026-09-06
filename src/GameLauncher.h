#pragma once
#include "Dialogs.h"
#include <QtGlobal>

class AppSettings;

namespace GameLauncher
{
    struct Result
    {
        bool ok = false;
        bool hasError = false;
        Dialogs::Msg errorMsg = Dialogs::Msg::Plutonium;
        bool needsBo2GameSettings = false;
        qint64 pid = 0;
    };

    Result launch(AppSettings &settings, const QString &modSelection);
    Result launchServer(AppSettings &settings, const QString &modSelection,
                         const QString &configSelection, const QString &port);
    bool terminatePid(qint64 pid);
    bool isPidRunning(qint64 pid);
}
