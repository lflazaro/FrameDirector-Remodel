// Panels/ToolsPanel.h - Enhanced with drawing tool settings
#ifndef TOOLSPANEL_H
#define TOOLSPANEL_H

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QPushButton>
#include <QButtonGroup>
#include <QLabel>
#include <QMenu>
#include <QAction>
#include <QColor>
#include "../MainWindow.h"

class ToolsPanel : public QWidget
{
    Q_OBJECT

public:
    explicit ToolsPanel(MainWindow* parent = nullptr);

    void setActiveTool(MainWindow::ToolType tool);
    MainWindow::ToolType getActiveTool() const;
    // NEW: Method to enable/disable tools based on frame state
    void setToolsEnabled(bool enabled);

signals:
    void toolChanged(MainWindow::ToolType tool);
    void toolSelected(MainWindow::ToolType tool);
    void drawingToolSettingsRequested();
    void quickStrokeWidthChanged(double width);
    void quickColorChanged(const QColor& color);

private slots:
    void onToolButtonClicked();
    void showDrawToolContextMenu(const QPoint& pos);

private:
    void setupUI();
    void createToolButton(const QString& iconPath, const QString& tooltip,
        MainWindow::ToolType tool, const QString& shortcut);
    void setupDrawToolContextMenu(QPushButton* drawButton);
    void handleDrawToolMenuAction(QAction* action);
    void showDrawingToolSettings();
    void handleQuickStrokeWidth(const QString& widthText);
    void handleQuickColor(const QString& colorName);

    MainWindow* m_mainWindow;
    QVBoxLayout* m_mainLayout;
    QGridLayout* m_toolsLayout;
    QButtonGroup* m_toolButtonGroup;
    MainWindow::ToolType m_activeTool;

    QPushButton* m_selectButton;
    QPushButton* m_drawButton;
    QPushButton* m_lineButton;
    QPushButton* m_rectangleButton;
    QPushButton* m_ellipseButton;
    QPushButton* m_textButton;
    QPushButton* m_bucketFillButton;
    QPushButton* m_eraseButton;
    // NEW: Store original enabled states for restoration
    QMap<QPushButton*, bool> m_originalEnabledStates;
    bool m_toolsCurrentlyEnabled;
};

#endif // TOOLSPANEL_H