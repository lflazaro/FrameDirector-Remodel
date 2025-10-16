#include "Dialogs/AutosaveSettingsDialog.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QList>
#include <QPushButton>
#include <QVBoxLayout>

AutosaveSettingsDialog::AutosaveSettingsDialog(int currentIntervalMinutes,
    const QString& currentDirectory,
    QWidget* parent)
    : QDialog(parent)
    , m_intervalCombo(new QComboBox(this))
    , m_directoryEdit(new QLineEdit(this))
{
    setWindowTitle(tr("Autosave Settings"));
    setModal(true);

    populateIntervals(currentIntervalMinutes);

    m_directoryEdit->setText(currentDirectory);

    auto* browseButton = new QPushButton(tr("Browse..."), this);
    connect(browseButton, &QPushButton::clicked, this, &AutosaveSettingsDialog::browseForDirectory);

    auto* directoryLayout = new QHBoxLayout;
    directoryLayout->addWidget(m_directoryEdit);
    directoryLayout->addWidget(browseButton);

    auto* formLayout = new QFormLayout;
    formLayout->addRow(tr("Autosave every:"), m_intervalCombo);
    formLayout->addRow(tr("Autosave folder:"), directoryLayout);

    auto* buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &AutosaveSettingsDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &AutosaveSettingsDialog::reject);

    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->addLayout(formLayout);
    mainLayout->addWidget(buttonBox);
}

int AutosaveSettingsDialog::intervalMinutes() const
{
    return m_intervalCombo->currentData().toInt();
}

QString AutosaveSettingsDialog::directory() const
{
    return m_directoryEdit->text();
}

void AutosaveSettingsDialog::browseForDirectory()
{
    const QString currentPath = m_directoryEdit->text();
    QString directory = QFileDialog::getExistingDirectory(this, tr("Select Autosave Folder"), currentPath);
    if (!directory.isEmpty()) {
        m_directoryEdit->setText(directory);
    }
}

void AutosaveSettingsDialog::populateIntervals(int currentIntervalMinutes)
{
    const QList<int> intervals = { 5, 10, 15, 30, 45, 60 };

    int defaultIndex = 0;
    for (int i = 0; i < intervals.size(); ++i) {
        const int minutes = intervals.at(i);
        m_intervalCombo->addItem(tr("%1 minutes").arg(minutes), minutes);
        if (minutes == currentIntervalMinutes) {
            defaultIndex = i;
        }
    }

    if (defaultIndex >= 0 && defaultIndex < m_intervalCombo->count()) {
        m_intervalCombo->setCurrentIndex(defaultIndex);
    }
    else {
        // If the current value isn't in the predefined list, append it.
        m_intervalCombo->addItem(tr("%1 minutes").arg(currentIntervalMinutes), currentIntervalMinutes);
        m_intervalCombo->setCurrentIndex(m_intervalCombo->count() - 1);
    }
}
