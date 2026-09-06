#pragma once
#include <QWidget>

class AboutPage : public QWidget
{
    Q_OBJECT
public:
    explicit AboutPage(QWidget *parent = nullptr);
    void retranslate();
private:
    class QLabel *m_by = nullptr;
    class QLabel *m_body = nullptr;
};
