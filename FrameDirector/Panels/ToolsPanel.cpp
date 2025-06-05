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
    m_activeTool = tool;
    QAbstractButton* button = m_toolButtonGroup->button(static_cast<int>(tool));
    if (button) {
        button->setChecked(true);
    }
}

MainWindow::ToolType ToolsPanel::getActiveTool() const
{
    return m_activeTool;
}

void ToolsPanel::onToolButtonClicked()
{
    QPushButton* button = qobject_cast<QPushButton*>(sender());
    if (button) {
        int toolId = m_toolButtonGroup->id(button);
        m_activeTool = static_cast<MainWindow::ToolType>(toolId);
        emit toolSelected(m_activeTool);
    }
}

void ToolsPanel::setDrawingToolsEnabled(bool enabled)
{
    m_drawingToolsEnabled = enabled;

    // Enable/disable drawing tool buttons
    if (m_drawButton) m_drawButton->setEnabled(enabled);
    if (m_lineButton) m_lineButton->setEnabled(enabled);
    if (m_rectangleButton) m_rectangleButton->setEnabled(enabled);
    if (m_ellipseButton) m_ellipseButton->setEnabled(enabled);
    if (m_textButton) m_textButton->setEnabled(enabled);
    if (m_bucketFillButton) m_bucketFillButton->setEnabled(enabled);
    if (m_eraseButton) m_eraseButton->setEnabled(enabled);

    // Visual feedback - gray out disabled tools
    QString styleSheet = enabled ? "" : "QToolButton:disabled { color: gray; }";
    setStyleSheet(styleSheet);
}