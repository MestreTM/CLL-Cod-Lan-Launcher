#include "ProgressDialog.h"

#include <QLabel>
#include <QProgressBar>
#include <QPushButton>
#include <QVBoxLayout>

ProgressDialog::ProgressDialog(const QString &title, QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(title);
    setModal(true);
    setMinimumWidth(420);
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(20, 18, 20, 16);
    root->setSpacing(12);

    m_status = new QLabel(tr("Preparando..."), this);
    m_status->setWordWrap(true);
    root->addWidget(m_status);

    m_bar = new QProgressBar(this);
    m_bar->setRange(0, 0); // indeterminado ate receber totais
    m_bar->setValue(0);
    root->addWidget(m_bar);

    m_closeBtn = new QPushButton(tr("Cancelar"), this);
    root->addWidget(m_closeBtn, 0, Qt::AlignRight);
    connect(m_closeBtn, &QPushButton::clicked, this, [this]() {
        emit cancelRequested();
        // If already finished, just close; otherwise the caller may abort.
        if (m_closeBtn->text() == tr("Fechar"))
            accept();
    });
}

void ProgressDialog::setStatus(const QString &text)
{
    m_status->setText(text);
}

void ProgressDialog::setProgress(qint64 received, qint64 total)
{
    if (total <= 0) {
        m_bar->setRange(0, 0);
        return;
    }
    m_bar->setRange(0, 1000);
    m_bar->setValue(int((received * 1000) / total));
}

void ProgressDialog::setIndeterminate(bool on)
{
    if (on)
        m_bar->setRange(0, 0);
    else
        m_bar->setRange(0, 100);
}

void ProgressDialog::setFinished(bool success, const QString &message)
{
    m_status->setText(message);
    m_bar->setRange(0, 100);
    m_bar->setValue(success ? 100 : 0);
    m_closeBtn->setText(tr("Fechar"));
}
