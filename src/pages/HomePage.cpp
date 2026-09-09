#include "HomePage.h"
#include "AppSettings.h"

#include <QApplication>
#include <QDialog>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QLinearGradient>
#include <QMouseEvent>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QResizeEvent>
#include <QScrollArea>
#include <QScrollBar>
#include <QVBoxLayout>
#include <QVariantAnimation>

#include <functional>

namespace {

constexpr int kCardMinWidth = 210;
constexpr int kRadius = 8;

QNetworkRequest remoteImageRequest(const QString &url)
{
    QNetworkRequest req{QUrl(url)};
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                     QNetworkRequest::NoLessSafeRedirectPolicy);
    req.setMaximumRedirectsAllowed(8);
    req.setRawHeader("User-Agent",
                     "Mozilla/5.0 (Windows NT 10.0; Win64; x64) Chrome/128.0.0.0 Safari/537.36");
    req.setRawHeader("Accept", "image/avif,image/webp,image/apng,image/*,*/*;q=0.8");
    return req;
}

QPixmap coverPixmap(const QPixmap &src, const QSize &target, int radius)
{
    if (src.isNull() || target.isEmpty())
        return QPixmap();

    const qreal dpr = qApp ? qApp->devicePixelRatio() : 1.0;
    const QSize dev = target * dpr;
    QPixmap out(dev);
    out.setDevicePixelRatio(dpr);
    out.fill(Qt::transparent);

    QPainter p(&out);
    p.setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform, true);
    QPainterPath path;
    path.addRoundedRect(QRectF(QPointF(0, 0), QSizeF(target)), radius, radius);
    p.setClipPath(path);

    const QPixmap scaled = src.scaled(target, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
    p.drawPixmap(QPointF((target.width() - scaled.width() / dpr) / 2.0,
                         (target.height() - scaled.height() / dpr) / 2.0),
                 scaled);
    return out;
}

// ---------------------------------------------------------------- thumbnail
class Thumb : public QWidget
{
public:
    explicit Thumb(QWidget *parent = nullptr) : QWidget(parent)
    {
        setMinimumHeight(120);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    }
    void setSource(const QPixmap &pm) { m_src = pm; m_cache = QPixmap(); update(); }
    void setBadge(const QString &text) { m_badge = text; update(); }
    int heightForWidth(int w) const override { return int(w * 10.0 / 16.0); }
    bool hasHeightForWidth() const override { return true; }
    QSize sizeHint() const override { return QSize(kCardMinWidth, kCardMinWidth * 10 / 16); }

protected:
    void resizeEvent(QResizeEvent *e) override
    {
        QWidget::resizeEvent(e);
        const int h = heightForWidth(width());
        if (height() != h)
            setFixedHeight(h);
        m_cache = QPixmap();
    }
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform, true);
        QPainterPath path;
        path.addRoundedRect(QRectF(rect()), kRadius, kRadius);
        p.fillPath(path, QColor("#1b1d2e"));

        if (m_cache.isNull() && !m_src.isNull())
            m_cache = coverPixmap(m_src, size(), kRadius);
        if (!m_cache.isNull())
            p.drawPixmap(0, 0, m_cache);

        if (!m_badge.isEmpty()) {
            QFont f = font();
            f.setPointSizeF(f.pointSizeF() - 1.5);
            f.setLetterSpacing(QFont::AbsoluteSpacing, 1.0);
            p.setFont(f);
            const QRect text = p.fontMetrics().boundingRect(m_badge.toUpper());
            const QRect box(8, 8, text.width() + 14, text.height() + 6);
            QPainterPath bp;
            bp.addRoundedRect(QRectF(box), 4, 4);
            p.fillPath(bp, QColor(12, 13, 22, 190));
            p.setPen(QColor("#cfd1dc"));
            p.drawText(box, Qt::AlignCenter, m_badge.toUpper());
        }
    }

private:
    QPixmap m_src, m_cache;
    QString m_badge;
};

// --------------------------------------------------------------- mod card
class ModCard : public QFrame
{
public:
    ModCard(const HomeCatalog::Mod &mod, const QString &gameName, QWidget *parent = nullptr)
        : QFrame(parent), m_id(mod.id)
    {
        setObjectName("ModCard");
        setProperty("modId", mod.id);
        setAttribute(Qt::WA_Hover, true);
        setCursor(Qt::PointingHandCursor);
        setMinimumWidth(kCardMinWidth);

        auto *v = new QVBoxLayout(this);
        v->setContentsMargins(0, 0, 0, 0);
        v->setSpacing(0);

        m_thumb = new Thumb(this);
        m_thumb->setSource(HomeCatalog::pixmap(mod.image));
        m_thumb->setBadge(mod.kind.compare("github", Qt::CaseInsensitive) == 0 ? QStringLiteral("GitHub")
                                                                               : QStringLiteral("CLL"));
        v->addWidget(m_thumb);

        auto *body = new QVBoxLayout();
        body->setContentsMargins(12, 10, 12, 12);
        body->setSpacing(3);

        auto *name = new QLabel(mod.name, this);
        name->setObjectName("ModCardTitle");
        name->setWordWrap(true);
        auto *author = new QLabel(QObject::tr("por %1").arg(mod.author), this);
        author->setObjectName("ModCardMeta");
        auto *meta = new QLabel(QStringLiteral("%1  ·  %2")
                                    .arg(mod.size.isEmpty() ? QStringLiteral("—") : mod.size,
                                         gameName),
                                this);
        meta->setObjectName("ModCardMeta");
        meta->setWordWrap(true);

        body->addWidget(name);
        body->addWidget(author);
        body->addSpacing(2);
        body->addWidget(meta);
        v->addLayout(body);
    }

    void setThumb(const QPixmap &pm) { m_thumb->setSource(pm); }
    QString modId() const { return m_id; }
    std::function<void(QString)> onClick;

protected:
    void mouseReleaseEvent(QMouseEvent *e) override
    {
        if (e->button() == Qt::LeftButton && rect().contains(e->pos()) && onClick)
            onClick(m_id);
        QFrame::mouseReleaseEvent(e);
    }

private:
    QString m_id;
    Thumb *m_thumb = nullptr;
};

} // namespace

// ------------------------------------------------------------- hero slider
class HeroSlider : public QWidget
{
public:
    struct Slide
    {
        QString modId;
        QString title;
        QString summary;
        QString author;
        QStringList tags;
        qint64 downloads = 0;
        QString imageUrl;
        QString logoUrl;
        QPixmap bg;
        QPixmap logo;
    };

    explicit HeroSlider(QWidget *parent = nullptr) : QWidget(parent)
    {
        setObjectName("HeroSlider");
        setMinimumHeight(300);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

        auto *root = new QVBoxLayout(this);
        root->setContentsMargins(32, 28, 32, 22);
        root->addStretch(1);

        m_logo = new QLabel(this);
        m_logo->setObjectName("HeroSlideLogo");
        m_title = new QLabel(this);
        m_title->setObjectName("HeroSlideTitle");
        m_title->setWordWrap(true);
        m_summary = new QLabel(this);
        m_summary->setObjectName("HeroSlideSummary");
        m_summary->setWordWrap(true);
        m_meta = new QLabel(this);
        m_meta->setObjectName("HeroSlideMeta");

        root->addWidget(m_logo);
        root->addSpacing(6);
        root->addWidget(m_title);
        root->addWidget(m_summary);
        root->addWidget(m_meta);
        root->addSpacing(14);

        auto *actions = new QHBoxLayout();
        actions->setSpacing(8);
        m_install = new QPushButton(QObject::tr("Instalar"), this);
        m_install->setProperty("cssClass", "primary");
        m_install->setCursor(Qt::PointingHandCursor);
        m_install->setMinimumHeight(38);
        m_details = new QPushButton(QObject::tr("Detalhes"), this);
        m_details->setCursor(Qt::PointingHandCursor);
        m_details->setMinimumHeight(38);
        actions->addWidget(m_install);
        actions->addWidget(m_details);
        actions->addStretch(1);

        m_dots = new QHBoxLayout();
        m_dots->setSpacing(7);
        actions->addLayout(m_dots);
        root->addLayout(actions);

        m_prev = new QPushButton(QStringLiteral("‹"), this);
        m_next = new QPushButton(QStringLiteral("›"), this);
        for (QPushButton *b : {m_prev, m_next}) {
            b->setObjectName("HeroArrow");
            b->setCursor(Qt::PointingHandCursor);
            b->setFixedSize(38, 38);
        }
        connect(m_prev, &QPushButton::clicked, this, [this]() { step(-1); });
        connect(m_next, &QPushButton::clicked, this, [this]() { step(1); });
        connect(m_install, &QPushButton::clicked, this, [this]() {
            if (onInstall && m_index < m_slides.size()) onInstall(m_slides[m_index].modId);
        });
        connect(m_details, &QPushButton::clicked, this, [this]() {
            if (onDetails && m_index < m_slides.size()) onDetails(m_slides[m_index].modId);
        });

        m_fade.setDuration(340);
        m_fade.setStartValue(0.0);
        m_fade.setEndValue(1.0);
        connect(&m_fade, &QVariantAnimation::valueChanged, this, [this](const QVariant &) { update(); });

        m_auto.setInterval(6000);
        connect(&m_auto, &QTimer::timeout, this, [this]() { step(1); });
    }

    void setSlides(const QList<Slide> &slides)
    {
        m_slides = slides;
        m_index = 0;
        rebuildDots();
        apply(false);
        if (slides.size() > 1) m_auto.start(); else m_auto.stop();
        setVisible(!slides.isEmpty());
    }

    void setBackgroundFor(const QString &modId, const QPixmap &pm)
    {
        for (int i = 0; i < m_slides.size(); ++i) {
            if (m_slides[i].modId == modId) {
                m_slides[i].bg = pm;
                if (i == m_index) apply(false);
            }
        }
    }

    void setAssetByUrl(const QString &url, const QPixmap &pm)
    {
        if (url.isEmpty() || pm.isNull())
            return;
        bool touched = false;
        for (int i = 0; i < m_slides.size(); ++i) {
            if (m_slides[i].imageUrl == url) {
                m_slides[i].bg = pm;
                touched = true;
            }
            if (m_slides[i].logoUrl == url) {
                m_slides[i].logo = pm;
                touched = true;
            }
            if (touched && i == m_index)
                apply(false);
        }
    }

    void setAutoplay(bool on)
    {
        if (on && m_slides.size() > 1) m_auto.start();
        else m_auto.stop();
    }

    void retranslate()
    {
        if (m_install)
            m_install->setText(QObject::tr("Instalar"));
        if (m_details)
            m_details->setText(QObject::tr("Detalhes"));
    }

    std::function<void(QString)> onInstall;
    std::function<void(QString)> onDetails;

protected:
    void resizeEvent(QResizeEvent *e) override
    {
        QWidget::resizeEvent(e);
        m_prev->move(12, (height() - m_prev->height()) / 2);
        m_next->move(width() - m_next->width() - 12, (height() - m_next->height()) / 2);
        m_cache = QPixmap();
        m_prevCache = QPixmap();
    }

    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform, true);

        QPainterPath clip;
        clip.addRoundedRect(QRectF(rect()), 14, 14);
        p.setClipPath(clip);
        p.fillPath(clip, QColor("#101120"));

        const Slide *s = (m_index < m_slides.size()) ? &m_slides[m_index] : nullptr;
        if (s && !s->bg.isNull()) {
            if (m_cache.isNull())
                m_cache = coverPixmap(s->bg, size(), 14);
            const qreal t = m_fade.state() == QAbstractAnimation::Running
                                ? m_fade.currentValue().toReal() : 1.0;
            if (!m_prevCache.isNull() && t < 1.0)
                p.drawPixmap(0, 0, m_prevCache);
            p.setOpacity(t);
            p.drawPixmap(0, 0, m_cache);
            p.setOpacity(1.0);
        }

        QLinearGradient g(0, 0, width() * 0.75, height());
        g.setColorAt(0.0, QColor(10, 11, 14, 40));
        g.setColorAt(0.45, QColor(10, 11, 14, 150));
        g.setColorAt(1.0, QColor(10, 11, 14, 225));
        p.fillRect(rect(), g);

        QLinearGradient bottom(0, height() - 220, 0, height());
        bottom.setColorAt(0, QColor(10, 11, 14, 0));
        bottom.setColorAt(1, QColor(10, 11, 14, 235));
        p.fillRect(QRect(0, height() - 220, width(), 220), bottom);

        QPen edge(QColor("#2b2e42"));
        edge.setWidth(1);
        p.setPen(edge);
        p.drawPath(clip);
    }

private:
    void step(int d)
    {
        if (m_slides.size() < 2)
            return;
        m_prevCache = m_cache;
        m_index = (m_index + d + m_slides.size()) % m_slides.size();
        apply(true);
    }

    void rebuildDots()
    {
        while (QLayoutItem *it = m_dots->takeAt(0)) {
            delete it->widget();
            delete it;
        }
        m_dotButtons.clear();
        for (int i = 0; i < m_slides.size(); ++i) {
            auto *dot = new QPushButton(this);
            dot->setObjectName("HeroDot");
            dot->setCheckable(true);
            dot->setCursor(Qt::PointingHandCursor);
            dot->setFixedSize(8, 8);
            connect(dot, &QPushButton::clicked, this, [this, i]() {
                m_prevCache = m_cache;
                m_index = i;
                apply(true);
                m_auto.start();
            });
            m_dots->addWidget(dot);
            m_dotButtons << dot;
        }
    }

    void apply(bool animate)
    {
        if (m_index >= m_slides.size())
            return;
        const Slide &s = m_slides[m_index];

        if (!s.logo.isNull()) {
            m_logo->setPixmap(s.logo.scaled(QSize(230, 30), Qt::KeepAspectRatio, Qt::SmoothTransformation));
            m_logo->show();
        } else {
            m_logo->hide();
        }
        m_title->setText(s.title);
        m_summary->setText(s.summary);
        QString meta = QObject::tr("por %1").arg(s.author);
        if (!s.tags.isEmpty())
            meta = s.tags.join(QStringLiteral("  ·  ")) + QStringLiteral("      ") + meta;
        m_meta->setText(meta);

        for (int i = 0; i < m_dotButtons.size(); ++i) {
            m_dotButtons[i]->setChecked(i == m_index);
            m_dotButtons[i]->setFixedWidth(i == m_index ? 22 : 8);
        }

        m_cache = QPixmap();
        if (animate) {
            m_fade.stop();
            m_fade.start();
        }
        update();
    }

    QList<Slide> m_slides;
    int m_index = 0;
    QLabel *m_logo = nullptr;
    QLabel *m_title = nullptr;
    QLabel *m_summary = nullptr;
    QLabel *m_meta = nullptr;
    QPushButton *m_install = nullptr;
    QPushButton *m_details = nullptr;
    QPushButton *m_prev = nullptr;
    QPushButton *m_next = nullptr;
    QHBoxLayout *m_dots = nullptr;
    QList<QPushButton *> m_dotButtons;
    QPixmap m_cache, m_prevCache;
    QVariantAnimation m_fade;
    QTimer m_auto;
};

// ------------------------------------------------------------- details dlg
namespace {

class ModDetailsDialog : public QDialog
{
public:
    ModDetailsDialog(const HomeCatalog::Mod &mod,
                     const QString &gameName,
                     const QPixmap &cachedCover,
                     QNetworkAccessManager *net,
                     std::function<void(QString, QPixmap)> onCoverFetched,
                     QWidget *parent)
        : QDialog(parent)
    {
        setWindowTitle(mod.name);
        setMinimumWidth(560);

        auto *v = new QVBoxLayout(this);
        v->setContentsMargins(0, 0, 0, 0);
        v->setSpacing(0);

        auto *art = new Thumb(this);
        QPixmap cover = cachedCover.isNull() ? HomeCatalog::pixmap(mod.image) : cachedCover;
        art->setSource(cover);
        art->setBadge(gameName);
        v->addWidget(art);

        if (cover.isNull() && HomeCatalog::isRemote(mod.image) && net) {
            const QString imageUrl = mod.image;
            QNetworkReply *reply = net->get(remoteImageRequest(imageUrl));
            QObject::connect(reply, &QNetworkReply::finished, art,
                             [art, reply, imageUrl, onCoverFetched]() {
                reply->deleteLater();
                if (reply->error() != QNetworkReply::NoError)
                    return;
                QPixmap pm;
                if (!pm.loadFromData(reply->readAll()))
                    return;
                art->setSource(pm);
                if (onCoverFetched)
                    onCoverFetched(imageUrl, pm);
            });
        }

        auto *body = new QVBoxLayout();
        body->setContentsMargins(20, 16, 20, 18);
        body->setSpacing(10);

        auto *title = new QLabel(mod.name, this);
        title->setObjectName("CardTitle");
        auto *kicker = new QLabel(gameName.toUpper(), this);
        kicker->setObjectName("CardKicker");
        body->addWidget(kicker);
        body->addWidget(title);

        if (!mod.tags.isEmpty()) {
            auto *tags = new QLabel(mod.tags.join(QStringLiteral("   ·   ")), this);
            tags->setObjectName("ModTags");
            body->addWidget(tags);
        }

        auto *desc = new QLabel(mod.description, this);
        desc->setObjectName("CardDesc");
        desc->setWordWrap(true);
        body->addWidget(desc);

        auto *grid = new QGridLayout();
        grid->setHorizontalSpacing(24);
        grid->setVerticalSpacing(2);
        const QList<QPair<QString, QString>> facts = {
            {QObject::tr("AUTOR"), mod.author},
            {QObject::tr("VERSAO"), mod.version.isEmpty() ? QStringLiteral("—") : mod.version},
            {QObject::tr("TAMANHO"), mod.size.isEmpty() ? QStringLiteral("—") : mod.size},
            {QObject::tr("ORIGEM"), mod.kind.compare("github", Qt::CaseInsensitive) == 0
                                        ? QStringLiteral("GitHub") : QObject::tr("Pacote .cll")},
        };
        for (int i = 0; i < facts.size(); ++i) {
            auto *k = new QLabel(facts[i].first, this);
            k->setObjectName("CardKicker");
            auto *val = new QLabel(facts[i].second, this);
            grid->addWidget(k, 0, i);
            grid->addWidget(val, 1, i);
        }
        body->addLayout(grid);

        auto *url = new QLabel(mod.url, this);
        url->setObjectName("MutedHint");
        url->setWordWrap(true);
        body->addWidget(url);

        auto *row = new QHBoxLayout();
        row->addStretch(1);
        auto *close = new QPushButton(QObject::tr("Fechar"), this);
        auto *install = new QPushButton(QObject::tr("Instalar"), this);
        install->setProperty("cssClass", "primary");
        install->setMinimumHeight(38);
        close->setMinimumHeight(38);
        row->addWidget(close);
        row->addWidget(install);
        body->addLayout(row);

        v->addLayout(body);

        connect(close, &QPushButton::clicked, this, &QDialog::reject);
        connect(install, &QPushButton::clicked, this, &QDialog::accept);
    }
};

} // namespace

// ------------------------------------------------------------------ page
HomePage::HomePage(AppSettings &settings, QWidget *parent)
    : QWidget(parent)
    , m_settings(settings)
{
    setObjectName("HomePage");
    m_net = new QNetworkAccessManager(this);

    m_searchDebounce.setSingleShot(true);
    m_searchDebounce.setInterval(180);
    connect(&m_searchDebounce, &QTimer::timeout, this, [this]() {
        m_query = m_search->text().trimmed();
        rebuildContent();
    });

    buildUi();
    reload();
}

void HomePage::buildUi()
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // barra de busca + filtros
    auto *bar = new QWidget(this);
    bar->setObjectName("HomeBar");
    auto *barLayout = new QHBoxLayout(bar);
    barLayout->setContentsMargins(28, 14, 28, 14);
    barLayout->setSpacing(10);

    m_search = new QLineEdit(bar);
    m_search->setPlaceholderText(tr("Buscar mods, mapas, autores…"));
    m_search->setClearButtonEnabled(true);
    m_search->setMinimumHeight(34);
    m_search->setMaximumWidth(360);
    connect(m_search, &QLineEdit::textChanged, this, [this](const QString &) { m_searchDebounce.start(); });
    barLayout->addWidget(m_search);

    m_filterBar = new QWidget(bar);
    m_filterLayout = new QHBoxLayout(m_filterBar);
    m_filterLayout->setContentsMargins(0, 0, 0, 0);
    m_filterLayout->setSpacing(6);
    barLayout->addWidget(m_filterBar);
    barLayout->addStretch(1);

    m_importBtn = new QPushButton(tr("Importar manualmente"), bar);
    m_importBtn->setProperty("cssClass", "primary");
    m_importBtn->setCursor(Qt::PointingHandCursor);
    m_importBtn->setMinimumHeight(34);
    connect(m_importBtn, &QPushButton::clicked, this, [this]() { emit installRequested(QString()); });
    barLayout->addWidget(m_importBtn);

    root->addWidget(bar);

    // conteudo rolavel
    m_scroll = new QScrollArea(this);
    m_scroll->setWidgetResizable(true);
    m_scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_content = new QWidget(m_scroll);
    m_content->setObjectName("HomeContent");
    m_contentLayout = new QVBoxLayout(m_content);
    m_contentLayout->setContentsMargins(28, 20, 28, 32);
    m_contentLayout->setSpacing(18);

    m_status = new QLabel(m_content);
    m_status->setObjectName("MutedHint");
    m_status->setWordWrap(true);
    m_status->hide();
    m_contentLayout->addWidget(m_status);

    m_hero = new HeroSlider(m_content);
    m_hero->onInstall = [this](const QString &id) { emit installRequested(id); };
    m_hero->onDetails = [this](const QString &id) { openDetails(id); };
    m_contentLayout->addWidget(m_hero);

    m_scroll->setWidget(m_content);
    root->addWidget(m_scroll, 1);
}

void HomePage::reload()
{
    m_catalog = HomeCatalog::load();
    if (!m_catalog.ok) {
        m_status->setText(tr("Catalogo indisponivel — %1").arg(m_catalog.error));
        m_status->show();
    } else {
        m_status->hide();
    }
    rebuildFilters();
    rebuildContent();
}

void HomePage::rebuildFilters()
{
    for (QPushButton *b : m_filterButtons)
        delete b;
    m_filterButtons.clear();

    struct Chip { QString id; QString label; };
    QList<Chip> chips { {QStringLiteral("all"), tr("Todos")} };
    for (const HomeCatalog::Game &g : m_catalog.games)
        chips << Chip{g.id, g.name};

    for (const Chip &c : chips) {
        auto *btn = new QPushButton(c.label, m_filterBar);
        btn->setObjectName("GameChip");
        btn->setCheckable(true);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setProperty("gameId", c.id);
        btn->setChecked(c.id == m_gameFilter);
        connect(btn, &QPushButton::clicked, this, [this, id = c.id]() { selectGame(id); });
        m_filterLayout->addWidget(btn);
        m_filterButtons << btn;
    }
}

void HomePage::selectGame(const QString &gameId)
{
    m_gameFilter = gameId.isEmpty() ? QStringLiteral("all") : gameId;
    for (QPushButton *b : m_filterButtons)
        b->setChecked(b->property("gameId").toString() == m_gameFilter);
    rebuildContent();
}

QList<HomeCatalog::Mod> HomePage::filteredMods(const QString &gameId) const
{
    QList<HomeCatalog::Mod> out;
    for (const HomeCatalog::Mod &m : m_catalog.mods) {
        if (!gameId.isEmpty() && gameId != "all" && m.game != gameId)
            continue;
        if (!m_query.isEmpty()) {
            const HomeCatalog::Game *g = HomeCatalog::findGame(m_catalog, m.game);
            const QString hay = QStringList{m.name, m.author, m.summary, m.tags.join(' '),
                                            g ? g->name : QString()}
                                    .join(' ');
            if (!hay.contains(m_query, Qt::CaseInsensitive))
                continue;
        }
        out << m;
    }
    return out;
}

void HomePage::rebuildContent()
{
    // limpa tudo abaixo do hero
    while (m_contentLayout->count() > 2) {
        QLayoutItem *item = m_contentLayout->takeAt(2);
        if (QWidget *w = item->widget())
            w->deleteLater();
        delete item;
    }

    // slider so aparece sem busca ativa
    QList<HeroSlider::Slide> slides;
    if (m_query.isEmpty()) {
        QStringList ids = m_catalog.featured;
        if (ids.isEmpty())
            for (const HomeCatalog::Mod &m : m_catalog.mods)
                if (ids.size() < 4) ids << m.id;
        for (const QString &id : ids) {
            const HomeCatalog::Mod *m = HomeCatalog::findMod(m_catalog, id);
            if (!m)
                continue;
            if (m_gameFilter != "all" && m->game != m_gameFilter)
                continue;
            const HomeCatalog::Game *g = HomeCatalog::findGame(m_catalog, m->game);
            HeroSlider::Slide s;
            s.modId = m->id;
            s.title = m->name;
            s.summary = m->summary;
            s.author = m->author;
            s.tags = m->tags;
            s.downloads = m->downloads;
            s.imageUrl = m->image;
            s.logoUrl = g ? g->logo : QString();
            s.bg = resolvedCover(m->image);
            s.logo = g ? resolvedCover(g->logo) : QPixmap();
            if (s.bg.isNull() && HomeCatalog::isRemote(m->image))
                requestRemote(m->image, m->id);
            if (s.logo.isNull() && g && HomeCatalog::isRemote(g->logo))
                requestRemote(g->logo, QString());
            slides << s;
        }
    }
    m_hero->setSlides(slides);

    if (!m_catalog.ok) {
        auto *card = new QFrame(m_content);
        card->setObjectName("Card");
        auto *cl = new QVBoxLayout(card);
        cl->setContentsMargins(18, 16, 18, 16);
        cl->setSpacing(8);
        auto *k = new QLabel(HomeCatalog::catalogUrl(), card);
        k->setObjectName("MutedHint");
        auto *t = new QLabel(tr("Could not load the page"), card);
        t->setObjectName("SectionTitle");
        t->setWordWrap(true);
        auto *b = new QLabel(m_catalog.error.isEmpty()
                                 ? tr("Could not load the page.")
                                 : m_catalog.error, card);
        b->setObjectName("MutedHint");
        b->setWordWrap(true);
        cl->addWidget(k);
        cl->addWidget(t);
        cl->addWidget(b);
        m_contentLayout->addWidget(card);
        m_contentLayout->addStretch();
        return;
    }

    // secoes por jogo (ou uma unica secao de resultados)
    struct SectionDef { QString id; QString name; QString logo; QList<HomeCatalog::Mod> mods; };
    QList<SectionDef> sections;
    if (!m_query.isEmpty()) {
        SectionDef s;
        s.id = QString();
        const QList<HomeCatalog::Mod> found = filteredMods(m_gameFilter);
        s.name = tr("%n resultado(s) para \"%1\"", "", int(found.size())).arg(m_query);
        s.mods = found;
        sections << s;
    } else {
        for (const HomeCatalog::Game &g : m_catalog.games) {
            if (m_gameFilter != "all" && g.id != m_gameFilter)
                continue;
            SectionDef s;
            s.id = g.id;
            s.name = g.name;
            s.logo = g.logo;
            s.mods = filteredMods(g.id);
            if (!s.mods.isEmpty())
                sections << s;
        }
    }

    for (const SectionDef &def : sections) {
        auto *section = new QWidget(m_content);
        auto *sv = new QVBoxLayout(section);
        sv->setContentsMargins(0, 8, 0, 0);
        sv->setSpacing(12);

        auto *head = new QHBoxLayout();
        head->setSpacing(10);
        const QPixmap logo = resolvedCover(def.logo);
        if (!logo.isNull() || HomeCatalog::isRemote(def.logo)) {
            auto *logoLabel = new QLabel(section);
            logoLabel->setObjectName("HomeGameLogo");
            logoLabel->setProperty("assetUrl", def.logo);
            if (!logo.isNull())
                logoLabel->setPixmap(logo.scaled(QSize(150, 22), Qt::KeepAspectRatio, Qt::SmoothTransformation));
            head->addWidget(logoLabel);
            if (logo.isNull() && HomeCatalog::isRemote(def.logo))
                requestRemote(def.logo, QString());
        }
        auto *name = new QLabel(def.name, section);
        name->setObjectName("SectionTitle");
        head->addWidget(name);
        auto *count = new QLabel(tr("%n mod(s)", "", int(def.mods.size())), section);
        count->setObjectName("MutedHint");
        head->addWidget(count);
        head->addStretch(1);
        if (!def.id.isEmpty()) {
            auto *all = new QPushButton(tr("Ver todos"), section);
            all->setProperty("cssClass", "ghost");
            all->setCursor(Qt::PointingHandCursor);
            connect(all, &QPushButton::clicked, this, [this, id = def.id]() {
                selectGame(id);
                emit gameRequested(id);
            });
            head->addWidget(all);
        }
        sv->addLayout(head);

        auto *rule = new QFrame(section);
        rule->setObjectName("NavRule");
        rule->setFrameShape(QFrame::HLine);
        sv->addWidget(rule);

        auto *grid = new QGridLayout();
        grid->setHorizontalSpacing(14);
        grid->setVerticalSpacing(14);
        grid->setProperty("isCardGrid", true);
        for (const HomeCatalog::Mod &m : def.mods) {
            const HomeCatalog::Game *g = HomeCatalog::findGame(m_catalog, m.game);
            auto *card = new ModCard(m, g ? g->name : m.game, section);
            card->onClick = [this](const QString &id) { openDetails(id); };
            const QPixmap cover = resolvedCover(m.image);
            if (!cover.isNull())
                card->setThumb(cover);
            else if (HomeCatalog::isRemote(m.image))
                requestRemote(m.image, m.id);
            grid->addWidget(card, 0, grid->count());
        }
        sv->addLayout(grid);
        m_contentLayout->addWidget(section);
    }

    if (sections.isEmpty()) {
        auto *empty = new QLabel(m_query.isEmpty() ? tr("Nenhum mod no catalogo.")
                                                   : tr("Nenhum mod encontrado para essa busca."),
                                 m_content);
        empty->setObjectName("MutedHint");
        empty->setAlignment(Qt::AlignCenter);
        empty->setMinimumHeight(160);
        m_contentLayout->addWidget(empty);
    }

    m_contentLayout->addStretch(1);
    relayoutGrids();
}

void HomePage::relayoutGrids()
{
    const int available = (m_scroll ? m_scroll->viewport()->width() : width()) - 56;
    const int columns = qMax(2, (available + 14) / (kCardMinWidth + 14));

    for (QGridLayout *grid : m_content->findChildren<QGridLayout *>()) {
        if (!grid->property("isCardGrid").toBool())
            continue;
        QList<QWidget *> cards;
        while (QLayoutItem *item = grid->takeAt(0)) {
            if (QWidget *w = item->widget())
                cards << w;
            delete item;
        }
        for (int i = 0; i < cards.size(); ++i)
            grid->addWidget(cards[i], i / columns, i % columns);
        for (int c = 0; c < columns; ++c)
            grid->setColumnStretch(c, 1);
    }
}

void HomePage::openDetails(const QString &modId)
{
    const HomeCatalog::Mod *m = HomeCatalog::findMod(m_catalog, modId);
    if (!m)
        return;
    emit detailsRequested(modId);
    const HomeCatalog::Game *g = HomeCatalog::findGame(m_catalog, m->game);
    const QPixmap cover = resolvedCover(m->image);
    ModDetailsDialog dlg(*m, g ? g->name : m->game, cover, m_net,
                         [this, modId](const QString &url, const QPixmap &pm) {
        m_remote.insert(url, pm);
        applyRemoteCover(modId, pm);
    }, this);
    m_hero->setAutoplay(false);
    const int result = dlg.exec();
    m_hero->setAutoplay(true);
    if (result == QDialog::Accepted)
        emit installRequested(modId);
}

QPixmap HomePage::resolvedCover(const QString &ref) const
{
    QPixmap pm = HomeCatalog::pixmap(ref);
    if (pm.isNull() && HomeCatalog::isRemote(ref))
        pm = m_remote.value(ref);
    return pm;
}

void HomePage::applyRemoteCover(const QString &modId, const QPixmap &pm)
{
    if (pm.isNull())
        return;
    if (m_hero && !modId.isEmpty())
        m_hero->setBackgroundFor(modId, pm);
    if (!m_content)
        return;
    for (QFrame *w : m_content->findChildren<QFrame *>()) {
        if (w->objectName() != QLatin1String("ModCard"))
            continue;
        if (w->property("modId").toString() != modId)
            continue;
        static_cast<ModCard *>(w)->setThumb(pm);
    }
}

void HomePage::applyRemoteUrl(const QString &url, const QPixmap &pm)
{
    if (url.isEmpty() || pm.isNull())
        return;
    if (m_hero)
        m_hero->setAssetByUrl(url, pm);
    if (!m_content)
        return;
    for (QLabel *lab : m_content->findChildren<QLabel *>()) {
        if (lab->objectName() != QLatin1String("HomeGameLogo"))
            continue;
        if (lab->property("assetUrl").toString() != url)
            continue;
        lab->setPixmap(pm.scaled(QSize(150, 22), Qt::KeepAspectRatio, Qt::SmoothTransformation));
    }
}

void HomePage::requestRemote(const QString &url, const QString &modId)
{
    if (url.isEmpty())
        return;
    const QPixmap cached = m_remote.value(url);
    if (!cached.isNull()) {
        applyRemoteCover(modId, cached);
        applyRemoteUrl(url, cached);
        return;
    }
    if (m_remote.contains(url))
        return;
    m_remote.insert(url, QPixmap());

    QNetworkReply *reply = m_net->get(remoteImageRequest(url));
    connect(reply, &QNetworkReply::finished, this, [this, reply, url, modId]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            m_remote.remove(url);
            return;
        }
        QPixmap pm;
        if (!pm.loadFromData(reply->readAll())) {
            m_remote.remove(url);
            return;
        }
        m_remote.insert(url, pm);
        applyRemoteCover(modId, pm);
        applyRemoteUrl(url, pm);
    });
}

void HomePage::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    if (m_hero)
        m_hero->setFixedHeight(qBound(280, int(width() * 0.34), 420));
    relayoutGrids();
}

void HomePage::retranslate()
{
    if (m_search)
        m_search->setPlaceholderText(tr("Buscar mods, mapas, autores…"));
    if (m_importBtn)
        m_importBtn->setText(tr("Importar manualmente"));
    if (m_hero)
        m_hero->retranslate();
    rebuildFilters();
    rebuildContent();
}
