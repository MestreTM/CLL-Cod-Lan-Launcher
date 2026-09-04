# Cod Lan Launcher (CLL)

Launcher offline para jogar **Call of Duty** via [Plutonium](https://plutonium.pw/) em LAN, sem depender de login ou do client oficial estar aberto.

Feito por **MestreTM**. A ideia original vem do LanLauncher do [JugAndDoubleTap](https://github.com/JugAndDoubleTap/LanLauncher).

Cerca de **40% deste codigo foi vibecoded** — escrito com auxilio de IA e depois revisado, testado e ajustado na unha.

O codigo e aberto. O `.exe` pronto fica nas [Releases](../../releases), nao neste repositorio.

## Jogos

| Codigo | Jogo |
| --- | --- |
| T4 | Call of Duty: World at War |
| T5 | Call of Duty: Black Ops |
| T6 | Call of Duty: Black Ops II |
| IW5 | Call of Duty: Modern Warfare 3 |

## O que o programa faz

- Assistente na primeira abertura: idioma, Plutonium Portable ou instalacao ja existente, nickname e pastas dos jogos
- Deteccao automatica do Steam (registro + `libraryfolders.vdf`) e pastas comuns
- Lancar multiplayer ou zombies/solo, com botao de encerrar o processo
- Kit **Plutonium Portable** (`pu.dat`) baixado e extraido em `./pu`, sem precisar da instalacao oficial
- Tambem aceita o Plutonium que ja estiver em `%LOCALAPPDATA%\Plutonium`
- Mods e mapas: zip / rar / 7z / exe, inclusive pacotes mistos (`storage/` + `steam/`)
- Checkpoint do mod: desinstalar devolve os arquivos originais e apaga o backup
- Servidor LAN (beta): sobe e para o dedicated, edita configs, lista IPs da maquina (incluindo `127.0.0.1`) e mostra como conectar no jogo
- Quatro idiomas: English (padrao), Portugues, Espanol, Russkiy
- Um unico `LanLauncherQt.exe` — artes, icones, tema e traducoes vao embutidos

## Como usar

1. Baixe o executavel na pagina de **Releases**
2. Coloque o `.exe` numa pasta gravavel
3. Na primeira abertura escolha o idioma e o client (Portable ou o Plutonium ja instalado)
4. Marque os jogos que voce tem
5. Clique em **Start** / **Iniciar**

Para conectar num servidor LAN: no jogo aperte `` ` `` (abaixo do Esc) e digite `connect IP:porta`.

## Compilar

Precisa de Qt 6 (Widgets, Network, Concurrent, Svg) e CMake 3.16+.

Build estatico no Windows, com o prefixo em `Y:\QT\6.11.2-static`:

```bat
scripts\build-app.bat
```

O resultado fica em `dist-static\LanLauncherQt.exe`.

Build dinamico:

```bat
cmake -S . -B build -DCMAKE_PREFIX_PATH=C:\Qt\6.11.2\mingw_64
cmake --build build --config Release
```

## Traducoes

Os JSON em `resources/i18n/` entram no executavel pelo `resources.qrc`.

- A chave e o texto em portugues do `tr()` no C++
- `en.json`, `es.json`, `ru.json` traduzem
- `pt_BR.json` so acerta acentuacao

Para outro idioma: copie `en.json`, traduza os valores, liste o arquivo no `.qrc` e registre o codigo em `I18n::codes()`.

## Empacotar o kit Portable

`pack_pu.py` gera o `pu.dat` (nao vai no binario do launcher):

```bat
python pack_pu.py
```

Formato: magic `LLQTPKG1` + tamanho + 7z com XOR.

## Estrutura

```
src/                 codigo
src/pages/           telas (jogar, mods, servidor, ajustes, sobre)
resources/           icones, artes, tema, idiomas
scripts/             build estatico
pack_pu.py           gera pu.dat
```

## Creditos

- **MestreTM** — este launcher
- **[JugAndDoubleTap](https://github.com/JugAndDoubleTap/LanLauncher)** — LanLauncher original em Python
- **Plutonium** e **Call of Duty** pertencem aos respectivos donos. Este projeto nao e afiliado a eles.

## Licenca

LGPL-3.0. Veja [LICENSE](LICENSE).
