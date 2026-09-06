#include "PlayPage.h"
#include "AppSettings.h"
#include "GameCatalog.h"
#include "../Checkables.h"

#include <QPainter>
#include <QLinearGradient>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QRadioButton>
#include <QStyle>
#include <QResizeEvent>

PlayPage::PlayPage(AppSettings &settings, QWidget *parent)
    : QWidget(parent)
    , m_settings(settings)
{
    setObjectName("PlayPage");

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(48, 40, 48, 40);
    root->addStretch(2);

    m_code = new QLabel(this);
    m_code->setObjectName("HeroCode");
    m_code->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    m_logo = new QLabel(this);
    m_logo->setObjectName("HeroLogo");
    m_logo->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    m_title = new QLabel(this);
    m_title->setObjectName("HeroTitle");
    m_title->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    m_sub = new QLabel(this);
    m_sub->setObjectName("HeroSub");
    m_sub->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    m_path = new QLabel(this);
    m_path->setObjectName("HeroPath");
    m_path->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    m_path->setWordWrap(true);

    root->addWidget(m_code);
    root->addWidget(m_logo);
    root->addWidget(m_title);
    root->addWidget(m_sub);
    root->addSpacing(8);
    root->addWidget(m_path);
    root->addSpacing(18);

    auto *row = new QHBoxLayout();
    m_spzm = new Ui::RadioButton(tr("Solo / Zombies"), this);
    m_mp = new Ui::RadioButton(tr("Multiplayer"), this);
    m_spzm->setChecked(true);
    m_play = new QPushButton(tr("Iniciar"), this);
    m_play->setObjectName("PlayButton");
    m_play->setProperty("cssClass", "primary");
    m_play->setMinimumSize(180, 48);
    m_play->setCursor(Qt::PointingHandCursor);
    row->addWidget(m_spzm);
    row->addWidget(m_mp);
    row->addStretch();
    row->addWidget(m_play);
    root->addLayout(row);
    root->addStretch(1);

    connect(m_play, &QPushButton::clicked, this, [this]() {
        if (m_running)
            emit stopRequested(m_gameId);
        else
            emit launchRequested(m_gameId, m_mp->isChecked());
    });

    // Switching MP <-> ZM swaps the background (t6_bg_mp.jpg / t6_bg_zm.jpg).
    connect(m_mp, &QRadioButton::toggled, this, [this](bool) {
        refreshArt();
        update();
    });

    setGame("Black ops II");
}

bool PlayPage::isMultiplayer() const
{
    return m_mp->isChecked();
}

void PlayPage::setGame(const QString &gameId)
{
    m_gameId = gameId;
    const auto g = GameCatalog::byId(gameId);
    m_code->setText(g.shortLabel);
    m_title->setText(g.title.toUpper());
    m_sub->setText(g.subtitle);
    const QString folder = m_settings.gameFolder(gameId);
    m_path->setText(folder.isEmpty()
                        ? tr("Pasta do jogo nao configurada — abra Configuracoes.")
                        : folder);
    m_spzm->setVisible(g.hasZmSp);
    if (!g.hasZmSp)
        m_mp->setChecked(true);
    refreshArt();
    update();
}

void PlayPage::setRunning(bool running)
{
    m_running = running;
    if (running) {
        m_play->setText(tr("Encerrar"));
        m_play->setProperty("cssClass", "danger");
        m_play->setObjectName("StopButton");
    } else {
        m_play->setText(tr("Iniciar"));
        m_play->setProperty("cssClass", "primary");
        m_play->setObjectName("PlayButton");
    }
    m_play->style()->unpolish(m_play);
    m_play->style()->polish(m_play);
}

void PlayPage::refreshArt()
{
    const auto g = GameCatalog::byCode(GameCatalog::byId(m_gameId).code);
    const QSize bgSize = size().isEmpty() ? QSize(1600, 900) : size();
    const QString variant = (m_mp && m_mp->isChecked()) ? QStringLiteral("mp") : QStringLiteral("zm");
    m_bg = GameCatalog::background(g.code, bgSize, variant);

    const int logoW = qMax(240, qMin(width() > 0 ? width() - 120 : 480, 520));
    const QPixmap logoPm = GameCatalog::logo(g.code, QSize(logoW, 104));
    if (!logoPm.isNull()) {
        m_logo->setPixmap(logoPm);
        m_logo->show();
        m_title->hide();
    } else {
        m_logo->hide();
        m_title->show();
        m_title->setText(g.title.toUpper());
    }
}

void PlayPage::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    refreshArt();
}

void PlayPage::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::SmoothPixmapTransform, true);
    if (!m_bg.isNull()) {
        const QPixmap scaled = m_bg.scaled(size(), Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
        const int x = (width() - scaled.width()) / 2;
        const int y = (height() - scaled.height()) / 2;
        p.drawPixmap(x, y, scaled);
    } else {
        p.fillRect(rect(), QColor("#101114"));
    }
    QLinearGradient g(0, 0, width() * 0.7, height());
    g.setColorAt(0.0, QColor(10, 11, 14, 40));
    g.setColorAt(0.45, QColor(10, 11, 14, 160));
    g.setColorAt(1.0, QColor(10, 11, 14, 230));
    p.fillRect(rect(), g);

    QLinearGradient bottom(0, height() - 180, 0, height());
    bottom.setColorAt(0, QColor(10, 11, 14, 0));
    bottom.setColorAt(1, QColor(10, 11, 14, 210));
    p.fillRect(QRect(0, height() - 180, width(), 180), bottom);
}

void PlayPage::retranslate()
{
    if (m_spzm)
        m_spzm->setText(tr("Solo / Zombies"));
    if (m_mp)
        m_mp->setText(tr("Multiplayer"));
    setRunning(m_running);
    if (!m_gameId.isEmpty())
        setGame(m_gameId);
}
