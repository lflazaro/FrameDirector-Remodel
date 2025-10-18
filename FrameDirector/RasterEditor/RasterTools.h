#pragma once

#include <QObject>
#include <QColor>
#include <QElapsedTimer>
#include <QPair>
#include <QPointF>
#include <QRect>
#include <QVector>
#include <memory>

#include <third_party/libmypaint/mypaint-brush.h>

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

    void setOpacity(float value);
    float opacity() const { return m_opacity; }

    void setHardness(float value);
    float hardness() const { return m_hardness; }

    void setSpacing(float value);
    float spacing() const { return m_spacing; }

    void setEraserMode(bool eraser);
    bool eraserMode() const { return m_eraserMode; }

    void applyPreset(const QVector<QPair<MyPaintBrushSetting, float>>& values);

protected:
    void updateBrushParameters();

private:
    struct Surface;

    void ensureSurface();
    int applyMyPaintStroke(const QPointF& position, double deltaTimeSeconds);
    void applyFallbackStroke(const QPointF& position, bool initial);

    std::unique_ptr<Surface> m_surface;

    QColor m_color;
    qreal m_size;
    bool m_eraserMode;
    QPointF m_lastPosition;
    bool m_lastPointValid;
    bool m_activeStroke;
    QElapsedTimer m_timer;
    QImage* m_targetImage;
    MyPaintBrush* m_brush;
    bool m_useFallback;
    float m_opacity;
    float m_hardness;
    float m_spacing;
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

