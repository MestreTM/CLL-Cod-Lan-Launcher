#pragma once
#include <QDialog>
#include <QHash>
#include <QString>

class QLineEdit;
class QRadioButton;
class QButtonGroup;
class QComboBox;
class AppSettings;

class MakeConfigDialog : public QDialog
{
    Q_OBJECT
public:
    MakeConfigDialog(AppSettings &settings, const QString &serverId,
                     bool multiplayer, QWidget *parent = nullptr);

signals:
    void configCreated();

private slots:
    void onMakeConfig();

private:
    void buildWawMaps(class QVBoxLayout *into);
    void buildWawMpMaps(class QVBoxLayout *into);
    void buildBo1Maps(class QVBoxLayout *into);
    void buildBo1MpMaps(class QVBoxLayout *into);
    void buildBo2Maps(class QVBoxLayout *into);
    void buildBo2MpMaps(class QVBoxLayout *into);

    AppSettings &m_settings;
    QString m_serverId;
    bool m_multiplayer = false;
    QLineEdit *m_nameEdit = nullptr;
    QButtonGroup *m_mapGroup = nullptr;
    QLineEdit *m_customMapEdit = nullptr;
    QComboBox *m_gameTypeCombo = nullptr;
    QHash<QString, QRadioButton *> m_radiosByKey;
};
