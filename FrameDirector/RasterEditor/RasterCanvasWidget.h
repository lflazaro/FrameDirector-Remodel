#pragma once

#include <QColor>
#include <QPointer>
#include <QWidget>

class RasterDocument;
class RasterTool;

class RasterCanvasWidget : public QWidget
{
    Q_OBJECT

public:
    explicit RasterCanvasWidget(QWidget* parent = nullptr);

    void setDocument(RasterDocument* document);
    RasterDocument* document() const { return m_document.data(); }

    void setActiveTool(RasterTool* tool);
    RasterTool* activeTool() const { return m_activeTool; }

    void setBackgroundColor(const QColor& color);
    QColor backgroundColor() const { return m_backgroundColor; }

    void setZoomFactor(qreal zoom);
    qreal zoomFactor() const { return m_zoomFactor; }

signals:
    void canvasClicked(const QPointF& position);

protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void leaveEvent(QEvent* event) override;

private slots:
    void onDocumentChanged();

private:
    QRectF canvasRectInWidget() const;
    QPointF mapToCanvas(const QPointF& pos) const;
    QPointF mapToActiveLayer(const QPointF& canvasPos) const;
    bool isInsideCanvas(const QPointF& canvasPos) const;
    void drawCheckerboard(QPainter& painter, const QSize& size);
    void drawFrameStack(QPainter& painter);
    void drawFrameComposite(QPainter& painter, int frameIndex, qreal opacity, const QColor& tint);

    QPointer<RasterDocument> m_document;
    RasterTool* m_activeTool;
    QColor m_backgroundColor;
    qreal m_zoomFactor;
    bool m_mouseDown;
    QPointF m_lastCanvasPosition;
};

