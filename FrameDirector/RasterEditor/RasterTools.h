#pragma once

#include <QObject>
#include <QColor>
#include <QPointF>
#include <QRect>
#include <QElapsedTimer>
#include <memory>

#include <mypaint-brush.h>

class QImage;
class RasterDocument;

class RasterTool : public QObject
{
    Q_OBJECT

public:
    explicit RasterTool(QObject* parent = nullptr);
    ~RasterTool() override;

    virtual bool isStrokeTool() const = 0;

    virtual void beginStroke(RasterDocument* document, int layerIndex, int frameIndex, const QPointF& position);
    virtual void strokeTo(const QPointF& position, double deltaTimeSeconds = 0.0);
    virtual void endStroke();
    virtual void applyClick(RasterDocument* document, int layerIndex, int frameIndex, const QPointF& position);

    QRect dirtyRect() const { return m_dirtyRect; }

protected:
    void resetDirtyRect();
    void expandDirtyRect(const QPointF& position, qreal radius);

    RasterDocument* m_document;
    int m_layerIndex;
    int m_frameIndex;
    QRect m_dirtyRect;
};

class RasterBrushTool : public RasterTool
{
    Q_OBJECT

public:
    explicit RasterBrushTool(QObject* parent = nullptr);
    ~RasterBrushTool() override;

    bool isStrokeTool() const override { return true; }

    void beginStroke(RasterDocument* document, int layerIndex, int frameIndex, const QPointF& position) override;
    void strokeTo(const QPointF& position, double deltaTimeSeconds = 0.0) override;
    void endStroke() override;

    void setColor(const QColor& color);
    QColor color() const { return m_color; }

    void setSize(qreal size);
    qreal size() const { return m_size; }

    void setEraserMode(bool eraser);
    bool eraserMode() const { return m_eraserMode; }

protected:
    void updateBrushParameters();

private:
    struct Surface;

    void ensureSurface();

    std::unique_ptr<Surface> m_surface;

    QColor m_color;
    qreal m_size;
    bool m_eraserMode;
    QPointF m_lastPosition;
    bool m_activeStroke;
    QElapsedTimer m_timer;
    QImage* m_targetImage;
    MyPaintBrush* m_brush;
};

class RasterEraserTool : public RasterBrushTool
{
    Q_OBJECT

public:
    explicit RasterEraserTool(QObject* parent = nullptr);
};

class RasterFillTool : public RasterTool
{
    Q_OBJECT

public:
    explicit RasterFillTool(QObject* parent = nullptr);

    bool isStrokeTool() const override { return false; }

    void applyClick(RasterDocument* document, int layerIndex, int frameIndex, const QPointF& position) override;

    void setColor(const QColor& color) { m_color = color; }
    QColor color() const { return m_color; }

private:
    QColor m_color;
};

