#pragma once
#include <QCheckBox>
#include <QRadioButton>
#include <QEvent>
#include <QFontMetrics>
#include <QPainter>
#include <QPainterPath>

// Hand-drawn checkbox and radio.
//
// Qt often fails to recompute the label width after QSS resizes the indicator,
// so text gets clipped ("Multiplaye"). This widget paints the mark and the
// label itself and reports its own sizeHint. No Q_OBJECT — no moc needed.
namespace Ui {

namespace detail {
inline const QColor kText(0xd1, 0xd2, 0xe3);
inline const QColor kTextOn(0xff, 0xff, 0xff);
inline const QColor kTextOff(0x6b, 0x6c, 0x86);
inline const QColor kFill(0x13, 0x14, 0x26);
inline const QColor kBorder(0x4a, 0x4d, 0x68);
inline const QColor kAccent(0x91, 0x84, 0xd9);
inline const QColor kAccentLight(0xcf, 0xc7, 0xf2);
inline const QColor kInk(0x14, 0x12, 0x1f);

constexpr int kBox = 18;   // lado do indicador
constexpr int kGap = 10;   // espaco entre indicador e texto
} // namespace detail

class CheckBox : public QCheckBox
{
public:
    explicit CheckBox(const QString &text, QWidget *parent = nullptr)
        : QCheckBox(text, parent)
    {
        setAttribute(Qt::WA_Hover, true);
        setCursor(Qt::PointingHandCursor);
    }

    QSize sizeHint() const override
    {
        const QFontMetrics fm(font());
        const int w = detail::kBox + detail::kGap + fm.horizontalAdvance(text()) + 4;
        const int h = qMax(detail::kBox + 8, fm.height() + 8);
        return QSize(w, h);
    }
    QSize minimumSizeHint() const override { return sizeHint(); }

protected:
    bool event(QEvent *e) override
    {
        switch (e->type()) {
        case QEvent::HoverEnter:
        case QEvent::HoverLeave:
        case QEvent::Enter:
        case QEvent::Leave:
            update();
            break;
        default:
            break;
        }
        return QCheckBox::event(e);
    }

    void paintEvent(QPaintEvent *) override
    {
        using namespace detail;
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);

        const bool on = isChecked();
        const bool hov = underMouse() && isEnabled();
        const bool off = !isEnabled();

        QRectF box(1.0, (height() - kBox) / 2.0, kBox - 1.0, kBox - 1.0);

        QColor border = on ? kAccentLight : kBorder;
        QColor fill = on ? kAccent : kFill;
        if (hov) {
            border = on ? QColor(0xe4, 0xdf, 0xfa) : kAccent;
            if (on)
                fill = QColor(0xa7, 0x9d, 0xe2);
            else
                fill = QColor(0x1b, 0x1d, 0x31);
        }
        if (off) {
            border = QColor(0x2d, 0x30, 0x45);
            fill = on ? QColor(0x4a, 0x44, 0x68) : QColor(0x19, 0x1b, 0x28);
        }

        if (hasFocus()) {
            p.setPen(QPen(kAccent, 1.0));
            p.setBrush(Qt::NoBrush);
            p.drawRoundedRect(box.adjusted(-3, -3, 3, 3), 7, 7);
        }

        p.setPen(QPen(border, 2.0));
        p.setBrush(fill);
        p.drawRoundedRect(box, 5, 5);

        if (on) {
            // checkmark
            QPainterPath tick;
            tick.moveTo(box.left() + box.width() * 0.24, box.top() + box.height() * 0.53);
            tick.lineTo(box.left() + box.width() * 0.44, box.top() + box.height() * 0.73);
            tick.lineTo(box.left() + box.width() * 0.78, box.top() + box.height() * 0.29);
            p.setBrush(Qt::NoBrush);
            p.setPen(QPen(off ? QColor(0x9a, 0x9a, 0xb0) : kInk, 2.4,
                          Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            p.drawPath(tick);
        }

        const QRect textRect(kBox + kGap, 0, width() - kBox - kGap, height());
        p.setPen(off ? kTextOff : (on || hov ? kTextOn : kText));
        p.setFont(font());
        p.drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter, text());
    }
};

class RadioButton : public QRadioButton
{
public:
    explicit RadioButton(const QString &text, QWidget *parent = nullptr)
        : QRadioButton(text, parent)
    {
        setAttribute(Qt::WA_Hover, true);
        setCursor(Qt::PointingHandCursor);
    }

    QSize sizeHint() const override
    {
        const QFontMetrics fm(font());
        const int w = detail::kBox + detail::kGap + fm.horizontalAdvance(text()) + 4;
        const int h = qMax(detail::kBox + 8, fm.height() + 8);
        return QSize(w, h);
    }
    QSize minimumSizeHint() const override { return sizeHint(); }

protected:
    bool event(QEvent *e) override
    {
        switch (e->type()) {
        case QEvent::HoverEnter:
        case QEvent::HoverLeave:
        case QEvent::Enter:
        case QEvent::Leave:
            update();
            break;
        default:
            break;
        }
        return QRadioButton::event(e);
    }

    void paintEvent(QPaintEvent *) override
    {
        using namespace detail;
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);

        const bool on = isChecked();
        const bool hov = underMouse() && isEnabled();
        const bool off = !isEnabled();

        QRectF circle(1.0, (height() - kBox) / 2.0, kBox - 1.0, kBox - 1.0);
        QColor border = on ? kAccent : (hov ? kAccent : kBorder);
        if (off)
            border = QColor(0x2d, 0x30, 0x45);

        if (hasFocus()) {
            p.setPen(QPen(kAccent, 1.0));
            p.setBrush(Qt::NoBrush);
            p.drawEllipse(circle.adjusted(-3, -3, 3, 3));
        }

        p.setPen(QPen(border, 2.0));
        p.setBrush(off ? QColor(0x19, 0x1b, 0x28)
                       : (hov && !on ? QColor(0x1b, 0x1d, 0x31) : kFill));
        p.drawEllipse(circle);

        if (on) {
            p.setPen(Qt::NoPen);
            p.setBrush(off ? QColor(0x4a, 0x44, 0x68) : kAccent);
            p.drawEllipse(circle.adjusted(4, 4, -4, -4));
        }

        const QRect textRect(kBox + kGap, 0, width() - kBox - kGap, height());
        p.setPen(off ? kTextOff : (on || hov ? kTextOn : kText));
        p.setFont(font());
        p.drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter, text());
    }
};

} // namespace Ui
