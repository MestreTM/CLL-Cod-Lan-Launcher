#include "MakeConfigDialog.h"
#include "AppSettings.h"
#include "ConfigMaker.h"
#include "Dialogs.h"

#include <QButtonGroup>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QComboBox>
#include <QScrollArea>
#include <QWidget>
#include <QRadioButton>
#include <QVBoxLayout>

MakeConfigDialog::MakeConfigDialog(AppSettings &settings, const QString &serverId,
                                     bool multiplayer, QWidget *parent)
    : QDialog(parent)
    , m_settings(settings)
    , m_serverId(serverId)
    , m_multiplayer(multiplayer)
{
    setWindowTitle("Cod Lan Launcher");
    m_mapGroup = new QButtonGroup(this);
    setMinimumWidth(520);

    auto *root = new QVBoxLayout(this);

    auto *mapsHost = new QWidget(this);
    auto *mapsLayout = new QVBoxLayout(mapsHost);
    mapsLayout->setContentsMargins(0, 0, 0, 0);
    if (serverId == "World at War" && multiplayer)
        buildWawMpMaps(mapsLayout);
    else if (serverId == "World at War")
        buildWawMaps(mapsLayout);
    else if (serverId == "Black ops" && multiplayer)
        buildBo1MpMaps(mapsLayout);
    else if (serverId == "Black ops")
        buildBo1Maps(mapsLayout);
    else if (serverId == "Black ops II" && multiplayer)
        buildBo2MpMaps(mapsLayout);
    else
        buildBo2Maps(mapsLayout);
    auto *scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setWidget(mapsHost);
    scroll->setMinimumHeight(280);
    root->addWidget(scroll);

    auto *nameLabel = new QLabel(tr("Nome da config, sem sufixo ou prefixo\nEx.: DieRise"), this);
    nameLabel->setStyleSheet("font-size:8.5pt; color:#9c9da4;");
    root->addWidget(nameLabel);

    m_nameEdit = new QLineEdit(this);
    root->addWidget(m_nameEdit);

    auto *btnRow = new QHBoxLayout();
    auto *closeBtn = new QPushButton(tr("Fechar"), this);
    auto *makeBtn = new QPushButton(tr("Fazer config"), this);
    makeBtn->setProperty("cssClass", "primary");
    btnRow->addStretch();
    btnRow->addWidget(closeBtn);
    btnRow->addWidget(makeBtn);
    root->addLayout(btnRow);

    connect(closeBtn, &QPushButton::clicked, this, &QDialog::reject);
    connect(makeBtn, &QPushButton::clicked, this, &MakeConfigDialog::onMakeConfig);
}


void MakeConfigDialog::buildWawMpMaps(QVBoxLayout *into)
{
    into->addWidget(new QLabel(tr("Modo"), this));
    m_gameTypeCombo = new QComboBox(this);
    struct Mode { QString code; QString label; };
    const QList<Mode> modes = {
        {"tdm", tr("Team Deathmatch")}, {"dm", tr("Free For All")},
        {"dom", tr("Domination")}, {"sab", tr("Sabotage")},
        {"sd", tr("Search & Destroy")}, {"ctf", tr("Capture the Flag")},
        {"koth", tr("Headquarters")}, {"twar", tr("War")},
    };
    for (const Mode &m : modes)
        m_gameTypeCombo->addItem(m.label, QStringList{m.code, QStringLiteral("0")});
    into->addWidget(m_gameTypeCombo);

    into->addWidget(new QLabel(tr("Selecionar mapa"), this));
    struct Entry { QString key; QString label; };
    const QList<Entry> maps = {
        {"mpairfield","Airfield"},{"mpasylum","Asylum"},{"mpkwai","Banzai"},
        {"mpdrum","Battery"},{"mpbgate","Breach"},{"mpcastle","Castle"},
        {"mpshrine","Cliffside"},{"mpstalingrad","Corrosion"},{"mpcourtyard","Courtyard"},
        {"mpdome","Dome"},{"mpdownfall","Downfall"},{"mphangar","Hangar"},
        {"mpkneedeep","Knee Deep"},{"mpmakin","Makin"},{"mpmakinday","Makin (Day)"},
        {"mpnachtfeuer","Nightfire"},{"mpoutskirts","Outskirts"},{"mpvodka","Revolution"},
        {"mproundhouse","Roundhouse"},{"mpseelow","Seelow"},{"mpsubway","Station"},
        {"mpdocks","Sub Pens"},{"mpsuburban","Upheaval"},{"mapother","Outro"},
    };
    for (const Entry &e : maps) {
        auto *radio = new QRadioButton(e.label, this);
        m_mapGroup->addButton(radio);
        m_radiosByKey.insert(e.key, radio);
        into->addWidget(radio);
    }
    m_radiosByKey["mpcastle"]->setChecked(true);
    auto *hint = new QLabel(tr("Digite o nome do mapa abaixo para mapas customizados\n(\"Outro\" deve estar selecionado)"), this);
    hint->setStyleSheet("font-size:8pt; font-weight:600; color:#9c9da4;");
    into->addWidget(hint);
    if (!m_customMapEdit)
        m_customMapEdit = new QLineEdit(this);
    into->addWidget(m_customMapEdit);
}

void MakeConfigDialog::buildBo1MpMaps(QVBoxLayout *into)
{
    into->addWidget(new QLabel(tr("Modo"), this));
    m_gameTypeCombo = new QComboBox(this);
    struct Mode { QString code; QString label; bool wager; };
    const QList<Mode> modes = {
        {"tdm",  tr("Team Deathmatch"), false},
        {"dm",   tr("Free For All"), false},
        {"dom",  tr("Domination"), false},
        {"dem",  tr("Demolition"), false},
        {"sd",   tr("Search & Destroy"), false},
        {"sab",  tr("Sabotage"), false},
        {"koth", tr("Headquarters"), false},
        {"ctf",  tr("Capture the Flag"), false},
        {"oic",  tr("One in the Chamber"), true},
        {"gun",  tr("Gun Game"), true},
        {"shrp", tr("Sharpshooter"), true},
        {"hlnd", tr("Sticks and Stones"), true},
    };
    for (const Mode &m : modes)
        m_gameTypeCombo->addItem(m.label, QStringList{m.code, m.wager ? QStringLiteral("1") : QStringLiteral("0")});
    into->addWidget(m_gameTypeCombo);

    into->addWidget(new QLabel(tr("Selecionar mapa"), this));
    struct Entry { QString key; QString label; };
    const QList<Entry> maps = {
        {"mparray", "Array"}, {"mpcracked", "Cracked"}, {"mpcrisis", "Crisis"},
        {"mpfiring", "Firing Range"}, {"mpduga", "Grid"}, {"mphanoi", "Hanoi"},
        {"mpcairo", "Havana"}, {"mphavoc", "Jungle"}, {"mpcosmo", "Launch"},
        {"mpnuked", "Nuketown"}, {"mprad", "Radiation"}, {"mpmountain", "Summit"},
        {"mpvilla", "Villa"}, {"mprussian", "WMD"},
        {"mpberlin", "Berlin Wall"}, {"mpdisc", "Discovery"}, {"mpkowloon", "Kowloon"},
        {"mpstadium", "Stadium"}, {"mpgridlock", "Convoy"}, {"mphotel", "Hotel"},
        {"mpoutskirts", "Stockpile"}, {"mpzoo", "Zoo"}, {"mpdrivein", "Drive-in"},
        {"mparea51", "Hangar 18"}, {"mpgolf", "Hazard"}, {"mpsilo", "Silo"},
        {"mapother", "Outro"},
    };
    for (const Entry &e : maps) {
        auto *radio = new QRadioButton(e.label, this);
        m_mapGroup->addButton(radio);
        m_radiosByKey.insert(e.key, radio);
        into->addWidget(radio);
    }
    m_radiosByKey["mpnuked"]->setChecked(true);
    auto *hint = new QLabel(tr("Digite o nome do mapa abaixo para mapas customizados\n(\"Outro\" deve estar selecionado)"), this);
    hint->setStyleSheet("font-size:8pt; font-weight:600; color:#9c9da4;");
    into->addWidget(hint);
    if (!m_customMapEdit)
        m_customMapEdit = new QLineEdit(this);
    into->addWidget(m_customMapEdit);
}

void MakeConfigDialog::buildBo2MpMaps(QVBoxLayout *into)
{
    into->addWidget(new QLabel(tr("Modo"), this));
    m_gameTypeCombo = new QComboBox(this);
    struct Mode { QString code; QString label; };
    const QList<Mode> modes = {
        {"tdm", tr("Team Deathmatch")}, {"dm", tr("Free For All")},
        {"dom", tr("Domination")}, {"dem", tr("Demolition")},
        {"sd", tr("Search & Destroy")}, {"ctf", tr("Capture the Flag")},
        {"koth", tr("Headquarters")}, {"hq", tr("Headquarters")},
        {"gun", tr("Gun Game")}, {"oic", tr("One in the Chamber")},
        {"shrp", tr("Sharpshooter")}, {"sas", tr("Sticks and Stones")},
    };
    for (const Mode &m : modes)
        m_gameTypeCombo->addItem(m.label, QStringList{m.code, QStringLiteral("0")});
    into->addWidget(m_gameTypeCombo);

    into->addWidget(new QLabel(tr("Selecionar mapa"), this));
    struct Entry { QString key; QString label; };
    const QList<Entry> maps = {
        {"mpla","Aftermath"},{"mpdockside","Cargo"},{"mpcarrier","Carrier"},
        {"mpdrone","Drone"},{"mpexpress","Express"},{"mphijacked","Hijacked"},
        {"mpmeltdown","Meltdown"},{"mpoverflow","Overflow"},{"mpnightclub","Plaza"},
        {"mpraid","Raid"},{"mpslums","Slums"},{"mpvillage","Standoff"},
        {"mpturbine","Turbine"},{"mpsocotra","Yemen"},{"mpnuketown","Nuketown 2025"},
        {"mpdownhill","Downhill"},{"mpmirage","Mirage"},{"mphydro","Hydro"},
        {"mpskate","Grind"},{"mpconcert","Encore"},{"mpmagma","Magma"},
        {"mpvertigo","Vertigo"},{"mpstudio","Studio"},{"mpuplink","Uplink"},
        {"mpbridge","Detour"},{"mpcastaway","Cove"},{"mppaintball","Rush"},
        {"mpdig","Dig"},{"mpfrostbite","Frost"},{"mppod","Pod"},
        {"mptakeoff","Takeoff"},{"mapother","Outro"},
    };
    for (const Entry &e : maps) {
        auto *radio = new QRadioButton(e.label, this);
        m_mapGroup->addButton(radio);
        m_radiosByKey.insert(e.key, radio);
        into->addWidget(radio);
    }
    m_radiosByKey["mphijacked"]->setChecked(true);
    auto *hint = new QLabel(tr("Digite o nome do mapa abaixo para mapas customizados\n(\"Outro\" deve estar selecionado)"), this);
    hint->setStyleSheet("font-size:8pt; font-weight:600; color:#9c9da4;");
    into->addWidget(hint);
    if (!m_customMapEdit)
        m_customMapEdit = new QLineEdit(this);
    into->addWidget(m_customMapEdit);
}

void MakeConfigDialog::buildWawMaps(QVBoxLayout *into)
{
    into->addWidget(new QLabel(tr("Selecionar mapa"), this));

    struct Entry { QString key; QString label; };
    const QList<Entry> entries = {
        {"mapnacht", "Nacht der Untoten"},
        {"mapverru", "Verruckt"},
        {"mapshi",   "Shi No Numa"},
        {"mapder",   "Der Riese"},
        {"mapother", "Outro"},
    };
    for (const Entry &e : entries) {
        auto *radio = new QRadioButton(e.label, this);
        m_mapGroup->addButton(radio);
        m_radiosByKey.insert(e.key, radio);
        into->addWidget(radio);
    }
    m_radiosByKey["mapnacht"]->setChecked(true);

    auto *hint = new QLabel(tr("Digite o nome do mapa abaixo para mapas customizados\n(\"Outro\" deve estar selecionado)"), this);
    hint->setStyleSheet("font-size:8pt; font-weight:600; color:#9c9da4;");
    into->addWidget(hint);

    m_customMapEdit = new QLineEdit(this);
    into->addWidget(m_customMapEdit);
}

void MakeConfigDialog::buildBo1Maps(QVBoxLayout *into)
{
    // T5 config generation was never implemented in the original launcher
    // (the map radios were drawn, selectedMap was not). Fail instead of crashing.
    into->addWidget(new QLabel(tr("Selecionar mapa"), this));
    struct Entry { QString key; QString label; };
    const QList<Entry> entries = {
        {"mapkino", "Kino der Toten"}, {"mapfive", "Five"}, {"mapasc", "Ascension"},
        {"mapcall", "Call of the Dead"}, {"mapshangri", "Shangri-La"}, {"mapmoon", "Moon"},
        {"mapops", "Dead ops Arcade"}, {"mapnacht", "Nacht der Untoten"},
        {"mapverru", "Verruckt"}, {"mapshi", "Shi No Numa"}, {"mapder", "Der Riese"},
    };
    for (const Entry &e : entries) {
        auto *radio = new QRadioButton(e.label, this);
        m_mapGroup->addButton(radio);
        m_radiosByKey.insert(e.key, radio);
        into->addWidget(radio);
    }
    m_radiosByKey["mapkino"]->setChecked(true);

    auto *other = new QRadioButton(QStringLiteral("Outro"), this);
    m_mapGroup->addButton(other);
    m_radiosByKey.insert(QStringLiteral("mapother"), other);
    into->addWidget(other);

    auto *hint = new QLabel(tr("Digite o nome do mapa abaixo para mapas customizados\n(\"Outro\" deve estar selecionado)"), this);
    hint->setStyleSheet("font-size:8pt; font-weight:600; color:#9c9da4;");
    into->addWidget(hint);
    if (!m_customMapEdit)
        m_customMapEdit = new QLineEdit(this);
    into->addWidget(m_customMapEdit);
}

void MakeConfigDialog::buildBo2Maps(QVBoxLayout *into)
{
    auto *row = new QHBoxLayout();

    auto *col1 = new QVBoxLayout();
    col1->addWidget(new QLabel(tr("Selecionar mapa"), this));
    struct Entry { QString key; QString label; };
    const QList<Entry> solo = {
        {"mapbus",     "Tranzit"},
        {"mapnuke",    "Nuke Town"},
        {"maprise",    "Die Rise"},
        {"mapmob",     "Mob of the Dead"},
        {"mapburied",  "Buried"},
        {"maporigins", "Origins"},
    };
    for (const Entry &e : solo) {
        auto *radio = new QRadioButton(e.label, this);
        m_mapGroup->addButton(radio);
        m_radiosByKey.insert(e.key, radio);
        col1->addWidget(radio);
    }
    m_radiosByKey["mapbus"]->setChecked(true);
    row->addLayout(col1);

    auto *col2 = new QVBoxLayout();
    col2->addWidget(new QLabel(" ", this));
    const QList<Entry> survival = {
        {"mapfarm",  "Farm Survival"},
        {"maptown",  "Town Survival"},
        {"mapdepot", "Bus Depot Survival"},
    };
    for (const Entry &e : survival) {
        auto *radio = new QRadioButton(e.label, this);
        m_mapGroup->addButton(radio);
        m_radiosByKey.insert(e.key, radio);
        col2->addWidget(radio);
    }
    row->addLayout(col2);

    into->addLayout(row);
}

void MakeConfigDialog::onMakeConfig()
{
    bool abort = false;
    const QString configName = m_nameEdit->text();

    if (configName.trimmed().isEmpty()) {
        Dialogs::info(this, Dialogs::Msg::MustNameConfig, m_settings);
        abort = true;
    }

    QString selectedMap;

    if (m_serverId == "World at War" && m_multiplayer) {
        static const QHash<QString, QString> mp = {
            {"mpairfield","map mp_airfield"},{"mpasylum","map mp_asylum"},
            {"mpkwai","map mp_kwai"},{"mpdrum","map mp_drum"},{"mpbgate","map mp_bgate"},
            {"mpcastle","map mp_castle"},{"mpshrine","map mp_shrine"},
            {"mpstalingrad","map mp_stalingrad"},{"mpcourtyard","map mp_courtyard"},
            {"mpdome","map mp_dome"},{"mpdownfall","map mp_downfall"},
            {"mphangar","map mp_hangar"},{"mpkneedeep","map mp_kneedeep"},
            {"mpmakin","map mp_makin"},{"mpmakinday","map mp_makin_day"},
            {"mpnachtfeuer","map mp_nachtfeuer"},{"mpoutskirts","map mp_outskirts"},
            {"mpvodka","map mp_vodka"},{"mproundhouse","map mp_roundhouse"},
            {"mpseelow","map mp_seelow"},{"mpsubway","map mp_subway"},
            {"mpdocks","map mp_docks"},{"mpsuburban","map mp_suburban"},
        };
        QString mapTok;
        for (auto it = mp.begin(); it != mp.end(); ++it) {
            if (m_radiosByKey.value(it.key()) && m_radiosByKey.value(it.key())->isChecked()) {
                mapTok = it.value(); break;
            }
        }
        if (mapTok.isEmpty() && m_radiosByKey.value("mapother") && m_radiosByKey.value("mapother")->isChecked()) {
            if (!m_customMapEdit || m_customMapEdit->text().trimmed().isEmpty()) {
                Dialogs::info(this, Dialogs::Msg::CustomMapEmpty, m_settings);
                abort = true;
            } else {
                mapTok = m_customMapEdit->text().trimmed();
                if (!mapTok.startsWith(QLatin1String("map ")))
                    mapTok = QStringLiteral("map ") + mapTok;
            }
        }
        QString gt = QStringLiteral("tdm");
        if (m_gameTypeCombo) {
            const QStringList extra = m_gameTypeCombo->currentData().toStringList();
            if (!extra.isEmpty()) gt = extra.at(0);
        }
        selectedMap = QStringLiteral("gametype %1 %2").arg(gt, mapTok);
    } else if (m_serverId == "World at War") {
        if (m_radiosByKey.value("mapnacht")->isChecked())      selectedMap = "map nazi_zombie_prototype";
        else if (m_radiosByKey.value("mapverru")->isChecked()) selectedMap = "map nazi_zombie_asylum";
        else if (m_radiosByKey.value("mapshi")->isChecked())   selectedMap = "map nazi_zombie_sumpf";
        else if (m_radiosByKey.value("mapder")->isChecked())   selectedMap = "map nazi_zombie_factory";
        else if (m_radiosByKey.value("mapother")->isChecked()) {
            if (m_customMapEdit->text().trimmed().isEmpty()) {
                Dialogs::info(this, Dialogs::Msg::CustomMapEmpty, m_settings);
                abort = true;
            } else {
                selectedMap = "map " + m_customMapEdit->text().trimmed();
            }
        }
    } else if (m_serverId == "Black ops" && m_multiplayer) {
        static const QHash<QString, QString> mp = {
            {"mparray", "map mp_array"}, {"mpcracked", "map mp_cracked"},
            {"mpcrisis", "map mp_crisis"}, {"mpfiring", "map mp_firingrange"},
            {"mpduga", "map mp_duga"}, {"mphanoi", "map mp_hanoi"},
            {"mpcairo", "map mp_cairo"}, {"mphavoc", "map mp_havoc"},
            {"mpcosmo", "map mp_cosmodrome"}, {"mpnuked", "map mp_nuked"},
            {"mprad", "map mp_radiation"}, {"mpmountain", "map mp_mountain"},
            {"mpvilla", "map mp_villa"}, {"mprussian", "map mp_russianbase"},
            {"mpberlin", "map mp_berlinwall2"}, {"mpdisc", "map mp_discovery"},
            {"mpkowloon", "map mp_kowloon"}, {"mpstadium", "map mp_stadium"},
            {"mpgridlock", "map mp_gridlock"}, {"mphotel", "map mp_hotel"},
            {"mpoutskirts", "map mp_outskirts"}, {"mpzoo", "map mp_zoo"},
            {"mpdrivein", "map mp_drivein"}, {"mparea51", "map mp_area51"},
            {"mpgolf", "map mp_golfcourse"}, {"mpsilo", "map mp_silo"},
        };
        bool found = false;
        for (auto it = mp.begin(); it != mp.end(); ++it) {
            if (m_radiosByKey.value(it.key()) && m_radiosByKey.value(it.key())->isChecked()) {
                selectedMap = it.value();
                found = true;
                break;
            }
        }
        if (!found && m_radiosByKey.value("mapother") && m_radiosByKey.value("mapother")->isChecked()) {
            if (!m_customMapEdit || m_customMapEdit->text().trimmed().isEmpty()) {
                Dialogs::info(this, Dialogs::Msg::CustomMapEmpty, m_settings);
                abort = true;
            } else {
                QString custom = m_customMapEdit->text().trimmed();
                if (!custom.startsWith(QLatin1String("map ")))
                    custom = QStringLiteral("map ") + custom;
                selectedMap = custom;
            }
        }
    } else if (m_serverId == "Black ops") {
        if (m_radiosByKey.value("mapkino")->isChecked())        selectedMap = "map zombie_theater";
        else if (m_radiosByKey.value("mapfive")->isChecked())   selectedMap = "map zombie_pentagon";
        else if (m_radiosByKey.value("mapops")->isChecked())    selectedMap = "map zombietron";
        else if (m_radiosByKey.value("mapasc")->isChecked())    selectedMap = "map zombie_cosmodrome";
        else if (m_radiosByKey.value("mapcall")->isChecked())   selectedMap = "map zombie_coast";
        else if (m_radiosByKey.value("mapshangri")->isChecked())selectedMap = "map zombie_temple";
        else if (m_radiosByKey.value("mapmoon")->isChecked())   selectedMap = "map zombie_moon";
        else if (m_radiosByKey.value("mapnacht")->isChecked())  selectedMap = "map zombie_cod5_prototype";
        else if (m_radiosByKey.value("mapverru")->isChecked())  selectedMap = "map zombie_cod5_asylum";
        else if (m_radiosByKey.value("mapshi")->isChecked())    selectedMap = "map zombie_cod5_sumpf";
        else if (m_radiosByKey.value("mapder")->isChecked())    selectedMap = "map zombie_cod5_factory";
        else if (m_radiosByKey.value("mapother") && m_radiosByKey.value("mapother")->isChecked()) {
            if (!m_customMapEdit || m_customMapEdit->text().trimmed().isEmpty()) {
                Dialogs::info(this, Dialogs::Msg::CustomMapEmpty, m_settings);
                abort = true;
            } else {
                QString custom = m_customMapEdit->text().trimmed();
                if (!custom.startsWith(QLatin1String("map ")))
                    custom = QStringLiteral("map ") + custom;
                selectedMap = custom;
            }
        }
    } else if (m_serverId == "Black ops II" && m_multiplayer) {
        static const QHash<QString, QString> mp = {
            {"mpla","map mp_la"},{"mpdockside","map mp_dockside"},{"mpcarrier","map mp_carrier"},
            {"mpdrone","map mp_drone"},{"mpexpress","map mp_express"},{"mphijacked","map mp_hijacked"},
            {"mpmeltdown","map mp_meltdown"},{"mpoverflow","map mp_overflow"},{"mpnightclub","map mp_nightclub"},
            {"mpraid","map mp_raid"},{"mpslums","map mp_slums"},{"mpvillage","map mp_village"},
            {"mpturbine","map mp_turbine"},{"mpsocotra","map mp_socotra"},{"mpnuketown","map mp_nuketown_2020"},
            {"mpdownhill","map mp_downhill"},{"mpmirage","map mp_mirage"},{"mphydro","map mp_hydro"},
            {"mpskate","map mp_skate"},{"mpconcert","map mp_concert"},{"mpmagma","map mp_magma"},
            {"mpvertigo","map mp_vertigo"},{"mpstudio","map mp_studio"},{"mpuplink","map mp_uplink"},
            {"mpbridge","map mp_bridge"},{"mpcastaway","map mp_castaway"},{"mppaintball","map mp_paintball"},
            {"mpdig","map mp_dig"},{"mpfrostbite","map mp_frostbite"},{"mppod","map mp_pod"},
            {"mptakeoff","map mp_takeoff"},
        };
        QString mapTok;
        for (auto it = mp.begin(); it != mp.end(); ++it) {
            if (m_radiosByKey.value(it.key()) && m_radiosByKey.value(it.key())->isChecked()) {
                mapTok = it.value(); break;
            }
        }
        if (mapTok.isEmpty() && m_radiosByKey.value("mapother") && m_radiosByKey.value("mapother")->isChecked()) {
            if (!m_customMapEdit || m_customMapEdit->text().trimmed().isEmpty()) {
                Dialogs::info(this, Dialogs::Msg::CustomMapEmpty, m_settings);
                abort = true;
            } else {
                mapTok = m_customMapEdit->text().trimmed();
                if (!mapTok.startsWith(QLatin1String("map ")))
                    mapTok = QStringLiteral("map ") + mapTok;
            }
        }
        QString gt = QStringLiteral("tdm");
        if (m_gameTypeCombo) {
            const QStringList extra = m_gameTypeCombo->currentData().toStringList();
            if (!extra.isEmpty()) gt = extra.at(0);
        }
        selectedMap = QStringLiteral("execgts %1.cfg %2").arg(gt, mapTok);
    } else if (m_serverId == "Black ops II") {
        // prer4516 e permanentemente falso (o proprio script original tinha
        // that feature was disabled with "if 1==3:"), so we always use the
        // comandos "execgts".
        if (m_radiosByKey.value("mapbus")->isChecked())          selectedMap = "execgts zm_classic_transit.cfg map zm_transit";
        else if (m_radiosByKey.value("mapfarm")->isChecked())    selectedMap = "execgts zm_standard_farm map zm_transit";
        else if (m_radiosByKey.value("maptown")->isChecked())    selectedMap = "execgts zm_standard_town.cfg map zm_transit";
        else if (m_radiosByKey.value("mapdepot")->isChecked())   selectedMap = "execgts zm_standard_transit.cfg map zm_transit";
        else if (m_radiosByKey.value("mapnuke")->isChecked())    selectedMap = "execgts zm_standard_nuked.cfg map zm_nuked";
        else if (m_radiosByKey.value("maprise")->isChecked())    selectedMap = "execgts zm_classic_rooftop.cfg map zm_highrise";
        else if (m_radiosByKey.value("mapmob")->isChecked())     selectedMap = "execgts zm_classic_prison.cfg map zm_prison";
        else if (m_radiosByKey.value("mapburied")->isChecked())  selectedMap = "execgts zm_classic_processing.cfg map zm_buried";
        else if (m_radiosByKey.value("maporigins")->isChecked()) selectedMap = "execgts zm_classic_tomb.cfg map zm_tomb";
    }

    if (!abort) {
        QString error;
        QString gameType;
        bool wager = false;
        if (m_gameTypeCombo) {
            const QStringList extra = m_gameTypeCombo->currentData().toStringList();
            if (extra.size() >= 1) gameType = extra.at(0);
            if (extra.size() >= 2) wager = extra.at(1) == QLatin1String("1");
        }
        if (!ConfigMaker::generateConfig(m_settings, m_serverId, configName, selectedMap,
                                          m_multiplayer, /*prer4516=*/false, gameType, wager, error)) {
            Dialogs::error(this, error);
        } else {
            emit configCreated();
            accept();
        }
    }
}
