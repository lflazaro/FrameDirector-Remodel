#include "RasterTools.h"

#include "RasterDocument.h"

#include <QElapsedTimer>
#include <QImage>
#include <QColor>
#include <QDebug>
#include <QFile>
#include <QIODevice>
#include <QPainter>
#include <QPen>
#include <QRadialGradient>
#include <QPoint>
#include <QRect>
#include <QStack>
#include <QtMath>
#include <QtGlobal>
#include <cmath>
#include <algorithm>

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
            const qreal baseAlpha = qBound(0.0f, opaque, 1.0f) * color.alphaF();
            color.setAlphaF(baseAlpha);
            painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
            const qreal hardnessRatio = qBound<qreal>(hardness, 0.0, 1.0);
            if (hardnessRatio >= 0.999) {
                painter.setBrush(color);
            } else {
                const qreal maxRadius = qMax(rx, ry);
                QRadialGradient gradient(QPointF(0.0, 0.0), maxRadius);
                QColor edgeColor = color;
                edgeColor.setAlphaF(0.0);
                gradient.setColorAt(0.0, color);
                gradient.setColorAt(qBound<qreal>(0.0, hardnessRatio, 1.0), color);
                gradient.setColorAt(1.0, edgeColor);
                painter.setBrush(gradient);
            }
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
    , m_timer()
    , m_targetImage(nullptr)
    , m_brush(mypaint_brush_new())
    , m_useFallback(false)
    , m_opacity(1.0f)
    , m_hardness(1.0f)
    , m_spacing(0.25f)
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

double RasterBrushTool::computeElapsedSeconds(double deltaTimeSeconds)
{
    if (deltaTimeSeconds > 0.0) {
        if (!m_timer.isValid()) {
            m_timer.start();
        } else {
            m_timer.restart();
        }
        return deltaTimeSeconds;
    }

    if (!m_timer.isValid()) {
        m_timer.start();
        return 1.0 / 1000.0;
    }

    double elapsedSeconds = m_timer.restart() / 1000.0;
    if (elapsedSeconds <= 0.0) {
        elapsedSeconds = 1.0 / 1000.0;
    }
    return elapsedSeconds;
}

bool RasterBrushTool::loadBrushDefinition(const QString& resourcePath)
{
    if (!m_brush) {
        return false;
    }

    if (resourcePath.isEmpty()) {
        mypaint_brush_from_defaults(m_brush);
        return true;
    }

    QFile file(resourcePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "Failed to open brush resource" << resourcePath;
        mypaint_brush_from_defaults(m_brush);
        return false;
    }

    const QByteArray data = file.readAll();
    if (data.isEmpty()) {
        qWarning() << "Brush resource is empty" << resourcePath;
        mypaint_brush_from_defaults(m_brush);
        return false;
    }

    const gboolean loaded = mypaint_brush_from_string(m_brush, data.constData());
    if (!loaded) {
        qWarning() << "Failed to parse brush resource" << resourcePath;
        mypaint_brush_from_defaults(m_brush);
        return false;
    }

    return true;
}

int RasterBrushTool::applyMyPaintStroke(const QPointF& position, double deltaTimeSeconds)
{
    if (!m_surface || !m_brush) {
        return -1;
    }

    const double elapsedSeconds = computeElapsedSeconds(deltaTimeSeconds);
    const float pressure = 1.0f;

    const QPointF startPoint = m_lastPointValid ? m_lastPosition : position;
    const double dx = position.x() - startPoint.x();
    const double dy = position.y() - startPoint.y();
    const double distance = std::hypot(dx, dy);

    float actualRadius = mypaint_brush_get_state(m_brush, MYPAINT_BRUSH_STATE_ACTUAL_RADIUS);
    if (!std::isfinite(actualRadius) || actualRadius <= 0.0f) {
        const float baseRadiusLog = mypaint_brush_get_base_value(m_brush, MYPAINT_BRUSH_SETTING_RADIUS_LOGARITHMIC);
        actualRadius = std::exp(baseRadiusLog);
    }
    const double radius = std::max<double>(actualRadius, 1.0);

    float spacingFactor = 0.0f;
    const float dabsState = mypaint_brush_get_state(m_brush, MYPAINT_BRUSH_STATE_DABS_PER_ACTUAL_RADIUS);
    if (std::isfinite(dabsState) && dabsState > 0.0f) {
        spacingFactor = 1.0f / dabsState;
    }
    if (spacingFactor <= 0.0f) {
        const float dabsBase = mypaint_brush_get_base_value(m_brush, MYPAINT_BRUSH_SETTING_DABS_PER_ACTUAL_RADIUS);
        if (std::isfinite(dabsBase) && dabsBase > 0.0f) {
            spacingFactor = 1.0f / dabsBase;
        }
    }
    if (spacingFactor <= 0.0f) {
        spacingFactor = qMax(0.01f, m_spacing);
    }
    spacingFactor = qBound(0.01f, spacingFactor, 2.0f);

    int steps = 1;
    if (distance > 0.0) {
        const double stepDistance = std::max(radius * static_cast<double>(spacingFactor), 0.5);
        steps = std::max(1, static_cast<int>(std::ceil(distance / stepDistance)));
        steps = std::min(steps, 1024);
    }

    const QPointF stepDelta(steps > 0 ? dx / steps : 0.0, steps > 0 ? dy / steps : 0.0);
    const float timeSlice = static_cast<float>(std::max(elapsedSeconds / steps, 1e-6));

    QPointF current = startPoint;
    bool painted = false;
    int lastResult = 0;
    for (int i = 0; i < steps; ++i) {
        current += stepDelta;
        lastResult = mypaint_brush_stroke_to(m_brush, m_surface.get(), current.x(), current.y(), pressure, 0.0f, 0.0f, timeSlice);
        if (lastResult < 0) {
            return lastResult;
        }
        if (lastResult > 0) {
            expandDirtyRect(current, radius);
            painted = true;
        }
    }

    if (painted) {
        return 1;
    }

    return lastResult;
}

void RasterBrushTool::applyFallbackStroke(const QPointF& position, bool initial)
{
    if (!m_targetImage) {
        return;
    }

    const qreal radius = qMax<qreal>(m_size, 1.0);
    QPainter painter(m_targetImage);
    painter.setRenderHint(QPainter::Antialiasing, true);

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
    bool requireFallback = (!m_surface || !m_brush);
    if (!requireFallback) {
        const int result = applyMyPaintStroke(position, 0.0);
        if (result > 0) {
            painted = true;
        } else if (result == 0) {
            applyFallbackStroke(position, true);
            painted = true;
        } else {
            requireFallback = true;
        }
    }

    if (requireFallback) {
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
        const int result = applyMyPaintStroke(position, deltaTimeSeconds);
        if (result > 0) {
            painted = true;
        } else if (result == 0) {
            applyFallbackStroke(position, !m_lastPointValid);
            painted = true;
        } else {
            m_useFallback = true;
        }
    } else if (!m_useFallback) {
        m_useFallback = true;
    }

    if (m_useFallback) {
        applyFallbackStroke(position, !m_lastPointValid && !painted);
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

void RasterBrushTool::setOpacity(float value)
{
    const float clamped = qBound(0.0f, value, 1.0f);
    if (qFuzzyCompare(m_opacity, clamped)) {
        return;
    }

    m_opacity = clamped;
    if (m_brush) {
        mypaint_brush_set_base_value(m_brush, MYPAINT_BRUSH_SETTING_OPAQUE, m_opacity);
    }
}

void RasterBrushTool::setHardness(float value)
{
    const float clamped = qBound(0.0f, value, 1.0f);
    if (qFuzzyCompare(m_hardness, clamped)) {
        return;
    }

    m_hardness = clamped;
    if (m_brush) {
        mypaint_brush_set_base_value(m_brush, MYPAINT_BRUSH_SETTING_HARDNESS, m_hardness);
    }
}

void RasterBrushTool::setSpacing(float value)
{
    const float clamped = qBound(0.01f, value, 5.0f);
    if (qFuzzyCompare(m_spacing, clamped)) {
        return;
    }

    m_spacing = clamped;
    if (m_brush) {
        const float dabsPerRadius = 1.0f / qMax(m_spacing, 0.01f);
        mypaint_brush_set_base_value(m_brush, MYPAINT_BRUSH_SETTING_DABS_PER_ACTUAL_RADIUS, dabsPerRadius);
    }
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

void RasterBrushTool::applyPreset(const QVector<QPair<MyPaintBrushSetting, float>>& values, const QString& brushResource)
{
    if (!m_brush) {
        return;
    }

    loadBrushDefinition(brushResource);
    updateBrushParameters();

    for (const auto& value : values) {
        mypaint_brush_set_base_value(m_brush, value.first, value.second);
    }
}

void RasterBrushTool::updateBrushParameters()
{
    if (!m_brush) {
        return;
    }

    const float radius = qMax<qreal>(m_size, 1.0);
    mypaint_brush_set_base_value(m_brush, MYPAINT_BRUSH_SETTING_RADIUS_LOGARITHMIC, std::log(radius));
    mypaint_brush_set_base_value(m_brush, MYPAINT_BRUSH_SETTING_OPAQUE, m_opacity);
    mypaint_brush_set_base_value(m_brush, MYPAINT_BRUSH_SETTING_HARDNESS, m_hardness);
    const float dabsPerRadius = 1.0f / qMax(m_spacing, 0.01f);
    mypaint_brush_set_base_value(m_brush, MYPAINT_BRUSH_SETTING_DABS_PER_ACTUAL_RADIUS, dabsPerRadius);
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

