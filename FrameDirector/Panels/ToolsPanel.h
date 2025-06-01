#ifndef TOOLSPANEL_H
#define TOOLSPANEL_H

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QPushButton>
#include <QButtonGroup>
#include <QLabel>
#include "../MainWindow.h"

class ToolsPanel : public QWidget
{
    Q_OBJECT

public:
    explicit ToolsPanel(MainWindow* parent = nullptr);

    void setActiveTool(MainWindow::ToolType tool);
    MainWindow::ToolType getActiveTool() const;

signals:
    void toolSelected(MainWindow::ToolType tool);

private slots:
    void onToolButtonClicked();

private:
    void setupUI();
    void createToolButton(const QString& iconText, const QString& tooltip,
        MainWindow::ToolType tool, const QString& shortcut);

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
};

#endif // TOOLSPANEL_H