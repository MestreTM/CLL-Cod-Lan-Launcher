#include "ArchiveTool.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QObject>
#include <QProcess>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <cstring>

namespace {

bool runProcess(const QString &program, const QStringList &args, const QString &workDir)
{
    QProcess proc;
    proc.setWorkingDirectory(workDir);
    proc.start(program, args);
    if (!proc.waitForStarted(5000))
        return false;
    proc.waitForFinished(-1);
    return proc.exitStatus() == QProcess::NormalExit && proc.exitCode() == 0;
}

QString sevenZipOutArg(const QString &dir)
{
    const QString abs = QFileInfo(dir).absoluteFilePath();
    QDir().mkpath(abs);
    return QStringLiteral("-o") + QDir::toNativeSeparators(abs);
}

// Fixed obfuscation key. NOT real encryption — anyone who dumps the exe
// will find it. It only stops generic tools from recognizing the .dat.
constexpr quint8 kXorKey[] = { 0x4C, 0x4C, 0x51, 0x54, 0x21, 0x9F, 0x3D, 0x7B };
constexpr char kMagic[8] = { 'L','L','Q','T','P','K','G','1' };

QByteArray xorTransform(const QByteArray &data)
{
    QByteArray out(data);
    for (int i = 0; i < out.size(); ++i)
        out[i] = static_cast<char>(static_cast<quint8>(out[i]) ^ kXorKey[i % sizeof(kXorKey)]);
    return out;
}

bool dirHasMatching(const QString &dirPath, const QString &pattern)
{
    QDir dir(dirPath);
    if (!dir.exists())
        return false;
    return !dir.entryList(QStringList() << pattern, QDir::Files).isEmpty();
}

// True if a direct subdirectory of dirPath contains a file matching pattern.
bool anySubdirHasMatching(const QString &dirPath, const QString &pattern)
{
    QDir dir(dirPath);
    if (!dir.exists())
        return false;
    const QFileInfoList subdirs = dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QFileInfo &sub : subdirs) {
        if (dirHasMatching(sub.absoluteFilePath(), pattern))
            return true;
    }
    return false;
}

} // namespace

namespace ArchiveTool {

QString findSevenZip()
{
    // next to the executable (downloaded from the 7-Zip prompt).
    const QString bundled = QDir(QCoreApplication::applicationDirPath()).filePath("7z/7z.exe");
    if (QFileInfo::exists(bundled))
        return bundled;

#ifdef Q_OS_WIN
    const QStringList winPaths = {
        "C:/Program Files/7-Zip/7z.exe",
        "C:/Program Files (x86)/7-Zip/7z.exe",
    };
    for (const QString &p : winPaths) {
        if (QFileInfo::exists(p))
            return p;
    }
#endif

#ifndef Q_OS_WIN
    // On Linux/macOS, try a system 7-Zip (p7zip / 7-zip).
    for (const char *name : {"7z", "7zz", "7za"}) {
        const QString found = QStandardPaths::findExecutable(name);
        if (!found.isEmpty())
            return found;
    }
#endif
    return QString();
}

bool hasSevenZip()
{
    return !findSevenZip().isEmpty();
}

bool extractToDirectory(const QString &archivePath, const QString &destDir, QString *errorOut,
                            const std::function<void(int percent)> &onProgress)
{
    if (!QFileInfo::exists(archivePath)) {
        if (errorOut)
            *errorOut = QObject::tr("Arquivo nao encontrado: %1").arg(archivePath);
        return false;
    }
    QDir().mkpath(destDir);
    const QString sevenZip = findSevenZip();
    if (sevenZip.isEmpty()) {
        if (errorOut)
            *errorOut = QObject::tr("7-Zip nao encontrado.");
        return false;
    }

    const QString absDest = QDir::toNativeSeparators(QFileInfo(destDir).absoluteFilePath());
    const QString nativeArchive = QDir::toNativeSeparators(QFileInfo(archivePath).absoluteFilePath());
    QDir().mkpath(QFileInfo(destDir).absoluteFilePath());
    QProcess proc;
    proc.setWorkingDirectory(QFileInfo(sevenZip).absolutePath());
    proc.setProcessChannelMode(QProcess::MergedChannels);
    proc.start(sevenZip, {"x", "-y", "-aoa", "-bb0", "-bsp1", "-o" + absDest, nativeArchive});
    if (!proc.waitForStarted(8000)) {
        if (errorOut)
            *errorOut = QObject::tr("Nao foi possivel iniciar o 7-Zip.");
        return false;
    }
    QRegularExpression pctRe(QStringLiteral("(\\d{1,3})%"));
    QString acc;
    while (!proc.waitForFinished(120)) {
        acc += QString::fromLocal8Bit(proc.readAll());
        if (onProgress) {
            auto it = pctRe.globalMatch(acc);
            int last = -1;
            while (it.hasNext())
                last = it.next().captured(1).toInt();
            if (last >= 0)
                onProgress(qBound(0, last, 99));
        }
        if (acc.size() > 8000)
            acc = acc.right(2000);
    }
    acc += QString::fromLocal8Bit(proc.readAll());
    if (onProgress)
        onProgress(100);
    const int code = proc.exitCode();
    if (proc.exitStatus() != QProcess::NormalExit || (code != 0 && code != 1)) {
        if (errorOut) {
            const QString err = acc.right(240);
            *errorOut = QObject::tr("7-Zip falhou (codigo %1). %2").arg(code).arg(err);
        }
        return false;
    }
    return true;
}

QStringList listEntries(const QString &archivePath)
{
    QStringList out;
    const QString sevenZip = findSevenZip();
    if (sevenZip.isEmpty() || !QFileInfo::exists(archivePath))
        return out;
    QProcess proc;
    proc.start(sevenZip, {"l", "-ba",
                          QDir::toNativeSeparators(QFileInfo(archivePath).absoluteFilePath())});
    if (!proc.waitForStarted(8000))
        return out;
    proc.waitForFinished(-1);
    const QString text = QString::fromLocal8Bit(proc.readAllStandardOutput());
    for (QString line : text.split(QLatin1Char('\n'))) {
        line = line.trimmed();
        if (line.isEmpty())
            continue;
        const int sp = line.lastIndexOf(QLatin1Char(' '));
        const QString name = (sp >= 0) ? line.mid(sp + 1).trimmed() : line;
        if (!name.isEmpty())
            out << QString(name).replace(QLatin1Char('\\'), QLatin1Char('/'));
    }
    return out;
}


QList<ListedFile> listDetailed(const QString &archivePath)
{
    QList<ListedFile> out;
    const QString sevenZip = findSevenZip();
    if (sevenZip.isEmpty() || !QFileInfo::exists(archivePath))
        return out;
    QProcess proc;
    proc.start(sevenZip, {"l", "-slt",
                          QDir::toNativeSeparators(QFileInfo(archivePath).absoluteFilePath())});
    if (!proc.waitForStarted(8000))
        return out;
    proc.waitForFinished(-1);
    const QString text = QString::fromLocal8Bit(proc.readAllStandardOutput());
    ListedFile cur;
    auto flush = [&]() {
        if (cur.path.isEmpty())
            return;
        cur.path.replace(QLatin1Char('\\'), QLatin1Char('/'));
        while (cur.path.startsWith(QLatin1Char('/')))
            cur.path.remove(0, 1);
        out.push_back(cur);
        cur = ListedFile();
    };
    for (QString line : text.split(QLatin1Char('\n'))) {
        line = line.trimmed();
        if (line.isEmpty()) {
            flush();
            continue;
        }
        if (line.startsWith(QLatin1String("Path = ")))
            cur.path = line.mid(7).trimmed();
        else if (line.startsWith(QLatin1String("Size = ")))
            cur.size = line.mid(7).trimmed().toLongLong();
        else if (line.startsWith(QLatin1String("Attributes = ")))
            cur.isDir = line.contains(QLatin1Char('D'));
    }
    flush();
    return out;
}

bool extractPaths(const QString &archivePath, const QString &destDir,
                  const QStringList &innerPaths, QString *errorOut)
{
    const QString sevenZip = findSevenZip();
    if (sevenZip.isEmpty()) {
        if (errorOut) *errorOut = QObject::tr("7-Zip nao encontrado.");
        return false;
    }
    const QString absDest = QDir::toNativeSeparators(QFileInfo(destDir).absoluteFilePath());
    const QString nativeArchive = QDir::toNativeSeparators(QFileInfo(archivePath).absoluteFilePath());
    QDir().mkpath(QFileInfo(destDir).absoluteFilePath());
    QStringList args = {"x", "-y", "-aoa", "-bb0", "-r", "-o" + absDest, nativeArchive};
    args << innerPaths;
    QProcess proc;
    proc.setWorkingDirectory(QFileInfo(sevenZip).absolutePath());
    proc.start(sevenZip, args);
    if (!proc.waitForStarted(8000)) {
        if (errorOut) *errorOut = QObject::tr("Nao foi possivel iniciar o 7-Zip.");
        return false;
    }
    proc.waitForFinished(-1);
    const int code = proc.exitCode();
    if (proc.exitStatus() != QProcess::NormalExit || (code != 0 && code != 1)) {
        if (errorOut)
            *errorOut = QObject::tr("7-Zip falhou (codigo %1).").arg(code);
        return false;
    }
    return true;
}

ExtractResult extractArchive(const QString &modFolder, const QString &archivePath)
{
    const QFileInfo archiveInfo(archivePath);
    const QString ext = archiveInfo.suffix().toLower();

    QString fileName;
    if (ext == "7z" || ext == "zip" || ext == "rar" || ext == "exe" || ext == "cll") {
        // used to truncate the full path for .7z instead of the file name).
        // completeBaseName() does this correctly and portably.
        fileName = archiveInfo.completeBaseName();
    } else {
        return ExtractResult::UnsupportedExtension;
    }

    const QString sevenZip = findSevenZip();
    if (sevenZip.isEmpty())
        return ExtractResult::SevenZipMissing;

    const QString tempDir = modFolder + "/TEMP";
    QDir(tempDir).removeRecursively();
    if (!QDir().mkpath(tempDir))
        return ExtractResult::Failed;

    const QString workDir = QFileInfo(sevenZip).absolutePath();

    if (!runProcess(sevenZip, {"x", "-y", sevenZipOutArg(tempDir), archivePath}, workDir)) {
        QDir(tempDir).removeRecursively();
        return ExtractResult::Failed;
    }

    const bool rootModFormat =
        QFile::exists(tempDir + "/mod.ff") ||
        QDir(tempDir + "/scripts").exists() ||
        QDir(tempDir + "/images").exists() ||
        QDir(tempDir + "/maps").exists();

    if (rootModFormat) {
        // if glob.glob(.../TEMP/*.lua) or glob.glob(.../TEMP/*.gsc)
        if (dirHasMatching(tempDir, "*.lua") || dirHasMatching(tempDir, "*.gsc")) {
            QDir(tempDir).removeRecursively();
            return ExtractResult::NotStandardModFormat;
        }
        QDir(tempDir).removeRecursively();

        QString destSub = modFolder + "/" + fileName.replace(' ', '_');
        QDir().mkpath(destSub);
        if (!runProcess(sevenZip, {"x", "-y", sevenZipOutArg(destSub), archivePath}, workDir))
            return ExtractResult::Failed;

        QDir pluginsDir(modFolder + "/$PLUGINSDIR");
        if (pluginsDir.exists())
            pluginsDir.removeRecursively();
    } else {
        // if glob.glob(.../TEMP/*/*.lua) or glob.glob(.../TEMP/*/*.gsc)
        if (anySubdirHasMatching(tempDir, "*.lua") || anySubdirHasMatching(tempDir, "*.gsc")) {
            QDir(tempDir).removeRecursively();
            return ExtractResult::NotStandardModFormat;
        }
        QDir(tempDir).removeRecursively();

        if (!runProcess(sevenZip, {"x", "-y", sevenZipOutArg(modFolder), archivePath}, workDir))
            return ExtractResult::Failed;

        QDir pluginsDir(modFolder + "/$PLUGINSDIR");
        if (pluginsDir.exists())
            pluginsDir.removeRecursively();
    }

    return ExtractResult::Success;
}

bool extractBootstrapZip(const QString &zipPath, const QString &destDir)
{
    QDir().mkpath(destDir);
#ifdef Q_OS_WIN
    const QString program = "powershell";
    const QStringList args = {
        "-NoProfile", "-Command",
        QString("Expand-Archive -LiteralPath '%1' -DestinationPath '%2' -Force").arg(zipPath, destDir)
    };
#else
    const QString program = "unzip";
    const QStringList args = {"-o", zipPath, "-d", destDir};
#endif
    return runProcess(program, args, destDir);
}

bool extractObfuscatedBundle(const QString &datPath, const QString &destDir, QString *errorOut)
{
    QFile f(datPath);
    if (!f.open(QIODevice::ReadOnly)) {
        if (errorOut) *errorOut = QObject::tr("Nao foi possivel abrir %1").arg(datPath);
        return false;
    }

    const QByteArray header = f.read(12);
    if (header.size() != 12 || std::memcmp(header.constData(), kMagic, 8) != 0) {
        if (errorOut) *errorOut = QObject::tr("Arquivo nao e um bundle valido (cabecalho invalido)");
        return false;
    }
    quint32 payloadSize = 0;
    std::memcpy(&payloadSize, header.constData() + 8, 4);

    const QByteArray obfuscated = f.read(payloadSize);
    f.close();
    if (static_cast<quint32>(obfuscated.size()) != payloadSize) {
        if (errorOut) *errorOut = QObject::tr("Bundle truncado/corrompido");
        return false;
    }

    const QByteArray sevenZipBytes = xorTransform(obfuscated);

    QTemporaryDir tempDir;
    if (!tempDir.isValid()) {
        if (errorOut) *errorOut = QObject::tr("Falha ao criar pasta temporaria");
        return false;
    }
    const QString tempArchive = tempDir.path() + "/bundle.7z";
    QFile out(tempArchive);
    if (!out.open(QIODevice::WriteOnly) || out.write(sevenZipBytes) != sevenZipBytes.size()) {
        if (errorOut) *errorOut = QObject::tr("Falha ao escrever arquivo temporario");
        return false;
    }
    out.close();

    const bool ok = extractToDirectory(tempArchive, destDir, errorOut);
    QFile::remove(tempArchive);
    return ok;
}

} // namespace ArchiveTool
