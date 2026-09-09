#pragma once
#include <QWidget>

class QLineEdit;
class QPushButton;
class QLabel;
class QComboBox;
class QCheckBox;
class AppSettings;

class SettingsPage : public QWidget
{
    Q_OBJECT
public:
    explicit SettingsPage(AppSettings &settings, QWidget *parent = nullptr);
    void pullFromSettings();
    void retranslate();

signals:
    void plutoniumFolderChanged();
    void homeEnabledChanged(bool enabled);

private:
    QLineEdit *folderRow(class QVBoxLayout *layout, const QString &label,
                          QString AppSettings::*field);
    void updateKitButton();

    AppSettings &m_settings;
    QLineEdit *m_username = nullptr;
    QLineEdit *m_plutonium = nullptr;
    QPushButton *m_kitBtn = nullptr;
    QLabel *m_kitHint = nullptr;
    QComboBox *m_langCombo = nullptr;
    QCheckBox *m_homeEnabled = nullptr;
};
