#include "RasterTools.h"

#include "RasterDocument.h"

#include <QElapsedTimer>
#include <QImage>
#include <QColor>
#include <QPainter>
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
    static int drawDab(MyPaintSurface* self, float x, float y,
                       float radius, float color_r, float color_g, float color_b,
                       float opaque, float hardness, float softness,
                       float alpha_eraser, float aspect_ratio, float angle,
                       float lock_alpha, float colorize, float posterize, float posterize_num,
                       float paint)
    {
        Q_UNUSED(color_r);
        Q_UNUSED(color_g);
        Q_UNUSED(color_b);
        Q_UNUSED(hardness);
        Q_UNUSED(softness);
        Q_UNUSED(lock_alpha);
        Q_UNUSED(colorize);
        Q_UNUSED(posterize);
        Q_UNUSED(posterize_num);
        Q_UNUSED(paint);

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

    static void getColor(MyPaintSurface* self, float x, float y, float radius,
                         float* color_r, float* color_g, float* color_b, float* color_a,
                         float paint)
    {
        Q_UNUSED(radius);
        Q_UNUSED(paint);
        auto* surface = static_cast<Surface*>(self);
        const QImage& image = surface->m_image;
        if (image.isNull()) {
            *color_r = *color_g = *color_b = *color_a = 0.0f;
            return;
        }

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
        const QPoint pt(qRound(x), qRound(y));
#else
        const QPoint pt(qRound(x), qRound(y));
#endif
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

    static void endAtomic(MyPaintSurface* self, MyPaintRectangles* roi)
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
    , m_activeStroke(false)
    , m_targetImage(nullptr)
    , m_brush(mypaint_brush_new())
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

void RasterBrushTool::beginStroke(RasterDocument* document, int layerIndex, int frameIndex, const QPointF& position)
{
    if (!document || !m_brush) {
        return;
    }

    QImage* image = document->frameImage(layerIndex, frameIndex);
    if (!image) {
        return;
    }

    RasterTool::beginStroke(document, layerIndex, frameIndex, position);
    m_targetImage = image;
    ensureSurface();
    if (!m_surface) {
        return;
    }

    image->detach();

    mypaint_brush_reset(m_brush);
    mypaint_brush_new_stroke(m_brush);
    m_timer.start();

    const float pressure = 1.0f;
    mypaint_brush_stroke_to(m_brush, m_surface.get(), position.x(), position.y(), pressure, 0.0f, 0.0f, 0.0, 1.0f, 0.0f, 0.0f, 1);
    expandDirtyRect(position, m_size);
    m_lastPosition = position;
    m_activeStroke = true;
}

void RasterBrushTool::strokeTo(const QPointF& position, double deltaTimeSeconds)
{
    if (!m_activeStroke || !m_surface || !m_brush) {
        return;
    }

    if (deltaTimeSeconds <= 0.0) {
        if (m_timer.isValid()) {
            deltaTimeSeconds = m_timer.restart() / 1000.0;
        }
    }

    const float pressure = 1.0f;
    mypaint_brush_stroke_to(m_brush, m_surface.get(), position.x(), position.y(), pressure, 0.0f, 0.0f,
                            deltaTimeSeconds, 1.0f, 0.0f, 0.0f, 1);
    expandDirtyRect(position, m_size);
    m_lastPosition = position;
}

void RasterBrushTool::endStroke()
{
    if (!m_activeStroke) {
        return;
    }

    m_activeStroke = false;
    m_surface.reset();
    m_targetImage = nullptr;
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

