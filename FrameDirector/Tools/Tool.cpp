// Tools/Tool.cpp
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

// Helper method for tools to add items to the current layer
void Tool::addItemToCanvas(QGraphicsItem* item)
{
    if (m_canvas && item) {
        m_canvas->addItemToCurrentLayer(item);
        emit itemCreated(item);
    }
}