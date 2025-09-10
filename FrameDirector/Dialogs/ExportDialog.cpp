#include "ExportDialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QPushButton>
#include <QCheckBox>
#include <QMessageBox>

ExportDialog::ExportDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle("Export Animation");
    setMinimumWidth(400);

    QVBoxLayout* mainLayout = new QVBoxLayout(this);

    // Format selection
    QFormLayout* formLayout = new QFormLayout;
    
    m_formatCombo = new QComboBox;
    m_formatCombo->addItems({"GIF", "MP4"});
    formLayout->addRow("Format:", m_formatCombo);

    m_qualitySpinBox = new QSpinBox;
    m_qualitySpinBox->setRange(1, 100);
    m_qualitySpinBox->setValue(80);
    m_qualitySpinBox->setSuffix("%");
    formLayout->addRow("Quality:", m_qualitySpinBox);

    m_loopCheckBox = new QCheckBox("Loop animation");
    m_loopCheckBox->setChecked(true);
    formLayout->addRow("", m_loopCheckBox);

    mainLayout->addLayout(formLayout);

    // Progress section
    m_progressBar = new QProgressBar;
    m_progressBar->setRange(0, 100);
    m_progressBar->setValue(0);
    m_progressBar->setTextVisible(true);
    mainLayout->addWidget(m_progressBar);

    m_statusLabel = new QLabel("Ready to export");
    m_statusLabel->setAlignment(Qt::AlignCenter);
    mainLayout->addWidget(m_statusLabel);

    // Buttons
    QHBoxLayout* buttonLayout = new QHBoxLayout;
    QPushButton* cancelButton = new QPushButton("Cancel");
    QPushButton* exportButton = new QPushButton("Export");
    exportButton->setDefault(true);

    buttonLayout->addWidget(cancelButton);
    buttonLayout->addWidget(exportButton);
    mainLayout->addLayout(buttonLayout);

    // Style
    setStyleSheet(R"(
        QDialog {
            background-color: #2D2D2D;
            color: #FFFFFF;
        }
        QComboBox, QSpinBox {
            background-color: #3D3D3D;
            color: #FFFFFF;
            border: 1px solid #555555;
            border-radius: 2px;
            padding: 5px;
            min-width: 100px;
        }
        QComboBox::drop-down {
            border: none;
            border-left: 1px solid #555555;
            width: 20px;
        }
        QComboBox::down-arrow {
            image: none;
            border-left: 5px solid transparent;
            border-right: 5px solid transparent;
            border-top: 5px solid #FFFFFF;
        }
        QPushButton {
            background-color: #4A90E2;
            color: #FFFFFF;
            border: none;
            border-radius: 2px;
            padding: 8px 16px;
            min-width: 80px;
        }
        QPushButton:hover {
            background-color: #5AA1E3;
        }
        QPushButton:pressed {
            background-color: #3A80D2;
        }
        QPushButton[text="Cancel"] {
            background-color: #666666;
        }
        QPushButton[text="Cancel"]:hover {
            background-color: #777777;
        }
        QProgressBar {
            background-color: #3D3D3D;
            border: 1px solid #555555;
            border-radius: 2px;
            height: 20px;
            text-align: center;
        }
        QProgressBar::chunk {
            background-color: #4A90E2;
            border-radius: 1px;
        }
        QLabel {
            color: #FFFFFF;
            font-size: 12px;
        }
        QCheckBox {
            color: #FFFFFF;
        }
        QCheckBox::indicator {
            width: 16px;
            height: 16px;
            background-color: #3D3D3D;
            border: 1px solid #555555;
            border-radius: 2px;
        }
        QCheckBox::indicator:checked {
            background-color: #4A90E2;
            border: 1px solid #4A90E2;
            image: url(:/icons/checkmark.png);
        }
    )");

    connect(cancelButton, &QPushButton::clicked, this, &QDialog::reject);
    connect(exportButton, &QPushButton::clicked, this, &QDialog::accept);

    connect(m_formatCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
        [this](int index) {
            bool isGif = index == 0;
            m_loopCheckBox->setEnabled(isGif);
            m_qualitySpinBox->setEnabled(!isGif);
        });
}

QString ExportDialog::getFormat() const
{
    return m_formatCombo->currentText().toLower();
}

int ExportDialog::getQuality() const
{
    return m_qualitySpinBox->value();
}

bool ExportDialog::getLoop() const
{
    return m_loopCheckBox->isChecked();
}

void ExportDialog::updateProgress(int value, int maximum)
{
    if (maximum > 0) {
        m_progressBar->setRange(0, maximum);
        m_progressBar->setValue(value);
        
        int percent = static_cast<int>((value * 100.0) / maximum);
        m_statusLabel->setText(QString("Exporting... %1%").arg(percent));
    }
}
