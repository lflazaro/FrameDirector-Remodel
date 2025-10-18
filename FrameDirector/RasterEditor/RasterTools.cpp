#include "RasterTools.h"

#include "RasterDocument.h"

#include <QElapsedTimer>
#include <QImage>
#include <QColor>
#include <QPainter>
#include <QPen>
#include <QPoint>
#include <QRect>
#include <QStack>
#include <QtMath>
#include <QtGlobal>
#include <cmath>

namespace
{
constexpr qreal kDefaultBrushSize = 12.0;
constexpr int kMaxFillIterations = 1'000'000;
}

class RasterBrushTool::Surface : public MyPaintSurface
{
public:
    explicit Surface(QImage& image)
        : m_image(image)
        , m_color(Qt::black)
        , m_eraser(false)
    {
        mypaint_surface_init(this);
        draw_dab = &Surface::drawDab;
        get_color = &Surface::getColor;
        begin_atomic = &Surface::beginAtomic;
        end_atomic = &Surface::endAtomic;
        destroy = nullptr;
        save_png = nullptr;
    }

    void setColor(const QColor& color) { m_color = color; }
    void setEraser(bool eraser) { m_eraser = eraser; }

private:
    // Match MyPaintSurfaceDrawDabFunction signature (no posterize/paint extras)
    static int drawDab(MyPaintSurface* self, float x, float y,
                       float radius, float color_r, float color_g, float color_b,
                       float opaque, float hardness,
                       float alpha_eraser,
                       float aspect_ratio, float angle,
                       float lock_alpha, float colorize)
    {
        Q_UNUSED(color_r);
        Q_UNUSED(color_g);
        Q_UNUSED(color_b);
        Q_UNUSED(hardness);
        Q_UNUSED(lock_alpha);
        Q_UNUSED(colorize);

        auto* surface = static_cast<Surface*>(self);
        QImage& image = surface->m_image;
        if (image.isNull()) {
            return 0;
        }

        QPainter painter(&image);
        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.setPen(Qt::NoPen);

        const qreal rx = radius * (aspect_ratio > 0.0f ? aspect_ratio : 1.0f);
        const qreal ry = radius;

        if (surface->m_eraser || alpha_eraser > 0.0f) {
            const qreal alpha = qBound(0.0f, alpha_eraser > 0.0f ? alpha_eraser : opaque, 1.0f);
            painter.setCompositionMode(QPainter::CompositionMode_Clear);
            painter.setBrush(QColor(0, 0, 0, static_cast<int>(alpha * 255.0f)));
        } else {
            QColor color = surface->m_color;
            color.setAlphaF(qBound(0.0f, opaque, 1.0f) * color.alphaF());
            painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
            painter.setBrush(color);
        }

        painter.translate(x, y);
        if (!qFuzzyIsNull(angle)) {
            painter.rotate(qRadiansToDegrees(angle));
        }
        painter.drawEllipse(QPointF(0.0, 0.0), rx, ry);
        return 1;
    }

    // Match MyPaintSurfaceGetColorFunction signature (no 'paint' param)
    static void getColor(MyPaintSurface* self, float x, float y, float radius,
                         float* color_r, float* color_g, float* color_b, float* color_a)
    {
        Q_UNUSED(radius);
        auto* surface = static_cast<Surface*>(self);
        const QImage& image = surface->m_image;
        if (image.isNull()) {
            *color_r = *color_g = *color_b = *color_a = 0.0f;
            return;
        }

        const QPoint pt(qRound(x), qRound(y));
        if (!QRect(QPoint(0, 0), image.size()).contains(pt)) {
            *color_r = *color_g = *color_b = *color_a = 0.0f;
            return;
        }

        const QColor color = image.pixelColor(pt);
        *color_r = color.redF();
        *color_g = color.greenF();
        *color_b = color.blueF();
        *color_a = color.alphaF();
    }

    static void beginAtomic(MyPaintSurface* self)
    {
        Q_UNUSED(self);
    }

    // Match MyPaintSurfaceEndAtomicFunction signature: MyPaintRectangle*
    static void endAtomic(MyPaintSurface* self, MyPaintRectangle* roi)
    {
        Q_UNUSED(self);
        Q_UNUSED(roi);
    }

    QImage& m_image;
    QColor m_color;
    bool m_eraser;
};

RasterTool::RasterTool(QObject* parent)
    : QObject(parent)
    , m_document(nullptr)
    , m_layerIndex(-1)
    , m_frameIndex(-1)
    , m_dirtyRect()
{
}

RasterTool::~RasterTool() = default;

void RasterTool::beginStroke(RasterDocument* document, int layerIndex, int frameIndex, const QPointF& position)
{
    m_document = document;
    m_layerIndex = layerIndex;
    m_frameIndex = frameIndex;
    resetDirtyRect();
    Q_UNUSED(position);
}

void RasterTool::strokeTo(const QPointF& position, double deltaTimeSeconds)
{
    Q_UNUSED(position);
    Q_UNUSED(deltaTimeSeconds);
}

void RasterTool::endStroke()
{
}

void RasterTool::applyClick(RasterDocument* document, int layerIndex, int frameIndex, const QPointF& position)
{
    beginStroke(document, layerIndex, frameIndex, position);
    endStroke();
}

void RasterTool::resetDirtyRect()
{
    m_dirtyRect = QRect();
}

void RasterTool::expandDirtyRect(const QPointF& position, qreal radius)
{
    const qreal r = qMax<qreal>(radius, 1.0);
    const QRectF rect(position.x() - r, position.y() - r, r * 2.0, r * 2.0);
    if (m_dirtyRect.isNull()) {
        m_dirtyRect = rect.toAlignedRect();
    } else {
        m_dirtyRect = m_dirtyRect.united(rect.toAlignedRect());
    }
}

RasterBrushTool::RasterBrushTool(QObject* parent)
    : RasterTool(parent)
    , m_surface()
    , m_color(Qt::black)
    , m_size(kDefaultBrushSize)
    , m_eraserMode(false)
    , m_lastPosition()
    , m_lastPointValid(false)
    , m_activeStroke(false)
    , m_targetImage(nullptr)
    , m_brush(mypaint_brush_new())
    , m_useFallback(false)
{
    if (m_brush) {
        mypaint_brush_from_defaults(m_brush);
        updateBrushParameters();
    }
}

RasterBrushTool::~RasterBrushTool()
{
    if (m_brush) {
        mypaint_brush_unref(m_brush);
        m_brush = nullptr;
    }
}

void RasterBrushTool::ensureSurface()
{
    if (!m_targetImage) {
        m_surface.reset();
        return;
    }

    m_surface = std::make_unique<Surface>(*m_targetImage);
    m_surface->setColor(m_color);
    m_surface->setEraser(m_eraserMode);
}

bool RasterBrushTool::applyMyPaintStroke(const QPointF& position, double deltaTimeSeconds)
{
    if (!m_surface || !m_brush) {
        return false;
    }

    double elapsedSeconds = deltaTimeSeconds;
    if (elapsedSeconds <= 0.0) {
        elapsedSeconds = m_timer.isValid() ? m_timer.restart() / 1000.0 : 0.0;
    }

    const float pressure = 1.0f;
    const int result = mypaint_brush_stroke_to(m_brush, m_surface.get(), position.x(), position.y(), pressure, 0.0f, 0.0f, elapsedSeconds);
    if (result <= 0) {
        return false;
    }

    expandDirtyRect(position, m_size);
    return true;
}

void RasterBrushTool::applyFallbackStroke(const QPointF& position, bool initial)
{
    if (!m_targetImage) {
        return;
    }

    const qreal radius = qMax<qreal>(m_size, 1.0);
    QPainter painter(m_targetImage);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::HighQualityAntialiasing, true);

    if (!initial && m_lastPointValid) {
        painter.save();
        painter.setBrush(Qt::NoBrush);
        if (m_eraserMode) {
            painter.setCompositionMode(QPainter::CompositionMode_Clear);
            QPen pen(Qt::transparent, radius * 2.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
            painter.setPen(pen);
        } else {
            QPen pen(m_color, radius * 2.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
            painter.setPen(pen);
            painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
        }
        painter.drawLine(m_lastPosition, position);
        painter.restore();
        expandDirtyRect(m_lastPosition, radius);
    }

    painter.save();
    painter.setPen(Qt::NoPen);
    if (m_eraserMode) {
        painter.setCompositionMode(QPainter::CompositionMode_Clear);
        painter.setBrush(Qt::transparent);
    } else {
        painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
        painter.setBrush(m_color);
    }
    painter.drawEllipse(QRectF(position.x() - radius, position.y() - radius, radius * 2.0, radius * 2.0));
    painter.restore();

    expandDirtyRect(position, radius);
}

void RasterBrushTool::beginStroke(RasterDocument* document, int layerIndex, int frameIndex, const QPointF& position)
{
    if (!document) {
        return;
    }

    QImage* image = document->frameImage(layerIndex, frameIndex);
    if (!image) {
        return;
    }

    RasterTool::beginStroke(document, layerIndex, frameIndex, position);
    m_targetImage = image;
    m_lastPosition = position;
    m_lastPointValid = false;
    m_useFallback = false;
    ensureSurface();

    image->detach();

    if (m_brush) {
        mypaint_brush_reset(m_brush);
        mypaint_brush_new_stroke(m_brush);
    }

    m_timer.restart();
    m_activeStroke = true;

    bool painted = false;
    if (m_surface && m_brush) {
        painted = applyMyPaintStroke(position, 0.0);
    }

    if (!painted) {
        m_useFallback = true;
        applyFallbackStroke(position, true);
        painted = true;
    }

    if (painted) {
        m_lastPointValid = true;
    }
}

void RasterBrushTool::strokeTo(const QPointF& position, double deltaTimeSeconds)
{
    if (!m_activeStroke) {
        return;
    }

    bool painted = false;
    if (!m_useFallback && m_surface && m_brush) {
        painted = applyMyPaintStroke(position, deltaTimeSeconds);
        if (!painted) {
            m_useFallback = true;
        }
    }

    if (m_useFallback) {
        applyFallbackStroke(position, !m_lastPointValid);
        painted = true;
    }

    if (painted) {
        m_lastPosition = position;
        m_lastPointValid = true;
    }
}

void RasterBrushTool::endStroke()
{
    if (!m_activeStroke) {
        return;
    }

    m_activeStroke = false;
    m_surface.reset();
    m_targetImage = nullptr;
    m_lastPointValid = false;
    m_useFallback = false;
    m_timer.invalidate();
}

void RasterBrushTool::setColor(const QColor& color)
{
    if (m_color == color) {
        return;
    }

    m_color = color;
    if (m_surface) {
        m_surface->setColor(m_color);
    }
}

void RasterBrushTool::setSize(qreal size)
{
    const qreal clamped = qMax<qreal>(size, 1.0);
    if (qFuzzyCompare(m_size, clamped)) {
        return;
    }

    m_size = clamped;
    updateBrushParameters();
}

void RasterBrushTool::setEraserMode(bool eraser)
{
    if (m_eraserMode == eraser) {
        return;
    }

    m_eraserMode = eraser;
    if (m_surface) {
        m_surface->setEraser(m_eraserMode);
    }
}

void RasterBrushTool::updateBrushParameters()
{
    if (!m_brush) {
        return;
    }

    const float radius = qMax<qreal>(m_size, 1.0);
    mypaint_brush_set_base_value(m_brush, MYPAINT_BRUSH_SETTING_RADIUS_LOGARITHMIC, std::log(radius));
    mypaint_brush_set_base_value(m_brush, MYPAINT_BRUSH_SETTING_OPAQUE, 1.0f);
}

RasterEraserTool::RasterEraserTool(QObject* parent)
    : RasterBrushTool(parent)
{
    setEraserMode(true);
    setColor(QColor(0, 0, 0, 0));
}

RasterFillTool::RasterFillTool(QObject* parent)
    : RasterTool(parent)
    , m_color(Qt::black)
{
}

void RasterFillTool::applyClick(RasterDocument* document, int layerIndex, int frameIndex, const QPointF& position)
{
    resetDirtyRect();

    if (!document) {
        return;
    }

    QImage* image = document->frameImage(layerIndex, frameIndex);
    if (!image || image->isNull()) {
        return;
    }

    const QPoint seed(qFloor(position.x()), qFloor(position.y()));
    const QRect bounds(QPoint(0, 0), image->size());
    if (!bounds.contains(seed)) {
        return;
    }

    image->detach();

    const QRgb replacement = qPremultiply(m_color.rgba());
    const QRgb target = image->pixel(seed);
    if (target == replacement) {
        return;
    }

    QStack<QPoint> stack;
    stack.push(seed);
    QRect dirty(seed, QSize(1, 1));
    int iterations = 0;

    while (!stack.isEmpty()) {
        const QPoint point = stack.pop();
        if (!bounds.contains(point)) {
            continue;
        }

        QRgb current = image->pixel(point);
        if (current != target) {
            continue;
        }

        image->setPixel(point, replacement);
        dirty = dirty.united(QRect(point, QSize(1, 1)));

        stack.push(QPoint(point.x() + 1, point.y()));
        stack.push(QPoint(point.x() - 1, point.y()));
        stack.push(QPoint(point.x(), point.y() + 1));
        stack.push(QPoint(point.x(), point.y() - 1));

        if (++iterations > kMaxFillIterations) {
            break;
        }
    }

    m_dirtyRect = dirty.normalized();
}

