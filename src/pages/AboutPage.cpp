#include "AboutPage.h"
#include "AnimatedLogo.h"

#include <QDesktopServices>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSizePolicy>
#include <QUrl>
#include <QVBoxLayout>

AboutPage::AboutPage(QWidget *parent)
    : QWidget(parent)
{
    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(28, 24, 28, 24);
    outer->setAlignment(Qt::AlignTop | Qt::AlignHCenter);

    auto *col = new QWidget(this);
    col->setObjectName("AboutColumn");
    col->setMaximumWidth(620);
    col->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Preferred);

    auto *root = new QVBoxLayout(col);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(14);
    root->setAlignment(Qt::AlignTop | Qt::AlignHCenter);

    auto *logo = new AnimatedLogo(col);
    logo->setLogoSize(96);
    root->addWidget(logo, 0, Qt::AlignHCenter);

    auto *title = new QLabel(tr("Cod Lan Launcher"), col);
    title->setObjectName("HeroTitle");
    title->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
    title->setWordWrap(true);
    root->addWidget(title, 0, Qt::AlignHCenter);

    auto *titleRepo = new QPushButton(QStringLiteral("GitHub"), col);
    titleRepo->setObjectName("RepoLink");
    titleRepo->setCursor(Qt::PointingHandCursor);
    titleRepo->setToolTip(QStringLiteral("https://github.com/MestreTM/CLL-CodLanLauncher"));
    connect(titleRepo, &QPushButton::clicked, this, []() {
        QDesktopServices::openUrl(QUrl(QStringLiteral("https://github.com/MestreTM/CLL-CodLanLauncher")));
    });
    root->addWidget(titleRepo, 0, Qt::AlignHCenter);

    m_by = new QLabel(tr("por MestreTM"), col);
    m_by->setObjectName("AboutCredit");
    m_by->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
    m_by->setWordWrap(true);
    root->addWidget(m_by, 0, Qt::AlignHCenter);

    auto *ver = new QLabel(QStringLiteral("v1.0.0"), col);
    ver->setObjectName("HeroSub");
    ver->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
    root->addWidget(ver, 0, Qt::AlignHCenter);

    m_body = new QLabel(col);
    m_body->setObjectName("AboutBody");
    m_body->setWordWrap(true);
    m_body->setAlignment(Qt::AlignHCenter | Qt::AlignTop);
    m_body->setTextFormat(Qt::PlainText);
    m_body->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    root->addWidget(m_body, 0, Qt::AlignHCenter);

    auto *pluto = new QPushButton(tr("plutonium.pw"), col);
    connect(pluto, &QPushButton::clicked, this, []() {
        QDesktopServices::openUrl(QUrl(QStringLiteral("https://plutonium.pw/")));
    });
    auto *xerxes = new QPushButton(QStringLiteral("xerxes-at"), col);
    connect(xerxes, &QPushButton::clicked, this, []() {
        QDesktopServices::openUrl(QUrl(QStringLiteral("https://github.com/xerxes-at")));
    });
    auto *jug = new QPushButton(QStringLiteral("JugAndDoubleTap"), col);
    connect(jug, &QPushButton::clicked, this, []() {
        QDesktopServices::openUrl(QUrl(QStringLiteral("https://github.com/JugAndDoubleTap/")));
    });
    auto *row = new QHBoxLayout();
    row->setSpacing(10);
    row->setAlignment(Qt::AlignHCenter);
    row->addStretch();
    row->addWidget(pluto);
    row->addWidget(xerxes);
    row->addWidget(jug);
    row->addStretch();
    root->addLayout(row);

    outer->addWidget(col, 0, Qt::AlignHCenter);
    outer->addStretch();

    retranslate();
}

void AboutPage::retranslate()
{
    if (m_by)
        m_by->setText(tr("por MestreTM"));
    if (m_body)
        m_body->setText(tr("Launcher offline para Plutonium (T4 / T5 / T6 / IW5).\n\n"
                           "Este programa e de MestreTM.\n"
                           "A ideia e a logica original foram baseadas no LanLauncher de JugAndDoubleTap.\n"
                           "Arquivos de configuracao de servidor LAN (T4 / T5 / T6) by xerxes-at.\n\n"
                           "Plutonium e uma marca da equipe Plutonium. Call of Duty e da Activision / Treyarch.\n"
                           "Este projeto nao e afiliado a nenhuma delas."));
}
