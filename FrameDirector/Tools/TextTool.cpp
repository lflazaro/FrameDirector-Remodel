// Tools/TextTool.cpp - Enhanced with undo support
#include "TextTool.h"
#include "../MainWindow.h"
#include "../Canvas.h"
#include "../Commands/UndoCommands.h"
#include <QGraphicsScene>
#include <QInputDialog>
#include <QFontDialog>
#include <QUndoStack>

TextTool::TextTool(MainWindow* mainWindow, QObject* parent)
    : Tool(mainWindow, parent)
    , m_currentText(nullptr)
{
    m_font = QFont("Arial", 12);
}

void TextTool::mousePressEvent(QMouseEvent* event, const QPointF& scenePos)
{
    if (!m_canvas) return;

    if (event->button() == Qt::LeftButton) {
        // Get text from user
        bool ok;
        QString text = QInputDialog::getText(m_mainWindow, "Add Text", "Enter text:",
            QLineEdit::Normal, "", &ok);

        if (ok && !text.isEmpty()) {
            m_currentText = new QGraphicsTextItem(text);
            m_currentText->setPos(scenePos);
            m_currentText->setFont(m_font);
            m_currentText->setDefaultTextColor(m_canvas->getStrokeColor());
            m_currentText->setFlags(QGraphicsItem::ItemIsSelectable |
                QGraphicsItem::ItemIsMovable |
                QGraphicsItem::ItemIsFocusable);
            m_currentText->setTextInteractionFlags(Qt::TextEditorInteraction);

            // Add through undo system
            if (m_mainWindow && m_mainWindow->m_undoStack) {
                DrawCommand* command = new DrawCommand(m_canvas, m_currentText);
                m_mainWindow->m_undoStack->push(command);
            }
            else {
                // Fallback: add directly
                addItemToCanvas(m_currentText);
            }
            m_currentText = nullptr;
        }
    }
}

void TextTool::mouseMoveEvent(QMouseEvent* event, const QPointF& scenePos)
{
    // Text tool doesn't need mouse move handling
}

void TextTool::mouseReleaseEvent(QMouseEvent* event, const QPointF& scenePos)
{
    // Text tool doesn't need mouse release handling
}

QCursor TextTool::getCursor() const
{
    return Qt::IBeamCursor;
}