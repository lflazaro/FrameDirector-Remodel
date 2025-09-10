// Panels/ToolsPanel.cpp - Enhanced with drawing tool settings
#include "ToolsPanel.h"
#include "../Tools/DrawingTool.h"
#include <QButtonGroup>
#include <QPushButton>
#include <QVBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QIcon>
#include <QMenu>
#include <QContextMenuEvent>
#include <QDebug>

ToolsPanel::ToolsPanel(MainWindow* parent)
    : QWidget(parent)
    , m_mainWindow(parent)
    , m_activeTool(MainWindow::SelectTool)
    , m_toolsCurrentlyEnabled(true)
{
    setupUI();
}

void ToolsPanel::setupUI()
{
    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setContentsMargins(4, 4, 4, 4);
    m_mainLayout->setSpacing(2);

    // Tools header
    QLabel* headerLabel = new QLabel("Tools");
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

    // Tool buttons
    m_toolsLayout = new QGridLayout;
    m_toolsLayout->setSpacing(2);

    m_toolButtonGroup = new QButtonGroup(this);
    m_toolButtonGroup->setExclusive(true);

    // Create tool buttons with icons
    createToolButton(":/icons/tool-select.png", "Select Tool", MainWindow::SelectTool, "V");
    createToolButton(":/icons/tool-draw.png", "Draw Tool", MainWindow::DrawTool, "P");
    createToolButton(":/icons/tool-line.png", "Line Tool", MainWindow::LineTool, "L");
    createToolButton(":/icons/tool-rectangle.png", "Rectangle Tool", MainWindow::RectangleTool, "R");
    createToolButton(":/icons/tool-ellipse.png", "Ellipse Tool", MainWindow::EllipseTool, "O");
    createToolButton(":/icons/tool-text.png", "Text Tool", MainWindow::TextTool, "T");
    createToolButton(":/icons/tool-bucket.png", "Bucket Fill Tool", MainWindow::BucketFillTool, "B");
    createToolButton(":/icons/tool-gradient.png", "Gradient Fill Tool", MainWindow::GradientFillTool, "G");
    createToolButton(":/icons/tool-eraser.png", "Erase Tool", MainWindow::EraseTool, "E");

    m_mainLayout->addLayout(m_toolsLayout);
    m_mainLayout->addStretch();

    // Connect signals
    connect(m_toolButtonGroup, QOverload<QAbstractButton*>::of(&QButtonGroup::buttonClicked),
        this, [this](QAbstractButton* button) {
            MainWindow::ToolType tool = static_cast<MainWindow::ToolType>(
                m_toolButtonGroup->id(button));
            emit toolSelected(tool);
        });

    // Set default tool
    m_selectButton->setChecked(true);
}

void ToolsPanel::setToolsEnabled(bool enabled)
{
    if (m_toolsCurrentlyEnabled == enabled) {
        return; // No change needed
    }

    m_toolsCurrentlyEnabled = enabled;

    if (!enabled) {
        // Store original states before disabling
        m_originalEnabledStates.clear();
        m_originalEnabledStates[m_drawButton] = m_drawButton->isEnabled();
        m_originalEnabledStates[m_lineButton] = m_lineButton->isEnabled();
        m_originalEnabledStates[m_rectangleButton] = m_rectangleButton->isEnabled();
        m_originalEnabledStates[m_ellipseButton] = m_ellipseButton->isEnabled();
        m_originalEnabledStates[m_textButton] = m_textButton->isEnabled();
        m_originalEnabledStates[m_bucketFillButton] = m_bucketFillButton->isEnabled();
        m_originalEnabledStates[m_gradientFillButton] = m_gradientFillButton->isEnabled();
        m_originalEnabledStates[m_eraseButton] = m_eraseButton->isEnabled();

        // Disable drawing tools (keep select tool enabled)
        m_drawButton->setEnabled(false);
        m_lineButton->setEnabled(false);
        m_rectangleButton->setEnabled(false);
        m_ellipseButton->setEnabled(false);
        m_textButton->setEnabled(false);
        m_bucketFillButton->setEnabled(false);
        m_gradientFillButton->setEnabled(false);
        m_eraseButton->setEnabled(false);

        // FIX: Apply visual styling using property instead of styleSheet
        m_drawButton->setProperty("tweenDisabled", true);
        m_lineButton->setProperty("tweenDisabled", true);
        m_rectangleButton->setProperty("tweenDisabled", true);
        m_ellipseButton->setProperty("tweenDisabled", true);
        m_textButton->setProperty("tweenDisabled", true);
        m_bucketFillButton->setProperty("tweenDisabled", true);
        m_gradientFillButton->setProperty("tweenDisabled", true);
        m_eraseButton->setProperty("tweenDisabled", true);

        // Force update
        m_drawButton->update();
        m_lineButton->update();
        m_rectangleButton->update();
        m_ellipseButton->update();
        m_textButton->update();
        m_bucketFillButton->update();
        m_gradientFillButton->update();
        m_eraseButton->update();

        // Force selection tool if current tool is now disabled
        if (m_activeTool != MainWindow::SelectTool) {
            setActiveTool(MainWindow::SelectTool);
            if (m_mainWindow) {
                m_mainWindow->setTool(MainWindow::SelectTool);
            }
        }
    }
    else {
        // Restore original enabled states
        if (m_originalEnabledStates.contains(m_drawButton)) {
            m_drawButton->setEnabled(m_originalEnabledStates[m_drawButton]);
        }
        if (m_originalEnabledStates.contains(m_lineButton)) {
            m_lineButton->setEnabled(m_originalEnabledStates[m_lineButton]);
        }
        if (m_originalEnabledStates.contains(m_rectangleButton)) {
            m_rectangleButton->setEnabled(m_originalEnabledStates[m_rectangleButton]);
        }
        if (m_originalEnabledStates.contains(m_ellipseButton)) {
            m_ellipseButton->setEnabled(m_originalEnabledStates[m_ellipseButton]);
        }
        if (m_originalEnabledStates.contains(m_textButton)) {
            m_textButton->setEnabled(m_originalEnabledStates[m_textButton]);
        }
        if (m_originalEnabledStates.contains(m_bucketFillButton)) {
            m_bucketFillButton->setEnabled(m_originalEnabledStates[m_bucketFillButton]);
        }
        if (m_originalEnabledStates.contains(m_gradientFillButton)) {
            m_gradientFillButton->setEnabled(m_originalEnabledStates[m_gradientFillButton]);
        }
        if (m_originalEnabledStates.contains(m_eraseButton)) {
            m_eraseButton->setEnabled(m_originalEnabledStates[m_eraseButton]);
        }

        // FIX: Remove property and force size consistency
        m_drawButton->setProperty("tweenDisabled", QVariant());
        m_lineButton->setProperty("tweenDisabled", QVariant());
        m_rectangleButton->setProperty("tweenDisabled", QVariant());
        m_ellipseButton->setProperty("tweenDisabled", QVariant());
        m_textButton->setProperty("tweenDisabled", QVariant());
        m_bucketFillButton->setProperty("tweenDisabled", QVariant());
        m_gradientFillButton->setProperty("tweenDisabled", QVariant());
        m_eraseButton->setProperty("tweenDisabled", QVariant());

        // Force size consistency and update
        QSize buttonSize = m_selectButton->size();
        m_drawButton->setFixedSize(buttonSize);
        m_lineButton->setFixedSize(buttonSize);
        m_rectangleButton->setFixedSize(buttonSize);
        m_ellipseButton->setFixedSize(buttonSize);
        m_textButton->setFixedSize(buttonSize);
        m_bucketFillButton->setFixedSize(buttonSize);
        m_gradientFillButton->setFixedSize(buttonSize);
        m_eraseButton->setFixedSize(buttonSize);

        m_drawButton->update();
        m_lineButton->update();
        m_rectangleButton->update();
        m_ellipseButton->update();
        m_textButton->update();
        m_bucketFillButton->update();
        m_gradientFillButton->update();
        m_eraseButton->update();
    }
}


void ToolsPanel::createToolButton(const QString& iconPath, const QString& tooltip,
    MainWindow::ToolType tool, const QString& shortcut)
{
    QPushButton* button = new QPushButton();

    // Set icon instead of text
    QIcon icon(iconPath);
    button->setIcon(icon);
    button->setIconSize(QSize(24, 24));

    button->setToolTip(QString("%1 (%2)").arg(tooltip, shortcut));
    button->setCheckable(true);
    button->setMinimumSize(40, 40);
    button->setMaximumSize(40, 40);

    QString buttonStyle =
        "QPushButton {"
        "    background-color: #3E3E42;"
        "    color: white;"
        "    border: 1px solid #5A5A5C;"
        "    border-radius: 4px;"
        "    font-size: 14px;"
        "    font-weight: bold;"
        "    padding: 2px;"
        "}"
        "QPushButton:hover {"
        "    background-color: #4A4A4F;"
        "    border: 1px solid #007ACC;"
        "}"
        "QPushButton:checked {"
        "    background-color: #007ACC;"
        "    border: 1px solid #005A9B;"
        "}"
        "QPushButton:pressed {"
        "    background-color: #0E639C;"
        "}";

    button->setStyleSheet(buttonStyle);

    int row = static_cast<int>(tool) / 2;
    int col = static_cast<int>(tool) % 2;
    m_toolsLayout->addWidget(button, row, col);

    m_toolButtonGroup->addButton(button, static_cast<int>(tool));

    // Store button references
    switch (tool) {
    case MainWindow::SelectTool: m_selectButton = button; break;
    case MainWindow::DrawTool:
        m_drawButton = button;
        setupDrawToolContextMenu(button);
        break;
    case MainWindow::LineTool: m_lineButton = button; break;
    case MainWindow::RectangleTool: m_rectangleButton = button; break;
    case MainWindow::EllipseTool: m_ellipseButton = button; break;
    case MainWindow::TextTool: m_textButton = button; break;
    case MainWindow::BucketFillTool: m_bucketFillButton = button; break;
    case MainWindow::GradientFillTool: m_gradientFillButton = button; break;
    case MainWindow::EraseTool: m_eraseButton = button; break;
    }
}

void ToolsPanel::setupDrawToolContextMenu(QPushButton* drawButton)
{
    // Enable context menu for the draw tool button
    drawButton->setContextMenuPolicy(Qt::CustomContextMenu);

    connect(drawButton, &QPushButton::customContextMenuRequested,
        this, &ToolsPanel::showDrawToolContextMenu);
}

void ToolsPanel::showDrawToolContextMenu(const QPoint& pos)
{
    QPushButton* button = qobject_cast<QPushButton*>(sender());
    if (!button) return;

    QMenu contextMenu(this);
    contextMenu.setStyleSheet(
        "QMenu {"
        "    background-color: #3E3E42;"
        "    color: #FFFFFF;"
        "    border: 1px solid #5A5A5C;"
        "    border-radius: 3px;"
        "}"
        "QMenu::item {"
        "    padding: 8px 16px;"
        "    border: none;"
        "}"
        "QMenu::item:selected {"
        "    background-color: #4A4A4F;"
        "}"
        "QMenu::separator {"
        "    height: 1px;"
        "    background-color: #5A5A5C;"
        "    margin: 4px 8px;"
        "}"
    );

    // Add context menu actions
    QAction* settingsAction = contextMenu.addAction("Drawing Tool Settings...");
    settingsAction->setIcon(QIcon(":/icons/guides.png")); // Use guides icon for settings

    contextMenu.addSeparator();

    QAction* quickStrokeAction = contextMenu.addAction("Quick Stroke Width");
    QMenu* strokeSubMenu = new QMenu("Stroke Width", &contextMenu);
    strokeSubMenu->addAction("Thin (1px)");
    strokeSubMenu->addAction("Normal (2px)");
    strokeSubMenu->addAction("Thick (4px)");
    strokeSubMenu->addAction("Very Thick (8px)");
    quickStrokeAction->setMenu(strokeSubMenu);

    QAction* quickColorAction = contextMenu.addAction("Quick Colors");
    QMenu* colorSubMenu = new QMenu("Colors", &contextMenu);
    colorSubMenu->addAction("Black");
    colorSubMenu->addAction("White");
    colorSubMenu->addAction("Red");
    colorSubMenu->addAction("Blue");
    colorSubMenu->addAction("Green");
    quickColorAction->setMenu(colorSubMenu);

    // Show context menu
    QAction* selectedAction = contextMenu.exec(button->mapToGlobal(pos));

    if (selectedAction) {
        handleDrawToolMenuAction(selectedAction);
    }
}

void ToolsPanel::handleDrawToolMenuAction(QAction* action)
{
    QString actionText = action->text();

    if (actionText == "Drawing Tool Settings...") {
        // Get the drawing tool from the main window and show its settings
        if (m_mainWindow) {
            // Find the drawing tool in the main window's tools
            auto tools = m_mainWindow->findChildren<DrawingTool*>();
            for (DrawingTool* tool : tools) {
                tool->showSettingsDialog();
                break;
            }

            // Alternative: Access through the main window's tool system
            // This would require exposing the tools map or adding a method to MainWindow
            // For now, we'll implement a direct approach
            showDrawingToolSettings();
        }
    }
    else if (actionText.contains("px)")) {
        // Handle quick stroke width changes
        handleQuickStrokeWidth(actionText);
    }
    else if (actionText == "Black" || actionText == "White" ||
        actionText == "Red" || actionText == "Blue" || actionText == "Green") {
        // Handle quick color changes
        handleQuickColor(actionText);
    }
}

void ToolsPanel::showDrawingToolSettings()
{
    // This is a placeholder implementation
    // In a complete implementation, this would access the actual DrawingTool instance
    // and call its showSettingsDialog() method

    qDebug() << "Drawing tool settings requested - implement tool access";

    // For now, emit a signal that the main window can catch
    emit drawingToolSettingsRequested();
}

void ToolsPanel::handleQuickStrokeWidth(const QString& widthText)
{
    double width = 2.0; // default

    if (widthText.contains("Thin (1px)")) width = 1.0;
    else if (widthText.contains("Normal (2px)")) width = 2.0;
    else if (widthText.contains("Thick (4px)")) width = 4.0;
    else if (widthText.contains("Very Thick (8px)")) width = 8.0;

    emit quickStrokeWidthChanged(width);
}

void ToolsPanel::handleQuickColor(const QString& colorName)
{
    QColor color = Qt::black; // default

    if (colorName == "Black") color = Qt::black;
    else if (colorName == "White") color = Qt::white;
    else if (colorName == "Red") color = Qt::red;
    else if (colorName == "Blue") color = Qt::blue;
    else if (colorName == "Green") color = Qt::green;

    emit quickColorChanged(color);
}

void ToolsPanel::setActiveTool(MainWindow::ToolType tool)
{
    // Check if the tool can be activated
    QPushButton* targetButton = nullptr;
    switch (tool) {
    case MainWindow::DrawTool: targetButton = m_drawButton; break;
    case MainWindow::LineTool: targetButton = m_lineButton; break;
    case MainWindow::RectangleTool: targetButton = m_rectangleButton; break;
    case MainWindow::EllipseTool: targetButton = m_ellipseButton; break;
    case MainWindow::TextTool: targetButton = m_textButton; break;
    case MainWindow::BucketFillTool: targetButton = m_bucketFillButton; break;
    case MainWindow::GradientFillTool: targetButton = m_gradientFillButton; break;
    case MainWindow::EraseTool: targetButton = m_eraseButton; break;
    }

    // If target tool is disabled, force select tool
    if (targetButton && !targetButton->isEnabled() && tool != MainWindow::SelectTool) {
        tool = MainWindow::SelectTool;
        targetButton = m_selectButton;
    }

    if (m_activeTool != tool && targetButton) {
        m_activeTool = tool;

        // Update button states
        for (int i = 0; i < m_toolButtonGroup->buttons().size(); ++i) {
            QPushButton* btn = qobject_cast<QPushButton*>(m_toolButtonGroup->buttons()[i]);
            if (btn) {
                btn->setChecked(btn == targetButton);
            }
        }
    }
}

MainWindow::ToolType ToolsPanel::getActiveTool() const
{
    return m_activeTool;
}


void ToolsPanel::onToolButtonClicked()
{
    QPushButton* button = qobject_cast<QPushButton*>(sender());
    if (!button || !button->isEnabled()) {
        return; // Don't activate disabled tools
    }

    // Find which tool was clicked
    MainWindow::ToolType tool = MainWindow::SelectTool;

    if (button == m_selectButton) tool = MainWindow::SelectTool;
    else if (button == m_drawButton) tool = MainWindow::DrawTool;
    else if (button == m_lineButton) tool = MainWindow::LineTool;
    else if (button == m_rectangleButton) tool = MainWindow::RectangleTool;
    else if (button == m_ellipseButton) tool = MainWindow::EllipseTool;
    else if (button == m_textButton) tool = MainWindow::TextTool;
    else if (button == m_bucketFillButton) tool = MainWindow::BucketFillTool;
    else if (button == m_gradientFillButton) tool = MainWindow::GradientFillTool;
    else if (button == m_eraseButton) tool = MainWindow::EraseTool;

    setActiveTool(tool);
    emit toolChanged(tool);
}
