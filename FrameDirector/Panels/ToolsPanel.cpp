#include "ToolsPanel.h"
#include <QButtonGroup>
#include <QPushButton>
#include <QVBoxLayout>
#include <QGridLayout>
#include <QLabel>

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

    createToolButton("↖", "Select Tool (V)", MainWindow::SelectTool, "V");
    createToolButton("✏", "Draw Tool (P)", MainWindow::DrawTool, "P");
    createToolButton("╱", "Line Tool (L)", MainWindow::LineTool, "L");
    createToolButton("□", "Rectangle Tool (R)", MainWindow::RectangleTool, "R");
    createToolButton("○", "Ellipse Tool (O)", MainWindow::EllipseTool, "O");
    createToolButton("T", "Text Tool (T)", MainWindow::TextTool, "T");
    createToolButton("🪣", "Bucket Fill Tool (B)", MainWindow::BucketFillTool, "B");
    createToolButton("🗙", "Erase Tool (E)", MainWindow::EraseTool, "E");

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

void ToolsPanel::createToolButton(const QString& iconText, const QString& tooltip,
    MainWindow::ToolType tool, const QString& shortcut)
{
    QPushButton* button = new QPushButton(iconText);
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
    case MainWindow::DrawTool: m_drawButton = button; break;
    case MainWindow::LineTool: m_lineButton = button; break;
    case MainWindow::RectangleTool: m_rectangleButton = button; break;
    case MainWindow::EllipseTool: m_ellipseButton = button; break;
    case MainWindow::TextTool: m_textButton = button; break;
    case MainWindow::BucketFillTool: m_bucketFillButton = button; break;
    case MainWindow::EraseTool: m_eraseButton = button; break;
    }
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