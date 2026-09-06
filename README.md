# Cod Lan Launcher (CLL)
Offline launcher to play **Call of Duty** via [Plutonium](https://plutonium.pw/) on LAN, without needing to log in or have the official client open.

### Supported Langs:

![English](https://img.shields.io/badge/lang-English-blue)
![Português](https://img.shields.io/badge/lang-Portugu%C3%AAs-green)
![Español](https://img.shields.io/badge/lang-Espa%C3%B1ol-yellow)
![Русский](https://img.shields.io/badge/lang-%D0%A0%D1%83%D1%81%D1%81%D0%BA%D0%B8%D0%B9-red)

## [>> Download in Releases <<](https://github.com/MestreTM/CLL-Cod-Lan-Launcher/releases)

---

![Cod Lan Launcher](https://i.imgur.com/xHyfkQH.png)

Made by **MestreTM**. The original idea comes from LanLauncher by [JugAndDoubleTap](https://github.com/JugAndDoubleTap/LanLauncher).

Repository: [github.com/MestreTM/CLL-CodLanLauncher](https://github.com/MestreTM/CLL-CodLanLauncher)

About **40% of this code was vibecoded** — written with AI assistance and then reviewed, tested, and fine-tuned by hand.

The code is open source. The ready-to-use `.exe` is available in the [Releases](../../releases), not in this repository.

## Games

| Code | Game |
| --- | --- |
| T4 | Call of Duty: World at War |
| T5 | Call of Duty: Black Ops |
| T6 | Call of Duty: Black Ops II |
| IW5 | Call of Duty: Modern Warfare 3 |

## What the program does

- First-launch wizard: language, Plutonium Portable or existing installation, nickname, and game folders
- Automatic Steam detection (registry + `libraryfolders.vdf`) and common folders
- Launch multiplayer or zombies/solo, with a button to kill the process
- **Plutonium Portable** kit (`pu.dat`) downloaded and extracted into `./pu`, no official installation required
- Also accepts an existing Plutonium install found in `%LOCALAPPDATA%\Plutonium`
- Mods and maps: zip / rar / 7z / exe / `.cll`, including mixed packages (`storage/` + `steam/`)
- Mod checkpoint: uninstalling restores the original files and deletes the backup
- LAN server (beta): starts and stops the dedicated server, edits configs, lists the machine's IPs (including `127.0.0.1`), and shows how to connect in-game
- Server config packs for T4 / T5 / T6 by [xerxes-at](https://github.com/xerxes-at)
- Four languages: English (default), Portuguese, Spanish, Russian
- A single `LanLauncherQt.exe` — art, icons, theme, and translations are all embedded

## Mods

The launcher can install maps and mods without copying files by hand.

### Generic packages

Drop a `.zip`, `.rar`, `.7z` or `.exe` on the **Mods** tab (or pick it with the file button).

If the archive has no CLL manifest, the path is loaded and the usual installer runs. Mixed packs that contain both `storage/` (Plutonium) and `steam/` (game folder) are sorted automatically. You confirm the plan and choose whether replaced files become a backup.

### CLL packs (`.cll` or a zip with `cll_installer.json`)

A **CLL** pack is a zip / rar / 7z that includes `cll_installer.json`. Renaming that archive to `.cll` is optional — the launcher also detects the JSON inside a normal zip.

The manifest tells the installer where each folder goes:

| Field | Meaning |
| --- | --- |
| `name`, `version`, `author`, `description` | Shown in the install dialog |
| `game` | Target title (`t4`, `t5`, `t6`, `iw5`) |
| `folders[].from` | Folder inside the archive |
| `folders[].to` | `pu_folder` (Plutonium / `./pu`) or `game_folder` (Steam/game install) |
| `folders[].dest` | Relative destination under that root |

Example:

```json
{
  "name": "Zombies Declassified Beta 1",
  "version": "1.0",
  "author": "Logo2k",
  "description": "A pc port of the cancelled DLC5 Expansion",
  "game": "t6",
  "folders": [
    { "from": "ZombiesDeclassified_BETA1/storage", "to": "pu_folder", "dest": "storage" },
    { "from": "ZombiesDeclassified_BETA1/steam/zone", "to": "game_folder", "dest": "zone" }
  ]
}
```

Drag the file onto **Mods**: the correct game is selected, a card shows name / author / version / description, and **Install** / **Cancel** run with a progress bar.

A sample manifest lives in `docs/cll_installer.example.json`.

### Uninstall and checkpoints

Every install writes a checkpoint of added and replaced files. Removing the mod from the manager:

1. Deletes files the pack created
2. Restores anything it overwrote
3. Drops the backup folder when that finishes

The list shows the **name from the JSON** when the pack is a CLL install, not the raw folder name.

## How to use

1. Download the executable from the **Releases** page
2. Put the `.exe` in a writable folder
3. On first launch, choose the language and client (Portable or an already installed Plutonium)
4. Check the games you own
5. Click **Start**

To connect to a LAN server: in-game, press `` ` `` (below Esc) and type `connect IP:port`.

## Building

Requires Qt 6 (Widgets, Network, Concurrent, Svg) and CMake 3.16+.

Static build on Windows, with the prefix at `Y:\QT\6.11.2-static`:

```bat
scripts\build-app.bat
```

The result is placed in `dist-static\LanLauncherQt.exe`.

Dynamic build:

```bat
cmake -S . -B build -DCMAKE_PREFIX_PATH=C:\Qt\6.11.2\mingw_64
cmake --build build --config Release
```

## Translations

The JSON files in `resources/i18n/` are embedded in the executable via `resources.qrc`.

- The key is the Portuguese text from `tr()` in the C++ code
- `en.json`, `es.json`, `ru.json` translate it
- `pt_BR.json` only fixes accentuation

To add another language: copy `en.json`, translate the values, list the file in the `.qrc`, and register the code in `I18n::codes()`.

## Packaging the Portable kit

`pack_pu.py` generates `pu.dat` (not included in the launcher binary):

```bat
python pack_pu.py
```

Format: magic `LLQTPKG1` + size + 7z with XOR.

## Structure

```
src/           code
src/pages/     screens (play, mods, server, settings, about)
resources/     icons, art, theme, languages
scripts/       static build
pack_pu.py     generates pu.dat
```

## Credits

- **MestreTM** — this launcher
- **[JugAndDoubleTap](https://github.com/JugAndDoubleTap/LanLauncher)** — original LanLauncher in Python
- **[xerxes-at](https://github.com/xerxes-at)** — T4 / T5 / T6 dedicated server config files
- **Plutonium** and **Call of Duty** belong to their respective owners. This project is not affiliated with them.

## License

LGPL-3.0. See [LICENSE](LICENSE).
