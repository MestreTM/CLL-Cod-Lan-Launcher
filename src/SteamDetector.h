#pragma once
#include <QString>
#include <QStringList>

// Finds Steam installs and common folders for the supported games.
namespace SteamDetector
{
    struct GameHit {
        QString gameId;   // "World at War" | "Black ops" | "Black ops II" | "Modern Warfare 3"
        QString path;
    };

    // Steam root from the registry (Windows) or default paths.
    QString steamInstallPath();

    // Every library listed in libraryfolders.vdf plus the main folder.
    QStringList steamLibraryPaths();

    // Common folders besides Steam (C:/Games, D:/Games, etc.).
    QStringList commonGameRoots();

    // Looks for the four games in Steam libraries and common folders.
    QList<GameHit> detectInstalledGames();

    // Folder checks (same markers GameLauncher uses).
    bool isValidWaw(const QString &dir);
    bool isValidBo1(const QString &dir);
    bool isValidBo2(const QString &dir);
    bool isValidMw3(const QString &dir);
}
