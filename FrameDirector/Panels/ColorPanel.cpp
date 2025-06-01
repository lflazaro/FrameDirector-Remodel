// ===== ColorPanel.cpp =====
#include "ColorPanel.h"
#include "../MainWindow.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QColorDialog>

ColorPanel::ColorPanel(MainWindow* parent)
    : QWidget(parent)
    , m_mainWindow(parent)
    , m_strokeColor(Qt::black)
    , m_fillColor(Qt::white)
{
    setupUI();
}

void ColorPanel::setupUI()
{
    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setContentsMargins(4, 4, 4, 4);

    // Header
    QLabel* headerLabel = new QLabel("Colors");
    headerLabel->setStyleSheet(
        "QLabel {"
        "    color: white;"
        "    font-weight: bold;"
        "    font-size: 12px;"
        "    padding: 4px;"
        "    background-color: #3E3E42;"
        "    border-radius: 2px;"
        "}"
    );
    headerLabel->setAlignment(Qt::AlignCenter);
    m_mainLayout->addWidget(headerLabel);

    // Color buttons
    QHBoxLayout* colorLayout = new QHBoxLayout;

    m_strokeColorButton = new QPushButton("Stroke");
    m_strokeColorButton->setStyleSheet(
        "QPushButton {"
        "    background-color: black;"
        "    color: white;"
        "    border: 2px solid #5A5A5C;"
        "    border-radius: 4px;"
        "    padding: 8px;"
        "    min-height: 30px;"
        "}"
        "QPushButton:hover {"
        "    border: 2px solid #007ACC;"
        "}"
    );

    m_fillColorButton = new QPushButton("Fill");
    m_fillColorButton->setStyleSheet(
        "QPushButton {"
        "    background-color: white;"
        "    color: black;"
        "    border: 2px solid #5A5A5C;"
        "    border-radius: 4px;"
        "    padding: 8px;"
        "    min-height: 30px;"
        "}"
        "QPushButton:hover {"
        "    border: 2px solid #007ACC;"
        "}"
    );

    colorLayout->addWidget(m_strokeColorButton);
    colorLayout->addWidget(m_fillColorButton);
    m_mainLayout->addLayout(colorLayout);

    m_mainLayout->addStretch();

    // Connect signals
    connect(m_strokeColorButton, &QPushButton::clicked, this, &ColorPanel::onStrokeColorClicked);
    connect(m_fillColorButton, &QPushButton::clicked, this, &ColorPanel::onFillColorClicked);
}

void ColorPanel::setStrokeColor(const QColor& color)
{
    m_strokeColor = color;
    m_strokeColorButton->setStyleSheet(
        QString("QPushButton {"
            "    background-color: %1;"
            "    color: %2;"
            "    border: 2px solid #5A5A5C;"
            "    border-radius: 4px;"
            "    padding: 8px;"
            "    min-height: 30px;"
            "}"
            "QPushButton:hover {"
            "    border: 2px solid #007ACC;"
            "}")
        .arg(color.name())
        .arg(color.lightness() > 128 ? "black" : "white"));
}

void ColorPanel::setFillColor(const QColor& color)
{
    m_fillColor = color;
    m_fillColorButton->setStyleSheet(
        QString("QPushButton {"
            "    background-color: %1;"
            "    color: %2;"
            "    border: 2px solid #5A5A5C;"
            "    border-radius: 4px;"
            "    padding: 8px;"
            "    min-height: 30px;"
            "}"
            "QPushButton:hover {"
            "    border: 2px solid #007ACC;"
            "}")
        .arg(color.name())
        .arg(color.lightness() > 128 ? "black" : "white"));
}

QColor ColorPanel::getStrokeColor() const
{
    return m_strokeColor;
}

QColor ColorPanel::getFillColor() const
{
    return m_fillColor;
}

void ColorPanel::onStrokeColorClicked()
{
    QColor color = QColorDialog::getColor(m_strokeColor, this, "Select Stroke Color");
    if (color.isValid()) {
        setStrokeColor(color);
        emit strokeColorChanged(color);
    }
}

void ColorPanel::onFillColorClicked()
{
    QColor color = QColorDialog::getColor(m_fillColor, this, "Select Fill Color");
    if (color.isValid()) {
        setFillColor(color);
        emit fillColorChanged(color);
    }
}
