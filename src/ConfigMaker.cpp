#include "ConfigMaker.h"
#include "AppSettings.h"

#include <QDir>
#include <QFile>
#include <QTextStream>
#include <QObject>
#include <QRegularExpression>
#include <QFileInfo>

namespace ConfigMaker {

bool generateConfig(const AppSettings &settings, const QString &serverId,
                     QString configName, const QString &selectedMap,
                     bool multiplayerMode, bool prer4516,
                     const QString &gameType, bool wagerMatch, QString &error)
{
    QString path;
    QString configType;

    // match serverid:
    if (serverId == "World at War") {
        path = settings.plutoniumInstance + "/storage/t4/main/";
        configType = "t4";
    } else if (serverId == "Black ops") {
        path = settings.plutoniumInstance + "/storage/t5/main/";
        configType = "t5";
    } else if (serverId == "Black ops II") {
        path = settings.plutoniumInstance + "/storage/t6/main/";
        configType = "t6";
    } else if (serverId == "Modern Warfare 3") {
        path = settings.plutoniumInstance + "/storage/iw5/main/";
        configType = "iw5";
        multiplayerMode = true;
    }

    if (multiplayerMode) {
        configName = "mp_" + configName.replace(' ', '_');
        configType += "mp";
    } else {
        configName = "zm_" + configName.replace(' ', '_');
        configType += "zm";
    }

    if (prer4516)
        configName += "_Pre_r4516";

    QString mapRotation;
    // match configType:
    if (configType == "t4mp" || configType == "t4zm")
        mapRotation = "set sv_maprotationcurrent \"\"";
    else if (configType == "t5mp" || configType == "t5zm")
        mapRotation = "";
    else if (configType == "t6mp" || configType == "t6zm")
        mapRotation = "map_rotate";
    else if (configType == "iw5mp")
        mapRotation = "";

    const QString cfg = QStringLiteral(
        "set sp_minplayers 1\n"
        "set g_password \"\"\n"
        "set rcon_rate_limit \"500\"\n"
        "\n"
        "rconWhitelistAdd \"127.0.0.1\"\n"
        "rconWhitelistAdd \"192.168.0.7\"\n"
        "rconWhitelistAdd \"10.0.0.12\"\n"
        "rconWhitelistAdd \"172.16.8.7\"\n"
        "\n"
        "set sv_maxclients \"4\"\n"
        "set sv_maxRate \"25000\"\n"
        "set sv_pure \"0\"\n"
        "set scr_game_spectatetype \"1\"\n"
        "set g_gravity \"800\"\n"
        "set g_speed \"190\"\n"
        "set bullet_penetration_affected_by_team false\n"
        "set perk_weapRateEnhanced false\n"
        "\n"
        "set rate \"25000\"\n"
        "set g_antilag \"1\"\n"
        "set sv_fps \"20\"\n"
        "\n"
        "set sv_allowDownload \"0\"\n"
        "set sv_wwwDownload \"0\"\n"
        "set sv_wwwBaseURL \"\"\n"
        "set sv_wwwDlDisconnected \"0\"\n"
        "\n"
        "set g_log \"%1.log\"\n"
        "set g_logSync \"2\"\n"
        "set logfile \"2\"\n"
        "set sv_kickBanTime \"300\"\n"
        "\n"
        "set fire_audio_random_max_duration \"1000\"\n"
        "set fire_audio_repeat_duration \"1500\"\n"
        "set fire_spread_probability \"0\"\n"
        "set fire_stage1_burn_time \"3000\"\n"
        "set fire_stage2_burn_time \"0\"\n"
        "set fire_stage3_burn_time \"0\"\n"
        "set fire_world_damage \"20\"\n"
        "set fire_world_damage_duration \"8\"\n"
        "set fire_world_damage_rate \"0.25\"\n"
        "set flareDisableEffects \"0\"\n"
        "\n"
        "demo_enabled 0\n"
        "\n"
        "set sv_mapRotation \"%2\"\n"
        "%3\n"
    ).arg(configName, selectedMap, mapRotation);

    if (configType.startsWith(QLatin1String("t5")) || configType.startsWith(QLatin1String("t4"))) {
        QString baseName;
        QString baseDir;
        if (configType.startsWith(QLatin1String("t4"))) {
            baseName = multiplayerMode ? QStringLiteral("server.cfg") : QStringLiteral("server_zm.cfg");
            baseDir = settings.plutoniumInstance + "/storage/t4/main/";
        } else {
            baseName = multiplayerMode ? QStringLiteral("dedicated.cfg") : QStringLiteral("dedicated_sp.cfg");
            baseDir = settings.plutoniumInstance + "/storage/t5/";
        }
        const QString basePath = baseDir + baseName;
        QFile base(basePath);
        if (base.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QString text = QString::fromUtf8(base.readAll());
            base.close();
            const QString rotation = QStringLiteral("set sv_maprotation \"%1\"").arg(selectedMap);
            QRegularExpression re(QStringLiteral("set\\s+sv_maprotation\\s+\"[^\"]*\""), QRegularExpression::CaseInsensitiveOption);
            if (re.match(text).hasMatch())
                text.replace(re, rotation);
            else
                text += QLatin1Char('\n') + rotation + QLatin1Char('\n');
            if (!gameType.isEmpty()) {
                QRegularExpression gt(QStringLiteral("set\\s+g_gametype\\s+\"[^\"]*\""));
                const QString line = QStringLiteral("set g_gametype \"%1\"").arg(gameType);
                if (gt.match(text).hasMatch())
                    text.replace(gt, line);
                else
                    text += QLatin1Char('\n') + line + QLatin1Char('\n');
                QRegularExpression wag(QStringLiteral("set\\s+xblive_wagermatch\\s+\"[^\"]*\""));
                const QString wline = QStringLiteral("set xblive_wagermatch \"%1\"").arg(wagerMatch ? QStringLiteral("1") : QStringLiteral("0"));
                if (wag.match(text).hasMatch())
                    text.replace(wag, wline);
                else
                    text += QLatin1Char('\n') + wline + QLatin1Char('\n');
            }
            QDir().mkpath(path);
            QFile file(path + configName + ".cfg");
            if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
                error = QObject::tr("Nao foi possivel escrever o arquivo de config em: %1").arg(file.fileName());
                return false;
            }
            QTextStream out(&file);
            out << text;
            return true;
        }
    }

    QDir().mkpath(path);
    QFile file(path + configName + ".cfg");
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        error = QObject::tr("Nao foi possivel escrever o arquivo de config em: %1").arg(file.fileName());
        return false;
    }
    QTextStream out(&file);
    out << cfg;
    file.close();
    return true;
}

} // namespace ConfigMaker
