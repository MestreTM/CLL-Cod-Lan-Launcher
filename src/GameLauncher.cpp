#include "GameLauncher.h"
#include "AppSettings.h"
#include "SmartModInstaller.h"
#include "Storage.h"

#include <QDir>
#include <QFileInfo>
#include <QProcess>
#include <QStringList>

#ifdef Q_OS_WIN
#  include <windows.h>
#else
#  include <signal.h>
#  include <errno.h>
#endif

namespace {

const QString kExePath = "/bin/plutonium-bootstrapper-win32.exe";

QStringList splitFlag(const QString &flag)
{
    return flag.split(' ', Qt::SkipEmptyParts);
}

} // namespace

namespace GameLauncher {

Result launch(AppSettings &settings, const QString &modSelection)
{
    Result r;

    if (settings.username.isEmpty()) {
        r.hasError = true;
        r.errorMsg = Dialogs::Msg::Username;
        return r;
    }

    const QString bootstrapper = settings.plutoniumInstance + kExePath;
    if (!QFileInfo::exists(bootstrapper)) {
        r.hasError = true;
        r.errorMsg = Dialogs::Msg::Plutonium;
        return r;
    }

    if (settings.gameId == "World at War") {
        settings.activeGame = settings.waw;
        if (!QFileInfo::exists(settings.activeGame + "/main/iw_00.iwd")) {
            r.hasError = true; r.errorMsg = Dialogs::Msg::T4; return r;
        }
    } else if (settings.gameId == "Black ops") {
        settings.activeGame = settings.bo1;
        if (!QFileInfo::exists(settings.activeGame + "/main/iw_00.iwd")) {
            r.hasError = true; r.errorMsg = Dialogs::Msg::T5; return r;
        }
    } else if (settings.gameId == "Black ops II") {
        settings.activeGame = settings.bo2;
        if (!QFileInfo::exists(settings.activeGame + "/zone/all/base.ipak")) {
            r.hasError = true; r.errorMsg = Dialogs::Msg::T6; return r;
        }
    } else if (settings.gameId == "Modern Warfare 3") {
        settings.activeGame = settings.mw3;
        if (!QFileInfo::exists(settings.activeGame + "/main/iw_00.iwd")) {
            r.hasError = true; r.errorMsg = Dialogs::Msg::IW5; return r;
        }
    }

    QStringList args;
    args << settings.modeId << settings.activeGame << "+name" << settings.username << "-lan";

    if (!modSelection.isEmpty()) {
        if (settings.gameId != settings.modId) {
            r.hasError = true;
            r.errorMsg = Dialogs::Msg::WrongGame;
            return r;
        }
        const QString sid = Storage::gameStorageId(settings.gameId);
        SmartModInstaller::cleanupEmptyAliasFolder(settings.plutoniumInstance, sid, modSelection);
        const QString folder = SmartModInstaller::resolveFsGameFolder(
            settings.plutoniumInstance, sid, modSelection);
        if (!folder.isEmpty())
            args << "+set" << "fs_game" << ("mods/" + folder);
    }

    qint64 pid = 0;
    if (!QProcess::startDetached(bootstrapper, args, settings.plutoniumInstance, &pid)) {
        r.hasError = true;
        r.errorMsg = Dialogs::Msg::Plutonium;
        return r;
    }
    r.ok = true;
    r.pid = pid;
    return r;
}

Result launchServer(AppSettings &settings, const QString &modSelection,
                     const QString &configSelection, const QString &port)
{
    Result r;

    const QString bootstrapper = settings.plutoniumInstance + kExePath;
    if (!QFileInfo::exists(bootstrapper)) {
        r.hasError = true;
        r.errorMsg = Dialogs::Msg::Plutonium;
        return r;
    }

    if (settings.serverId == "World at War") {
        settings.activeGame = settings.waw;
        settings.wawServMult = settings.serverMultiplayerSelected ? QString() : QStringLiteral("+set zombiemode 1");
        if (!QFileInfo::exists(settings.activeGame + "/main/iw_00.iwd")) {
            r.hasError = true; r.errorMsg = Dialogs::Msg::T4; return r;
        }
    } else if (settings.serverId == "Black ops") {
        settings.activeGame = settings.bo1;
        if (!QFileInfo::exists(settings.activeGame + "/main/iw_00.iwd")) {
            r.hasError = true; r.errorMsg = Dialogs::Msg::T5; return r;
        }
    } else if (settings.serverId == "Black ops II") {
        if (!QDir(settings.plutoniumInstance + "/storage/t6/gamesettings").exists()) {
            r.hasError = true;
            r.needsBo2GameSettings = true;
            return r;
        }
        settings.activeGame = settings.bo2;
        if (!QFileInfo::exists(settings.activeGame + "/zone/all/base.ipak")) {
            r.hasError = true; r.errorMsg = Dialogs::Msg::T6; return r;
        }
    } else if (settings.serverId == "Modern Warfare 3") {
        settings.activeGame = settings.mw3;
        if (!QFileInfo::exists(settings.activeGame + "/main/iw_00.iwd")) {
            r.hasError = true; r.errorMsg = Dialogs::Msg::IW5; return r;
        }
    }

    QStringList args;
    args << settings.modeId << settings.activeGame << "-lan" << "-dedicated";
    args << splitFlag(settings.wawServMult);

    if (!modSelection.isEmpty()) {
        if (settings.serverId != settings.modId) {
            r.hasError = true;
            r.errorMsg = Dialogs::Msg::WrongGameServer;
            return r;
        }
        const QString sid = Storage::gameStorageId(settings.serverId);
        SmartModInstaller::cleanupEmptyAliasFolder(settings.plutoniumInstance, sid, modSelection);
        const QString folder = SmartModInstaller::resolveFsGameFolder(
            settings.plutoniumInstance, sid, modSelection);
        if (!folder.isEmpty())
            args << "+set" << "fs_game" << ("mods/" + folder);
    }

    args << "+exec" << configSelection << "+set" << "net_port" << port << "+map_rotate";

    qint64 pid = 0;
    if (!QProcess::startDetached(bootstrapper, args, settings.plutoniumInstance, &pid)) {
        r.hasError = true;
        r.errorMsg = Dialogs::Msg::Plutonium;
        return r;
    }
    r.ok = true;
    r.pid = pid;
    return r;
}

bool terminatePid(qint64 pid)
{
    if (pid <= 0)
        return false;
#ifdef Q_OS_WIN
    return QProcess::startDetached("taskkill", {"/PID", QString::number(pid), "/T", "/F"});
#else
    return ::kill(static_cast<pid_t>(pid), SIGTERM) == 0 || errno == ESRCH;
#endif
}

bool isPidRunning(qint64 pid)
{
    if (pid <= 0)
        return false;
#ifdef Q_OS_WIN
    HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, static_cast<DWORD>(pid));
    if (!h)
        return false;
    DWORD code = 0;
    const BOOL ok = GetExitCodeProcess(h, &code);
    CloseHandle(h);
    return ok && code == STILL_ACTIVE;
#else
    return ::kill(static_cast<pid_t>(pid), 0) == 0 || errno == EPERM;
#endif
}

} // namespace GameLauncher
