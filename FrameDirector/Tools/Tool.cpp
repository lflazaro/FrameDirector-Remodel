// Tools/Tool.cpp
#include "Tool.h"
#include "../MainWindow.h"
#include "../Canvas.h"
#include <QMessageBox>
#include <QDebug>

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

// FIXED: Move implementations from header to here
void Tool::checkAutoConversion(Canvas* canvas, int layer, int frame)
{
    if (canvas && canvas->isExtendedFrame(frame, layer)) {
        qDebug() << "Auto-converting extended frame before drawing";
        canvas->convertExtendedFrameToKeyframe(frame, layer);
    }
}

// NEW: Check if drawing is allowed
bool Tool::canDrawOnCurrentFrame(Canvas* canvas, int layer, int frame)
{
    if (!canvas) return false;

    if (!canvas->canDrawOnFrame(frame, layer)) {
        // Show warning about tweening
        QMessageBox::information(nullptr, "Drawing Disabled",
            "Cannot draw on tweened frames. Remove tweening first or create a new keyframe.");
        return false;
    }

    return true;
}