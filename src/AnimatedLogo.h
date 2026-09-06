#pragma once
#include <QWidget>
#include <QPixmap>

// App mark from :/icons/app.svg — static (no animation).
// The old setSpinning API still exists, but it is a no-op.
class AnimatedLogo : public QWidget
{
    Q_OBJECT
public:
    explicit AnimatedLogo(QWidget *parent = nullptr);
    void setLogoSize(int px);
    void setSpinning(bool) {}

protected:
    void paintEvent(QPaintEvent *event) override;
    QSize sizeHint() const override { return QSize(m_size, m_size); }

private:
    QPixmap m_pix;
    int m_size = 64;
};
