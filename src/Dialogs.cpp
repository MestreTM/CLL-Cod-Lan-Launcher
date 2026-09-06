#include "Dialogs.h"
#include "AppSettings.h"

#include <QMessageBox>
#include <QWidget>
#include <QObject>

namespace {

QString messageFor(Dialogs::Msg kind, const AppSettings &s)
{
    switch (kind) {
    case Dialogs::Msg::Help:
        return QObject::tr(
            "A pasta se refere ao local onde o seu jogo esta instalado.\n"
            "Por exemplo, se ele estiver instalado em C:/games/pluto_t6_fullgame\n"
            "entao e isso que voce deve digitar ou procurar.");
    case Dialogs::Msg::Username:
        return QObject::tr("O nome de usuario informado e invalido.");
    case Dialogs::Msg::T4:
        return QObject::tr("A pasta informada \"%1\"\nnao contem dados validos de World at War.").arg(s.waw);
    case Dialogs::Msg::T5:
        return QObject::tr("A pasta informada \"%1\"\nnao contem dados validos de Black Ops.").arg(s.bo1);
    case Dialogs::Msg::T6:
        return QObject::tr("A pasta informada \"%1\"\nnao contem dados validos de Black Ops II.").arg(s.bo2);
    case Dialogs::Msg::IW5:
        return QObject::tr("A pasta informada \"%1\"\nnao contem dados validos de Modern Warfare 3.").arg(s.mw3);
    case Dialogs::Msg::Plutonium:
        return QObject::tr("A pasta informada \"%1\"\nnao contem uma instalacao valida do Plutonium.").arg(s.plutoniumInstance);
    case Dialogs::Msg::WrongGame:
        return QObject::tr("Voce selecionou um mod para %1,\nmas tentou lancar %2.").arg(s.modId, s.gameId);
    case Dialogs::Msg::WrongGameServer:
        return QObject::tr("Voce selecionou um mod para %1,\nmas tentou hospedar um servidor de %2.").arg(s.modId, s.serverId);
    case Dialogs::Msg::NoMod:
        return QObject::tr("Voce nao selecionou nenhum mod.");
    case Dialogs::Msg::NoCfg:
        return QObject::tr("Voce nao selecionou nenhuma config.");
    case Dialogs::Msg::UnsupportedExtension:
        return QObject::tr("O mod que voce tentou instalar tem uma extensao de arquivo nao suportada.");
    case Dialogs::Msg::NotStandardModFormat:
        return QObject::tr("O mod que voce tentou instalar nao usa a estrutura padrao de arquivos de mod,\ne nao foi instalado.");
    case Dialogs::Msg::FailedInstallMod:
        return QObject::tr("Falha ao instalar o mod.");
    case Dialogs::Msg::MultiConfigUnsupported:
        return QObject::tr("Criar configs para multiplayer nao e suportado no momento.\nAinda assim voce pode hospedar um servidor multiplayer criando uma config manualmente.");
    case Dialogs::Msg::CustomMapEmpty:
        return QObject::tr("Voce selecionou \"Outro\", mas deixou o campo de texto vazio.");
    case Dialogs::Msg::MustNameConfig:
        return QObject::tr("Voce precisa definir um nome para a config.");
    case Dialogs::Msg::DownloadedMainConfigs:
        return QObject::tr("Arquivos de configuracao principais do Black Ops II baixados com sucesso.");
    case Dialogs::Msg::DownloadedT5Configs:
        return QObject::tr("Arquivos de configuracao do Black Ops (T5) baixados com sucesso.");
    case Dialogs::Msg::DownloadedT4Configs:
        return QObject::tr("Arquivos de configuracao do World at War (T4) baixados com sucesso.");
    case Dialogs::Msg::SevenZipSuccess:
        return QObject::tr("7-Zip baixado e instalado com sucesso.");
    case Dialogs::Msg::SevenZipFailDownload:
        return QObject::tr("Nao foi possivel baixar o 7-Zip.");
    case Dialogs::Msg::SevenZipFailInstall:
        return QObject::tr("Nao foi possivel instalar o 7-Zip.");
    case Dialogs::Msg::UpdateCheckFailed:
        return QObject::tr("Nao foi possivel verificar a versao mais recente.");
    case Dialogs::Msg::NoUpdateAvailable:
        return QObject::tr("Nao ha atualizacoes disponiveis no momento.");
    }
    return QString();
}

} // namespace

void Dialogs::info(QWidget *parent, Msg kind, const AppSettings &settings)
{
    const QString text = messageFor(kind, settings);
    QMessageBox box(parent);
    box.setWindowTitle("Cod Lan Launcher");
    box.setIcon(QMessageBox::Information);
    box.setText(text);
    box.setStandardButtons(QMessageBox::Ok);
    box.exec();
}

bool Dialogs::confirmDownload7z(QWidget *parent)
{
    QMessageBox box(parent);
    box.setWindowTitle("Cod Lan Launcher");
    box.setIcon(QMessageBox::Question);
    box.setText(QObject::tr(
        "Voce ainda nao tem o 7-Zip instalado na pasta do Cod Lan Launcher,\n"
        "e nao pode instalar mods compactados por essa interface.\n"
        "Deseja baixar o 7-Zip agora?\n(Requer conexao com a internet)"));
    box.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
    box.setDefaultButton(QMessageBox::Yes);
    return box.exec() == QMessageBox::Yes;
}

bool Dialogs::confirmDownloadGameSettings(QWidget *parent)
{
    QMessageBox box(parent);
    box.setWindowTitle("Cod Lan Launcher");
    box.setIcon(QMessageBox::Question);
    box.setText(QObject::tr(
        "Voce ainda nao tem os arquivos de configuracao principais necessarios\n"
        "para hospedar corretamente um servidor de Black Ops II.\n"
        "Deseja baixa-los agora? (Requer conexao com a internet)"));
    box.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
    box.setDefaultButton(QMessageBox::Yes);
    return box.exec() == QMessageBox::Yes;
}

bool Dialogs::confirmDownloadT5Settings(QWidget *parent)
{
    QMessageBox box(parent);
    box.setWindowTitle("Cod Lan Launcher");
    box.setIcon(QMessageBox::Question);
    box.setText(QObject::tr(
        "Voce ainda nao tem dedicated.cfg / dedicated_sp.cfg do Black Ops (T5).\n"
        "Deseja baixa-los agora do xerxes-at/T5ServerConfig?\n"
        "(Requer conexao com a internet)"));
    box.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
    box.setDefaultButton(QMessageBox::Yes);
    return box.exec() == QMessageBox::Yes;
}

bool Dialogs::confirmDownloadT4Settings(QWidget *parent)
{
    QMessageBox box(parent);
    box.setWindowTitle("Cod Lan Launcher");
    box.setIcon(QMessageBox::Question);
    box.setText(QObject::tr(
        "Voce ainda nao tem server.cfg / server_zm.cfg do World at War (T4).\n"
        "Deseja baixa-los agora do xerxes-at/T4ServerConfigs?\n"
        "(Requer conexao com a internet)"));
    box.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
    box.setDefaultButton(QMessageBox::Yes);
    return box.exec() == QMessageBox::Yes;
}

bool Dialogs::confirmUpdate(QWidget *parent, const QString &newVersion)
{
    QMessageBox box(parent);
    box.setWindowTitle("Cod Lan Launcher");
    box.setIcon(QMessageBox::Question);
    box.setText(QObject::tr("Ha uma atualizacao disponivel,\ndeseja atualizar para a versao %1?").arg(newVersion));
    box.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
    box.setDefaultButton(QMessageBox::Yes);
    return box.exec() == QMessageBox::Yes;
}

void Dialogs::error(QWidget *parent, const QString &message)
{
    QMessageBox box(parent);
    box.setWindowTitle("Cod Lan Launcher");
    box.setIcon(QMessageBox::Warning);
    box.setText(message);
    box.setStandardButtons(QMessageBox::Ok);
    box.exec();
}
