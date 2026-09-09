#include "SettingsPage.h"
#include "AppSettings.h"
#include "I18n.h"
#include "ImportKitDialog.h"
#include "../Checkables.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>

namespace {

// Standard card: kicker + title + optional description, vertical body.
QFrame *makeCard(QWidget *parent, const QString &kicker, const QString &title,
                 const QString &desc, QVBoxLayout **bodyOut)
{
    auto *card = new QFrame(parent);
    card->setObjectName("Card");
    auto *lay = new QVBoxLayout(card);
    lay->setContentsMargins(18, 15, 18, 16);
    lay->setSpacing(8);
    auto *k = new QLabel(kicker, card);
    k->setObjectName("CardKicker");
    lay->addWidget(k);
    auto *t = new QLabel(title, card);
    t->setObjectName("CardTitle");
    lay->addWidget(t);
    if (!desc.isEmpty()) {
        auto *d = new QLabel(desc, card);
        d->setObjectName("CardDesc");
        d->setWordWrap(true);
        lay->addWidget(d);
    }
    lay->addSpacing(2);
    *bodyOut = lay;
    return card;
}

} // namespace

SettingsPage::SettingsPage(AppSettings &settings, QWidget *parent)
    : QWidget(parent)
    , m_settings(settings)
{
    auto *scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    auto *inner = new QWidget;
    auto *root = new QVBoxLayout(inner);
    root->setContentsMargins(28, 22, 28, 24);
    root->setSpacing(14);

    // ---- 1. Profile ----
    QVBoxLayout *profileBody = nullptr;
    auto *profile = makeCard(inner, tr("PERFIL"), tr("Seu nickname"),
                             QString(), &profileBody);
    m_username = new QLineEdit(settings.username, profile);
    m_username->setPlaceholderText(tr("Ex.: MestreTM"));
    m_username->setMinimumHeight(38);
    profileBody->addWidget(m_username);
    connect(m_username, &QLineEdit::textChanged, this, [this](const QString &t) { m_settings.username = t; });
    profile->setObjectName("cardProfile");
    root->addWidget(profile);

    QVBoxLayout *langBody = nullptr;
    auto *langCard = makeCard(inner, tr("IDIOMA"), tr("Idioma do programa"),
                              tr("Vale para o setup e para o launcher. Alguns textos ja abertos atualizam ao reabrir."),
                              &langBody);
    m_langCombo = new QComboBox(langCard);
    m_langCombo->setMinimumHeight(36);
    for (const QString &code : I18n::codes())
        m_langCombo->addItem(I18n::displayName(code), code);
    int li = m_langCombo->findData(m_settings.language);
    m_langCombo->setCurrentIndex(li >= 0 ? li : 0);
    langBody->addWidget(m_langCombo);
    connect(m_langCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int) {
        const QString code = m_langCombo->currentData().toString();
        if (code.isEmpty())
            return;
        m_settings.language = code;
        I18n::apply(code);
        m_settings.saveToIni();
    });
    langCard->setObjectName("cardLang");
    root->addWidget(langCard);

    QVBoxLayout *homeBody = nullptr;
    auto *homeCard = makeCard(inner, tr("INÍCIO"), tr("Página inicial"),
                              tr("Desative para esconder o catálogo da barra lateral."),
                              &homeBody);
    m_homeEnabled = new QCheckBox(tr("Mostrar a aba Início"), homeCard);
    m_homeEnabled->setChecked(m_settings.homeEnabled);
    homeBody->addWidget(m_homeEnabled);
    connect(m_homeEnabled, &QCheckBox::toggled, this, [this](bool on) {
        m_settings.homeEnabled = on;
        m_settings.saveToIni();
        emit homeEnabledChanged(on);
    });
    homeCard->setObjectName("cardHome");
    root->addWidget(homeCard);

    // ---- 2. Plutonium client (folder + portable kit on the same card) ----
    QVBoxLayout *puBody = nullptr;
    auto *pu = makeCard(inner, tr("CLIENT"), tr("Plutonium"),
                        tr("Use o kit portatil em ./pu ou a instalacao oficial em AppData."),
                        &puBody);
    auto *puRow = new QHBoxLayout();
    puRow->setSpacing(8);
    m_plutonium = new QLineEdit(settings.plutoniumInstance, pu);
    m_plutonium->setPlaceholderText(tr("Pasta do client"));
    auto *browse = new QPushButton(tr("Procurar"), pu);
    browse->setObjectName("settingsPuBrowse");
    puRow->addWidget(m_plutonium, 1);
    puRow->addWidget(browse);
    puBody->addLayout(puRow);

    auto *kitRow = new QHBoxLayout();
    kitRow->setSpacing(10);
    m_kitHint = new QLabel(pu);
    m_kitHint->setWordWrap(true);
    m_kitHint->setObjectName("MutedHint");
    m_kitBtn = new QPushButton(tr("Baixar Plutonium Portable"), pu);
    kitRow->addWidget(m_kitHint, 1);
    kitRow->addWidget(m_kitBtn, 0, Qt::AlignBottom);
    puBody->addSpacing(2);
    puBody->addLayout(kitRow);

    connect(m_plutonium, &QLineEdit::textChanged, this, [this](const QString &t) {
        m_settings.plutoniumInstance = t;
        updateKitButton();
        emit plutoniumFolderChanged();
    });
    connect(m_plutonium, &QLineEdit::editingFinished, this, [this]() {
        const QString abs = AppSettings::resolvePath(m_plutonium->text());
        if (abs != m_plutonium->text())
            m_plutonium->setText(abs);
        m_settings.plutoniumInstance = abs;
        emit plutoniumFolderChanged();
    });
    connect(browse, &QPushButton::clicked, this, [this]() {
        const QString dir = QFileDialog::getExistingDirectory(this, tr("Pasta Plutonium"), m_plutonium->text());
        if (!dir.isEmpty())
            m_plutonium->setText(dir);
    });
    connect(m_kitBtn, &QPushButton::clicked, this, [this]() {
        if (m_settings.usingLocalPortableKit())
            return;
        ImportKitDialog dlg(this);
        dlg.exec();
        if (!dlg.installedOk())
            return;
        const QString puDir = AppSettings::localPuDir();
        m_settings.plutoniumInstance = puDir;
        m_plutonium->setText(puDir);
        m_settings.saveToIni();
        emit plutoniumFolderChanged();
        updateKitButton();
    });
    updateKitButton();
    pu->setObjectName("cardPu");
    root->addWidget(pu);

    // ---- 3. Game folders ----
    QVBoxLayout *gamesBody = nullptr;
    auto *games = makeCard(inner, tr("JOGOS"), tr("Pastas de instalacao"),
                           tr("Marque so os jogos que voce tem. Desmarcar nao apaga o caminho digitado."),
                           &gamesBody);
    folderRow(gamesBody, tr("T4 · World at War"), &AppSettings::waw);
    folderRow(gamesBody, tr("T5 · Black Ops"), &AppSettings::bo1);
    folderRow(gamesBody, tr("T6 · Black Ops II"), &AppSettings::bo2);
    folderRow(gamesBody, tr("IW5 · Modern Warfare 3"), &AppSettings::mw3);
    games->setObjectName("cardGames");
    root->addWidget(games);
    root->addStretch();

    scroll->setWidget(inner);
    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->addWidget(scroll);
}

QLineEdit *SettingsPage::folderRow(QVBoxLayout *layout, const QString &label,
                                   QString AppSettings::*field)
{
    const bool has = !(m_settings.*field).isEmpty();

    auto *row = new QHBoxLayout();
    row->setSpacing(8);
    auto *check = new Ui::CheckBox(label, this);
    check->setChecked(has);
    check->setMinimumWidth(qMax(check->sizeHint().width(), 190));
    auto *edit = new QLineEdit(m_settings.*field, this);
    edit->setPlaceholderText(tr("Pasta de instalacao"));
    edit->setEnabled(has);
    auto *browse = new QPushButton(tr("Procurar"), this);
    browse->setEnabled(has);
    row->addWidget(check);
    row->addWidget(edit, 1);
    row->addWidget(browse);
    layout->addSpacing(4);
    layout->addLayout(row);

    connect(check, &QCheckBox::toggled, this, [this, field, edit, browse](bool on) {
        edit->setEnabled(on);
        browse->setEnabled(on);
        // The path stays visible; unchecking only clears the effective value.
        m_settings.*field = on ? edit->text() : QString();
    });
    connect(edit, &QLineEdit::textChanged, this, [this, field, check](const QString &t) {
        if (check->isChecked())
            m_settings.*field = t;
    });
    connect(browse, &QPushButton::clicked, this, [this, edit, check]() {
        const QString dir = QFileDialog::getExistingDirectory(this, tr("Procurar"), edit->text());
        if (!dir.isEmpty()) {
            edit->setText(dir);
            check->setChecked(true);
        }
    });
    return edit;
}

void SettingsPage::updateKitButton()
{
    if (!m_kitBtn)
        return;
    const bool usingPu = m_settings.usingLocalPortableKit();
    m_kitBtn->setEnabled(!usingPu);
    if (usingPu)
        m_kitHint->setText(tr("✓ Usando kit PU Portable"));
    else
        m_kitHint->setText(tr("Baixa o kit portatil (pu.dat) para a pasta pu ao lado do programa."));
}

void SettingsPage::pullFromSettings()
{
    m_username->setText(m_settings.username);
    m_plutonium->setText(m_settings.plutoniumInstance);
    if (m_langCombo) {
        const int i = m_langCombo->findData(m_settings.language);
        if (i >= 0)
            m_langCombo->setCurrentIndex(i);
    }
    if (m_homeEnabled) {
        m_homeEnabled->blockSignals(true);
        m_homeEnabled->setChecked(m_settings.homeEnabled);
        m_homeEnabled->blockSignals(false);
    }
    updateKitButton();
}

void SettingsPage::retranslate()
{
    auto fill = [](QFrame *card, const QString &k, const QString &ti, const QString &d) {
        if (!card) return;
        for (auto *l : card->findChildren<QLabel*>(QString(), Qt::FindDirectChildrenOnly)) {
            if (l->objectName() == QLatin1String("CardKicker")) l->setText(k);
            else if (l->objectName() == QLatin1String("CardTitle")) l->setText(ti);
            else if (l->objectName() == QLatin1String("CardDesc")) l->setText(d);
        }
    };
    fill(findChild<QFrame*>("cardProfile"), tr("PERFIL"), tr("Seu nickname"), QString());
    fill(findChild<QFrame*>("cardLang"), tr("IDIOMA"), tr("Idioma do programa"),
         tr("Vale para o setup e para o launcher."));
    fill(findChild<QFrame*>("cardHome"), tr("INÍCIO"), tr("Página inicial"),
         tr("Desative para esconder o catálogo da barra lateral."));
    if (m_homeEnabled)
        m_homeEnabled->setText(tr("Mostrar a aba Início"));
    fill(findChild<QFrame*>("cardPu"), tr("CLIENT"), tr("Plutonium"),
         tr("Use o kit portatil em ./pu ou a instalacao oficial em AppData."));
    fill(findChild<QFrame*>("cardGames"), tr("JOGOS"), tr("Pastas de instalacao"),
         tr("Marque so os jogos que voce tem. Desmarcar nao apaga o caminho digitado."));
    if (m_username)
        m_username->setPlaceholderText(tr("Ex.: MestreTM"));
    if (m_plutonium)
        m_plutonium->setPlaceholderText(tr("Pasta do client"));
    if (auto *b = findChild<QPushButton*>("settingsPuBrowse"))
        b->setText(tr("Procurar"));
    if (m_kitBtn)
        m_kitBtn->setText(tr("Baixar Plutonium Portable"));
    updateKitButton();
    if (m_langCombo) {
        const QString cur = m_langCombo->currentData().toString();
        m_langCombo->blockSignals(true);
        m_langCombo->clear();
        for (const QString &code : I18n::codes())
            m_langCombo->addItem(I18n::displayName(code), code);
        const int i = m_langCombo->findData(cur);
        m_langCombo->setCurrentIndex(i >= 0 ? i : 0);
        m_langCombo->blockSignals(false);
    }
}
