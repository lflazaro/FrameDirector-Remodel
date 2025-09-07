#ifndef EXPORTDIALOG_H
#define EXPORTDIALOG_H

#include <QDialog>
#include <QComboBox>
#include <QSpinBox>
#include <QProgressBar>
#include <QLabel>
#include <QCheckBox>

class ExportDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ExportDialog(QWidget* parent = nullptr);
    QString getFormat() const;
    int getQuality() const;
    bool getLoop() const;

public slots:
    void updateProgress(int value, int maximum);

private:
    QComboBox* m_formatCombo;
    QSpinBox* m_qualitySpinBox;
    QCheckBox* m_loopCheckBox;
    QProgressBar* m_progressBar;
    QLabel* m_statusLabel;
};

#endif // EXPORTDIALOG_H