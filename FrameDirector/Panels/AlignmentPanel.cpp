// Panels/AlignmentPanel.cpp
#include "AlignmentPanel.h"
#include "../MainWindow.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QPushButton>
#include <QGroupBox>
#include <QLabel>
#include <QSpacerItem>

AlignmentPanel::AlignmentPanel(MainWindow* parent)
    : QWidget(parent)
    , m_mainWindow(parent)
{
    setupUI();

    // Connect to main window alignment signals
    connect(this, &AlignmentPanel::alignmentRequested,
        parent, &MainWindow::alignObjects);
}

void AlignmentPanel::setupUI()
{
    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setContentsMargins(4, 4, 4, 4);
    m_mainLayout->setSpacing(4);

    createAlignmentGroup();
    createDistributeGroup();
    createArrangeGroup();

    m_mainLayout->addStretch();
}

void AlignmentPanel::createAlignmentGroup()
{
    m_alignmentGroup = new QGroupBox("Align");
    m_alignmentGroup->setStyleSheet(
        "QGroupBox {"
        "    color: white;"
        "    font-weight: bold;"
        "    border: 1px solid #5A5A5C;"
        "    border-radius: 4px;"
        "    margin: 4px 0px;"
        "    padding-top: 8px;"
        "}"
        "QGroupBox::title {"
        "    subcontrol-origin: margin;"
        "    left: 8px;"
        "    padding: 0 4px 0 4px;"
        "}"
    );

    QGridLayout* alignLayout = new QGridLayout(m_alignmentGroup);
    alignLayout->setSpacing(2);

    // Define button style
    QString buttonStyle =
        "QPushButton {"
        "    background-color: #3E3E42;"
        "    color: white;"
        "    border: 1px solid #5A5A5C;"
        "    border-radius: 3px;"
        "    padding: 4px;"
        "    font-size: 11px;"
        "    min-width: 20px;"
        "    min-height: 20px;"
        "}"
        "QPushButton:hover {"
        "    background-color: #4A4A4F;"
        "    border: 1px solid #007ACC;"
        "}"
        "QPushButton:pressed {"
        "    background-color: #007ACC;"
        "}";

    // Horizontal alignment buttons
    m_alignLeftButton = new QPushButton("⫷");
    m_alignLeftButton->setToolTip("Align Left");
    m_alignLeftButton->setStyleSheet(buttonStyle);
    m_alignLeftButton->setProperty(0, static_cast<int>(MainWindow::AlignLeft));

    m_alignCenterButton = new QPushButton("⫸");
    m_alignCenterButton->setToolTip("Align Center");
    m_alignCenterButton->setStyleSheet(buttonStyle);
    m_alignCenterButton->setProperty(0, static_cast<int>(MainWindow::AlignCenter));

    m_alignRightButton = new QPushButton("⫸");
    m_alignRightButton->setToolTip("Align Right");
    m_alignRightButton->setStyleSheet(buttonStyle);
    m_alignRightButton->setProperty(0, static_cast<int>(MainWindow::AlignRight));

    // Vertical alignment buttons
    m_alignTopButton = new QPushButton("⫯");
    m_alignTopButton->setToolTip("Align Top");
    m_alignTopButton->setStyleSheet(buttonStyle);
    m_alignTopButton->setProperty(0, static_cast<int>(MainWindow::AlignTop));

    m_alignMiddleButton = new QPushButton("☰");
    m_alignMiddleButton->setToolTip("Align Middle");
    m_alignMiddleButton->setStyleSheet(buttonStyle);
    m_alignMiddleButton->setProperty(0, static_cast<int>(MainWindow::AlignMiddle));

    m_alignBottomButton = new QPushButton("⫰");
    m_alignBottomButton->setToolTip("Align Bottom");
    m_alignBottomButton->setStyleSheet(buttonStyle);
    m_alignBottomButton->setProperty(0, static_cast<int>(MainWindow::AlignBottom));

    // Layout the buttons in a 2x3 grid
    alignLayout->addWidget(m_alignLeftButton, 0, 0);
    alignLayout->addWidget(m_alignCenterButton, 0, 1);
    alignLayout->addWidget(m_alignRightButton, 0, 2);
    alignLayout->addWidget(m_alignTopButton, 1, 0);
    alignLayout->addWidget(m_alignMiddleButton, 1, 1);
    alignLayout->addWidget(m_alignBottomButton, 1, 2);

    m_mainLayout->addWidget(m_alignmentGroup);

    // Connect alignment buttons
    connect(m_alignLeftButton, &QPushButton::clicked, this, &AlignmentPanel::onAlignmentClicked);
    connect(m_alignCenterButton, &QPushButton::clicked, this, &AlignmentPanel::onAlignmentClicked);
    connect(m_alignRightButton, &QPushButton::clicked, this, &AlignmentPanel::onAlignmentClicked);
    connect(m_alignTopButton, &QPushButton::clicked, this, &AlignmentPanel::onAlignmentClicked);
    connect(m_alignMiddleButton, &QPushButton::clicked, this, &AlignmentPanel::onAlignmentClicked);
    connect(m_alignBottomButton, &QPushButton::clicked, this, &AlignmentPanel::onAlignmentClicked);
}

void AlignmentPanel::createDistributeGroup()
{
    m_distributeGroup = new QGroupBox("Distribute");
    m_distributeGroup->setStyleSheet(m_alignmentGroup->styleSheet());

    QHBoxLayout* distributeLayout = new QHBoxLayout(m_distributeGroup);
    distributeLayout->setSpacing(2);

    QString buttonStyle =
        "QPushButton {"
        "    background-color: #3E3E42;"
        "    color: white;"
        "    border: 1px solid #5A5A5C;"
        "    border-radius: 3px;"
        "    padding: 4px 6px;"
        "    font-size: 10px;"
        "}"
        "QPushButton:hover {"
        "    background-color: #4A4A4F;"
        "    border: 1px solid #007ACC;"
        "}"
        "QPushButton:pressed {"
        "    background-color: #007ACC;"
        "}";

    m_distributeHorizontalButton = new QPushButton("↔");
    m_distributeHorizontalButton->setToolTip("Distribute Horizontally");
    m_distributeHorizontalButton->setStyleSheet(buttonStyle);
    m_distributeHorizontalButton->setProperty(0, static_cast<int>(MainWindow::DistributeHorizontally));

    m_distributeVerticalButton = new QPushButton("↕");
    m_distributeVerticalButton->setToolTip("Distribute Vertically");
    m_distributeVerticalButton->setStyleSheet(buttonStyle);
    m_distributeVerticalButton->setProperty(0, static_cast<int>(MainWindow::DistributeVertically));

    distributeLayout->addWidget(m_distributeHorizontalButton);
    distributeLayout->addWidget(m_distributeVerticalButton);

    m_mainLayout->addWidget(m_distributeGroup);

    // Connect distribute buttons
    connect(m_distributeHorizontalButton, &QPushButton::clicked, this, &AlignmentPanel::onAlignmentClicked);
    connect(m_distributeVerticalButton, &QPushButton::clicked, this, &AlignmentPanel::onAlignmentClicked);
}

void AlignmentPanel::createArrangeGroup()
{
    m_arrangeGroup = new QGroupBox("Arrange");
    m_arrangeGroup->setStyleSheet(m_alignmentGroup->styleSheet());

    QGridLayout* arrangeLayout = new QGridLayout(m_arrangeGroup);
    arrangeLayout->setSpacing(2);

    QString buttonStyle =
        "QPushButton {"
        "    background-color: #3E3E42;"
        "    color: white;"
        "    border: 1px solid #5A5A5C;"
        "    border-radius: 3px;"
        "    padding: 4px;"
        "    font-size: 10px;"
        "    min-height: 20px;"
        "}"
        "QPushButton:hover {"
        "    background-color: #4A4A4F;"
        "    border: 1px solid #007ACC;"
        "}"
        "QPushButton:pressed {"
        "    background-color: #007ACC;"
        "}";

    m_bringToFrontButton = new QPushButton("⇈");
    m_bringToFrontButton->setToolTip("Bring to Front (Ctrl+Shift+])");
    m_bringToFrontButton->setStyleSheet(buttonStyle);

    m_bringForwardButton = new QPushButton("↑");
    m_bringForwardButton->setToolTip("Bring Forward (Ctrl+])");
    m_bringForwardButton->setStyleSheet(buttonStyle);

    m_sendBackwardButton = new QPushButton("↓");
    m_sendBackwardButton->setToolTip("Send Backward (Ctrl+[)");
    m_sendBackwardButton->setStyleSheet(buttonStyle);

    m_sendToBackButton = new QPushButton("⇊");
    m_sendToBackButton->setToolTip("Send to Back (Ctrl+Shift+[)");
    m_sendToBackButton->setStyleSheet(buttonStyle);

    arrangeLayout->addWidget(m_bringToFrontButton, 0, 0);
    arrangeLayout->addWidget(m_bringForwardButton, 0, 1);
    arrangeLayout->addWidget(m_sendBackwardButton, 1, 0);
    arrangeLayout->addWidget(m_sendToBackButton, 1, 1);

    m_mainLayout->addWidget(m_arrangeGroup);

    // Connect arrange buttons directly to main window methods
    connect(m_bringToFrontButton, &QPushButton::clicked, m_mainWindow, &MainWindow::bringToFront);
    connect(m_bringForwardButton, &QPushButton::clicked, m_mainWindow, &MainWindow::bringForward);
    connect(m_sendBackwardButton, &QPushButton::clicked, m_mainWindow, &MainWindow::sendBackward);
    connect(m_sendToBackButton, &QPushButton::clicked, m_mainWindow, &MainWindow::sendToBack);
}

void AlignmentPanel::onAlignmentClicked()
{
    QPushButton* button = qobject_cast<QPushButton*>(sender());
    if (button) {
        bool ok;
        int alignmentType = button->property("alignmentType").toInt(&ok); // Changed from data(0) to property()
        if (ok) {
            emit alignmentRequested(static_cast<MainWindow::AlignmentType>(alignmentType));
        }
    }
}
