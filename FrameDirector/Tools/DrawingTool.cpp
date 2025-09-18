// Tools/DrawingTool.cpp - Enhanced with stabilizer and undo support
#include "DrawingTool.h"
#include "../MainWindow.h"
#include "../Canvas.h"
#include "../Commands/UndoCommands.h"
#include <QGraphicsScene>
#include <QPen>
#include <QUndoStack>
#include <QDebug>
#include <QTimer>
#include <QLineF>
#include <QColorDialog>
#include <QSpinBox>
#include <QSlider>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QLabel>
#include <QPushButton>
#include <QGroupBox>
#include <QCheckBox>
#include <QtMath>

// Drawing Tool Settings Dialog
class DrawingToolSettingsDialog : public QDialog
{

public:
    explicit DrawingToolSettingsDialog(DrawingTool* tool, QWidget* parent = nullptr)
        : QDialog(parent), m_drawingTool(tool)
    {
        setWindowTitle("Drawing Tool Settings");
        setModal(true);
        setFixedSize(320, 280);

        setupUI();
        loadSettings();
        
        // Apply dark theme
        setStyleSheet(
            "QDialog {"
            "    background-color: #2D2D30;"
            "    color: #FFFFFF;"
            "}"
            "QGroupBox {"
            "    color: white;"
            "    font-weight: bold;"
            "    border: 1px solid #5A5A5C;"
            "    border-radius: 4px;"
            "    margin: 8px 0px;"
            "    padding-top: 8px;"
            "}"
            "QGroupBox::title {"
            "    subcontrol-origin: margin;"
            "    left: 8px;"
            "    padding: 0 4px 0 4px;"
            "}"
            "QLabel { color: #CCCCCC; }"
            "QSpinBox, QSlider {"
            "    background-color: #3E3E42;"
            "    color: white;"
            "    border: 1px solid #5A5A5C;"
            "    border-radius: 2px;"
            "}"
            "QPushButton {"
            "    background-color: #3E3E42;"
            "    color: white;"
            "    border: 1px solid #5A5A5C;"
            "    border-radius: 3px;"
            "    padding: 6px 12px;"
            "}"
            "QPushButton:hover {"
            "    background-color: #4A4A4F;"
            "    border: 1px solid #007ACC;"
            "}"
            "QPushButton:pressed {"
            "    background-color: #007ACC;"
            "}"
        );
    }

private:
    void onStrokeWidthChanged(double value)
    {
        if (m_drawingTool) {
            m_drawingTool->setStrokeWidth(value);
        }
    }

    void onStabilizerChanged(int value)
    {
        if (m_drawingTool) {
            m_drawingTool->setStabilizerAmount(value);
            int delayMs = m_drawingTool->getStabilizerDelayMs();

            if (value == 0) {
                m_stabilizerLabel->setText("Stabilizer: Off");
            }
            else {
                m_stabilizerLabel->setText(QString("Stabilizer: %1 (delay %2ms)")
                                               .arg(value)
                                               .arg(delayMs));
            }
        }
        else {
            m_stabilizerLabel->setText(QString("Stabilizer: %1").arg(value));
        }
    }

    void onColorButtonClicked()
    {
        if (!m_drawingTool) return;
        
        QColor color = QColorDialog::getColor(m_drawingTool->getStrokeColor(), this, "Select Stroke Color");
        if (color.isValid()) {
            m_drawingTool->setStrokeColor(color);
            updateColorButton(color);
        }
    }

    void onSmoothingToggled(bool enabled)
    {
        if (m_drawingTool) {
            m_drawingTool->setSmoothingEnabled(enabled);
        }
    }

    void onPressureSensitivityToggled(bool enabled)
    {
        if (m_drawingTool) {
            m_drawingTool->setPressureSensitivity(enabled);
        }
    }

private:
    void setupUI()
    {
        QVBoxLayout* mainLayout = new QVBoxLayout(this);
        
        // Stroke settings group
        QGroupBox* strokeGroup = new QGroupBox("Stroke Settings");
        QFormLayout* strokeLayout = new QFormLayout(strokeGroup);
        
        // Stroke width
        m_strokeWidthSpinBox = new QDoubleSpinBox;
        m_strokeWidthSpinBox->setRange(0.1, 50.0);
        m_strokeWidthSpinBox->setSingleStep(0.5);
        m_strokeWidthSpinBox->setSuffix(" px");
        strokeLayout->addRow("Stroke Width:", m_strokeWidthSpinBox);
        
        // Color button
        m_colorButton = new QPushButton("Choose Color");
        m_colorButton->setMinimumHeight(30);
        strokeLayout->addRow("Stroke Color:", m_colorButton);
        
        mainLayout->addWidget(strokeGroup);
        
        // Stabilizer settings group
        QGroupBox* stabilizerGroup = new QGroupBox("Stabilizer Settings");
        QVBoxLayout* stabilizerLayout = new QVBoxLayout(stabilizerGroup);
        
        m_stabilizerLabel = new QLabel("Stabilizer: Off");
        stabilizerLayout->addWidget(m_stabilizerLabel);
        
        m_stabilizerSlider = new QSlider(Qt::Horizontal);
        m_stabilizerSlider->setRange(0, 20);
        m_stabilizerSlider->setValue(0);
        stabilizerLayout->addWidget(m_stabilizerSlider);
        
        QLabel* stabilizerHint = new QLabel("Higher values = smoother strokes, more delay");
        stabilizerHint->setStyleSheet("color: #999999; font-size: 10px;");
        stabilizerLayout->addWidget(stabilizerHint);
        
        mainLayout->addWidget(stabilizerGroup);
        
        // Advanced settings group
        QGroupBox* advancedGroup = new QGroupBox("Advanced Settings");
        QVBoxLayout* advancedLayout = new QVBoxLayout(advancedGroup);
        
        m_smoothingCheckBox = new QCheckBox("Enable Path Smoothing");
        m_smoothingCheckBox->setChecked(true);
        advancedLayout->addWidget(m_smoothingCheckBox);
        
        m_pressureCheckBox = new QCheckBox("Pressure Sensitivity (if supported)");
        m_pressureCheckBox->setChecked(false);
        advancedLayout->addWidget(m_pressureCheckBox);
        
        mainLayout->addWidget(advancedGroup);
        
        // Buttons
        QHBoxLayout* buttonLayout = new QHBoxLayout;
        QPushButton* okButton = new QPushButton("OK");
        QPushButton* cancelButton = new QPushButton("Cancel");
        QPushButton* resetButton = new QPushButton("Reset");
        
        buttonLayout->addWidget(resetButton);
        buttonLayout->addStretch();
        buttonLayout->addWidget(cancelButton);
        buttonLayout->addWidget(okButton);
        
        mainLayout->addLayout(buttonLayout);
        
        // Connect signals
        connect(m_strokeWidthSpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                [this](double value) { onStrokeWidthChanged(value); });
        connect(m_stabilizerSlider, &QSlider::valueChanged,
                [this](int value) { onStabilizerChanged(value); });
        connect(m_colorButton, &QPushButton::clicked,
                [this]() { onColorButtonClicked(); });
        connect(m_smoothingCheckBox, &QCheckBox::toggled,
                [this](bool enabled) { onSmoothingToggled(enabled); });
        connect(m_pressureCheckBox, &QCheckBox::toggled,
                [this](bool enabled) { onPressureSensitivityToggled(enabled); });
        
        connect(okButton, &QPushButton::clicked, this, &QDialog::accept);
        connect(cancelButton, &QPushButton::clicked, this, &QDialog::reject);
        connect(resetButton, &QPushButton::clicked, [this]() { resetToDefaults(); });
    }
    
    void loadSettings()
    {
        if (!m_drawingTool) return;
        
        m_strokeWidthSpinBox->setValue(m_drawingTool->getStrokeWidth());
        m_stabilizerSlider->setValue(m_drawingTool->getStabilizerAmount());
        m_smoothingCheckBox->setChecked(m_drawingTool->isSmoothingEnabled());
        m_pressureCheckBox->setChecked(m_drawingTool->isPressureSensitive());
        
        updateColorButton(m_drawingTool->getStrokeColor());
        onStabilizerChanged(m_stabilizerSlider->value());
    }
    
    void updateColorButton(const QColor& color)
    {
        QString colorStyle = QString(
            "QPushButton {"
            "    background-color: %1;"
            "    color: %2;"
            "    border: 2px solid #5A5A5C;"
            "    border-radius: 3px;"
            "    padding: 6px 12px;"
            "}"
            "QPushButton:hover {"
            "    border: 2px solid #007ACC;"
            "}"
        ).arg(color.name()).arg(color.lightness() > 128 ? "black" : "white");
        
        m_colorButton->setStyleSheet(colorStyle);
    }
    
    void resetToDefaults()
    {
        m_strokeWidthSpinBox->setValue(2.0);
        m_stabilizerSlider->setValue(0);
        m_smoothingCheckBox->setChecked(true);
        m_pressureCheckBox->setChecked(false);
        
        if (m_drawingTool) {
            m_drawingTool->setStrokeColor(Qt::black);
            updateColorButton(Qt::black);
        }
    }

private:
    DrawingTool* m_drawingTool;
    QDoubleSpinBox* m_strokeWidthSpinBox;
    QSlider* m_stabilizerSlider;
    QPushButton* m_colorButton;
    QCheckBox* m_smoothingCheckBox;
    QCheckBox* m_pressureCheckBox;
    QLabel* m_stabilizerLabel;
};

// Enhanced DrawingTool Implementation
DrawingTool::DrawingTool(MainWindow* mainWindow, QObject* parent)
    : Tool(mainWindow, parent)
    , m_drawing(false)
    , m_currentPath(nullptr)
    , m_strokeWidth(2.0)
    , m_strokeColor(Qt::black)
    , m_stabilizerAmount(0)
    , m_smoothingEnabled(true)
    , m_pressureSensitive(false)
    , m_stabilizerTimer(new QTimer(this))
    , m_hasSmoothedPoint(false)
{
    // Setup stabilizer timer
    m_stabilizerTimer->setSingleShot(true);
    m_stabilizerTimer->setTimerType(Qt::PreciseTimer);
    connect(m_stabilizerTimer, &QTimer::timeout, this, &DrawingTool::onStabilizerTimeout);
    
    // Initialize stabilizer settings
    updateStabilizerDelay();
}

void DrawingTool::mousePressEvent(QMouseEvent* event, const QPointF& scenePos)
{
    if (!m_canvas) return;

    if (event->button() == Qt::LeftButton) {
        m_drawing = true;
        m_path = QPainterPath();
        m_path.moveTo(scenePos);
        m_lastPoint = scenePos;
        m_stabilizerTimer->stop();
        m_stabilizerPoints.clear();
        m_stabilizerPoints.append(scenePos);
        m_smoothedPoint = scenePos;
        m_hasSmoothedPoint = true;

        m_currentPath = new QGraphicsPathItem();
        m_currentPath->setPath(m_path);

        QPen pen(m_strokeColor, m_strokeWidth);
        pen.setCapStyle(Qt::RoundCap);
        pen.setJoinStyle(Qt::RoundJoin);
        m_currentPath->setPen(pen);

        m_currentPath->setFlags(QGraphicsItem::ItemIsSelectable | QGraphicsItem::ItemIsMovable);

        // Add to scene temporarily for preview
        m_canvas->scene()->addItem(m_currentPath);

        qDebug() << "DrawingTool: Started drawing at" << scenePos;
    }
}

void DrawingTool::mouseMoveEvent(QMouseEvent* event, const QPointF& scenePos)
{
    if (m_drawing && m_currentPath) {
        if (m_stabilizerAmount > 0) {
            // Add point to stabilizer buffer and process immediately
            m_stabilizerPoints.append(scenePos);
            processStabilizerPoints(false);
        } else {
            // No stabilization - draw immediately
            addPointToPath(scenePos);
        }
    }
}

void DrawingTool::mouseReleaseEvent(QMouseEvent* event, const QPointF& scenePos)
{
    if (event->button() == Qt::LeftButton && m_drawing) {
        m_drawing = false;
        
        // Process any remaining stabilizer points
        if (m_stabilizerAmount > 0) {
            m_stabilizerTimer->stop();
            if (m_stabilizerPoints.isEmpty() ||
                QLineF(m_stabilizerPoints.last(), scenePos).length() > 0.01) {
                m_stabilizerPoints.append(scenePos);
            }
            processStabilizerPoints(true); // Process remaining points
        }

        if (m_currentPath) {
            // Apply final smoothing if enabled
            if (m_smoothingEnabled) {
                applySmoothingToPath();
            }
            
            // Check if we have a valid path
            if (m_path.elementCount() > 1) {
                // Remove from scene temporarily
                m_canvas->scene()->removeItem(m_currentPath);

                // Add through undo system
                if (m_mainWindow && m_mainWindow->m_undoStack) {
                    DrawCommand* command = new DrawCommand(m_canvas, m_currentPath);
                    m_mainWindow->m_undoStack->push(command);
                    qDebug() << "DrawingTool: Added drawing to undo stack";
                }
                else {
                    // Fallback: add directly if undo system not available
                    qDebug() << "DrawingTool: Warning - undo stack not available, adding directly";
                    addItemToCanvas(m_currentPath);
                }
            }
            else {
                // Path too short, delete it
                m_canvas->scene()->removeItem(m_currentPath);
                delete m_currentPath;
                qDebug() << "DrawingTool: Path too short, discarded";
            }

            m_currentPath = nullptr;
        }

        m_path = QPainterPath();
        m_stabilizerPoints.clear();
        m_hasSmoothedPoint = false;
    }
}

void DrawingTool::onStabilizerTimeout()
{
    processStabilizerPoints(false);
}

void DrawingTool::processStabilizerPoints(bool forceFlush)
{
    if (m_stabilizerAmount <= 0 || m_stabilizerPoints.isEmpty())
        return;

    const double normalized = qBound(0.0, m_stabilizerAmount / 20.0, 1.0);
    const int kMinWindow = 3;
    const int kMaxWindow = 20;
    int desiredWindow = kMinWindow + qRound(normalized * 12.0);
    desiredWindow = qBound(kMinWindow, desiredWindow, kMaxWindow);

    int iterations = 1;
    if (forceFlush) {
        iterations = m_stabilizerPoints.size();
    } else {
        int backlog = qMax(0, m_stabilizerPoints.size() - desiredWindow);
        iterations = qBound(1, backlog + 1, 96);
    }

    for (int iter = 0; iter < iterations; ++iter) {
        if (m_stabilizerPoints.isEmpty())
            break;

        int buffered = m_stabilizerPoints.size();
        int windowSize = qMin(buffered, desiredWindow);
        if (windowSize <= 0)
            break;

        QPointF target(0.0, 0.0);
        double totalWeight = 0.0;
        const double weightStep = 0.55;
        for (int i = 0; i < windowSize; ++i) {
            double weight = 1.0 + i * weightStep;
            target += m_stabilizerPoints[buffered - 1 - i] * weight;
            totalWeight += weight;
        }

        if (totalWeight > 0.0) {
            target /= totalWeight;
        }

        if (!m_hasSmoothedPoint) {
            m_smoothedPoint = target;
            m_hasSmoothedPoint = true;
        }

        QPointF cursorPos = m_stabilizerPoints.last();
        double distanceToCursor = QLineF(m_smoothedPoint, cursorPos).length();

        double smoothingStrength = 0.25 + normalized * 0.6; // Higher values = smoother
        double follow = 1.0 - smoothingStrength;            // How fast we move toward the target
        follow = qBound(0.12, follow, 0.85);

        if (forceFlush) {
            follow = qMax(follow, 0.45);
        } else {
            if (buffered > windowSize) {
                double backlogRatio = qBound(0.0, double(buffered - windowSize) / windowSize, 1.0);
                double catchUp = follow + backlogRatio * (0.7 - follow);
                follow = qBound(follow, catchUp, 0.9);
            }

            double maxLag = 6.0 + normalized * 22.0;
            if (distanceToCursor > maxLag) {
                double over = distanceToCursor - maxLag;
                double ratio = qBound(0.0, over / (maxLag * 1.2), 1.0);
                double catchUp = follow + ratio * (0.85 - follow);
                follow = qBound(follow, catchUp, 0.9);
            }
        }

        QPointF newPoint = m_smoothedPoint + (target - m_smoothedPoint) * follow;

        if (distanceToCursor < 3.0) {
            double closeness = qBound(0.0, (3.0 - distanceToCursor) / 3.0, 1.0);
            newPoint = newPoint * (1.0 - closeness * 0.35) + cursorPos * (closeness * 0.35);
        }

        m_smoothedPoint = newPoint;
        addPointToPath(newPoint);

        m_stabilizerPoints.removeFirst();

        if (forceFlush) {
            if (m_stabilizerPoints.isEmpty())
                break;
        } else {
            if (m_stabilizerPoints.isEmpty())
                break;

            if (m_stabilizerPoints.size() <= 1)
                break;

            int remainingBacklog = qMax(0, m_stabilizerPoints.size() - desiredWindow);
            if (remainingBacklog <= 0)
                break;
        }
    }

    if (!forceFlush && m_drawing && m_stabilizerPoints.size() > 1) {
        m_stabilizerTimer->start();
    }
}


void DrawingTool::addPointToPath(const QPointF& point)
{
    if (!m_currentPath) return;
    
    // Only add points that are far enough apart to avoid too many tiny segments
    qreal distance = QLineF(m_lastPoint, point).length();
    if (distance >= 1.5) { // Minimum distance threshold
        m_path.lineTo(point);
        m_currentPath->setPath(m_path);
        m_lastPoint = point;
    }
}

void DrawingTool::applySmoothingToPath()
{
    if (!m_currentPath || m_path.elementCount() < 3) return;
    
    // Create smoothed path using quadratic curves
    QPainterPath smoothPath;
    
    if (m_path.elementCount() > 0) {
        QPainterPath::Element firstElement = m_path.elementAt(0);
        smoothPath.moveTo(firstElement.x, firstElement.y);
        
        for (int i = 1; i < m_path.elementCount() - 1; ++i) {
            QPainterPath::Element currentElement = m_path.elementAt(i);
            QPainterPath::Element nextElement = m_path.elementAt(i + 1);
            
            QPointF currentPoint(currentElement.x, currentElement.y);
            QPointF nextPoint(nextElement.x, nextElement.y);
            QPointF controlPoint = (currentPoint + nextPoint) / 2.0;
            
            smoothPath.quadTo(currentPoint, controlPoint);
        }
        
        // Add final point
        if (m_path.elementCount() > 1) {
            QPainterPath::Element lastElement = m_path.elementAt(m_path.elementCount() - 1);
            smoothPath.lineTo(lastElement.x, lastElement.y);
        }
        
        m_path = smoothPath;
        m_currentPath->setPath(m_path);
    }
}

void DrawingTool::updateStabilizerDelay()
{
    double normalized = qBound(0.0, m_stabilizerAmount / 20.0, 1.0);
    int delayMs = 8 + qRound(normalized * 24.0); // 8ms at low strength, up to ~32ms at max
    m_stabilizerTimer->setInterval(delayMs);
}

int DrawingTool::getStabilizerDelayMs() const
{
    return m_stabilizerTimer ? m_stabilizerTimer->interval() : 0;
}

void DrawingTool::showSettingsDialog()
{
    DrawingToolSettingsDialog dialog(this, m_mainWindow);
    dialog.exec();
}

// Getters and setters
void DrawingTool::setStrokeWidth(double width)
{
    m_strokeWidth = qBound(0.1, width, 50.0);
}

void DrawingTool::setStrokeColor(const QColor& color)
{
    m_strokeColor = color;
}

void DrawingTool::setStabilizerAmount(int amount)
{
    m_stabilizerAmount = qBound(0, amount, 20);
    updateStabilizerDelay();
}

void DrawingTool::setSmoothingEnabled(bool enabled)
{
    m_smoothingEnabled = enabled;
}

void DrawingTool::setPressureSensitivity(bool enabled)
{
    m_pressureSensitive = enabled;
}

double DrawingTool::getStrokeWidth() const
{
    return m_strokeWidth;
}

QColor DrawingTool::getStrokeColor() const
{
    return m_strokeColor;
}

int DrawingTool::getStabilizerAmount() const
{
    return m_stabilizerAmount;
}

bool DrawingTool::isSmoothingEnabled() const
{
    return m_smoothingEnabled;
}

bool DrawingTool::isPressureSensitive() const
{
    return m_pressureSensitive;
}

QCursor DrawingTool::getCursor() const
{
    return Qt::CrossCursor;
}