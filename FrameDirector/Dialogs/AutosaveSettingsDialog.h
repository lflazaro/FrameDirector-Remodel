#ifndef AUTOSAVESETTINGSDIALOG_H
#define AUTOSAVESETTINGSDIALOG_H

#include <QDialog>
#include <QString>

class QComboBox;
class QLineEdit;

class AutosaveSettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit AutosaveSettingsDialog(int currentIntervalMinutes,
        const QString& currentDirectory,
        QWidget* parent = nullptr);

    int intervalMinutes() const;
    QString directory() const;

private slots:
    void browseForDirectory();

private:
    void populateIntervals(int currentIntervalMinutes);

    QComboBox* m_intervalCombo;
    QLineEdit* m_directoryEdit;
};

#endif // AUTOSAVESETTINGSDIALOG_H
