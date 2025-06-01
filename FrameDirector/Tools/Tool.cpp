#include "Tool.h"
#include "../MainWindow.h"
#include "../Canvas.h"

Tool::Tool(MainWindow* mainWindow, QObject* parent)
    : QObject(parent)
    , m_mainWindow(mainWindow)
    , m_canvas(nullptr)
{
    if (mainWindow) {
        m_canvas = mainWindow->findChild<Canvas*>();
    }
}