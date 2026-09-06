#include "AnimatedLogo.h"

#include <QIcon>
#include <QPainter>

AnimatedLogo::AnimatedLogo(QWidget *parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_TransparentForMouseEvents);
    m_pix = QIcon(":/icons/app.svg").pixmap(QSize(256, 256));
}

void AnimatedLogo::setLogoSize(int px)
{
    m_size = px;
    setFixedSize(px, px);
}

void AnimatedLogo::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::SmoothPixmapTransform, true);
    const int s = qMin(width(), height());
    const QRect target((width() - s) / 2, (height() - s) / 2, s, s);
    p.drawPixmap(target, m_pix);
}
