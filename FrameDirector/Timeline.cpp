// Timeline.cpp
#include "Timeline.h"
#include "MainWindow.h"
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QGridLayout>
#include <QScrollArea>
#include <QScrollBar>
#include <QPushButton>
#include <QSlider>
#include <QSpinBox>
#include <QComboBox>
#include <QLabel>
#include <QListWidget>
#include <QPainter>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QResizeEvent>
#include <QPaintEvent>
#include <QApplication>
#include <QStyle>

// TimelineDrawingArea implementation
TimelineDrawingArea::TimelineDrawingArea(QWidget* parent)
    : QWidget(parent), m_timeline(nullptr)
{
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setMinimumSize(800, 200);
}

void TimelineDrawingArea::paintEvent(QPaintEvent* event)
{
    if (!m_timeline) return;

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, false);

    QRect rect = event->rect();

    // Draw all timeline components
    m_timeline->drawTimelineBackground(&painter, rect);
    m_timeline->drawFrameRuler(&painter, rect);
    m_timeline->drawLayers(&painter, rect);
    m_timeline->drawKeyframes(&painter, rect);
    m_timeline->drawPlayhead(&painter, rect);
    m_timeline->drawSelection(&painter, rect);
}

void TimelineDrawingArea::mousePressEvent(QMouseEvent* event)
{
    if (!m_timeline) return;

    int mouseX = event->position().toPoint().x();
    int mouseY = event->position().toPoint().y();

    if (event->button() == Qt::LeftButton) {
        int frame = m_timeline->getFrameFromX(mouseX);
        int layer = m_timeline->getLayerFromY(mouseY);

        if (mouseX > m_timeline->getDrawingAreaRect().left() &&
            mouseY > m_timeline->getDrawingAreaRect().top()) {

            if (event->modifiers() & Qt::ControlModifier) {
                // Add/remove keyframe
                if (m_timeline->hasKeyframe(layer, frame)) {
                    m_timeline->removeKeyframe(layer, frame);
                }
                else {
                    m_timeline->addKeyframe(layer, frame);
                }
            }
            else {
                // Set current frame
                m_timeline->setCurrentFrame(frame);
                emit m_timeline->layerSelected(layer);
            }
        }
    }

    update();
}

void TimelineDrawingArea::mouseMoveEvent(QMouseEvent* event)
{
    if (!m_timeline) return;

    int mouseX = event->position().toPoint().x();

    if (event->buttons() & Qt::LeftButton &&
        mouseX > m_timeline->getDrawingAreaRect().left()) {
        int frame = m_timeline->getFrameFromX(mouseX);
        m_timeline->setCurrentFrame(frame);
    }

    update();
}

void TimelineDrawingArea::mouseReleaseEvent(QMouseEvent* event)
{
    // Handle mouse release
    update();
}

void TimelineDrawingArea::wheelEvent(QWheelEvent* event)
{
    if (!m_timeline) return;

    if (event->modifiers() & Qt::ControlModifier) {
        // Zoom
        double delta = event->angleDelta().y() / 120.0;
        double newZoom = m_timeline->getZoomLevel() * (1.0 + delta * 0.1);
        m_timeline->setZoomLevel(qBound(0.5, newZoom, 3.0));
    }
    else {
        // Scroll
        QWidget::wheelEvent(event);
    }
}

// Timeline implementation
Timeline::Timeline(MainWindow* parent)
    : QWidget(parent)
    , m_mainWindow(parent)
    , m_currentFrame(1)
    , m_totalFrames(100)
    , m_frameRate(24)
    , m_isPlaying(false)
    , m_zoomLevel(1.0)
    , m_scrollX(0)
    , m_scrollY(0)
    , m_frameWidth(12)
    , m_layerHeight(22)
    , m_rulerHeight(32)
    , m_layerPanelWidth(120)
    , m_dragging(false)
    , m_selectedLayer(-1)
{
    // Initialize colors (Flash-like dark theme)
    m_backgroundColor = QColor(32, 32, 32);           // Dark gray background
    m_frameColor = QColor(48, 48, 48);                // Slightly lighter frame areas
    m_keyframeColor = QColor(255, 165, 0);            // Orange keyframes like Flash
    m_selectedKeyframeColor = QColor(255, 200, 100);  // Light orange for selected
    m_playheadColor = QColor(255, 0, 0);              // Red playhead
    m_rulerColor = QColor(64, 64, 64);                // Dark ruler background
    m_layerColor = QColor(42, 42, 42);                // Layer background
    m_alternateLayerColor = QColor(38, 38, 38);       // Alternate layer color

    setupUI();
    setMinimumHeight(200);
    setMaximumHeight(400);

    // Connect signals
    connect(m_frameSlider, QOverload<int>::of(&QSlider::valueChanged),
        this, &Timeline::onFrameSliderChanged);
    connect(m_frameSpinBox, QOverload<int>::of(&QSpinBox::valueChanged),
        this, &Timeline::onFrameSpinBoxChanged);
    connect(m_frameRateCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
        this, &Timeline::onFrameRateChanged);
    connect(m_layerList, &QListWidget::currentRowChanged,
        this, &Timeline::onLayerSelectionChanged);
}

Timeline::~Timeline()
{
}

void Timeline::setupUI()
{
    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setContentsMargins(0, 0, 0, 0);
    m_mainLayout->setSpacing(0);

    setupControls();

    // Create main timeline area
    QHBoxLayout* timelineLayout = new QHBoxLayout;
    timelineLayout->setContentsMargins(0, 0, 0, 0);
    timelineLayout->setSpacing(0);

    // Layer panel on the left
    QVBoxLayout* layerPanelLayout = new QVBoxLayout;

    QLabel* layersLabel = new QLabel("Layers");
    layersLabel->setStyleSheet(
        "QLabel {"
        "    background-color: #404040;"
        "    color: #FFFFFF;"
        "    padding: 6px;"
        "    font-weight: bold;"
        "    font-size: 11px;"
        "    border-bottom: 1px solid #555555;"
        "}"
    );
    layersLabel->setAlignment(Qt::AlignCenter);
    layerPanelLayout->addWidget(layersLabel);

    m_layerList = new QListWidget;
    m_layerList->setMaximumWidth(m_layerPanelWidth);
    m_layerList->setMinimumWidth(m_layerPanelWidth);
    m_layerList->setStyleSheet(
        "QListWidget {"
        "    background-color: #2A2A2A;"
        "    color: #FFFFFF;"
        "    border: none;"
        "    border-right: 1px solid #555555;"
        "    selection-background-color: #4A90E2;"
        "    font-size: 11px;"
        "}"
        "QListWidget::item {"
        "    padding: 4px 8px;"
        "    border-bottom: 1px solid #353535;"
        "    min-height: 18px;"
        "}"
        "QListWidget::item:selected {"
        "    background-color: #4A90E2;"
        "    color: #FFFFFF;"
        "}"
        "QListWidget::item:hover {"
        "    background-color: #383838;"
        "}"
    );
    layerPanelLayout->addWidget(m_layerList);

    // Layer buttons
    QHBoxLayout* layerButtonsLayout = new QHBoxLayout;
    layerButtonsLayout->setContentsMargins(4, 4, 4, 4);

    m_addLayerButton = new QPushButton("+");
    m_removeLayerButton = new QPushButton("-");

    QString layerButtonStyle =
        "QPushButton {"
        "    background-color: #404040;"
        "    color: #FFFFFF;"
        "    border: 1px solid #555555;"
        "    border-radius: 2px;"
        "    padding: 2px 6px;"
        "    font-weight: bold;"
        "    font-size: 12px;"
        "    min-width: 20px;"
        "    max-width: 30px;"
        "    min-height: 18px;"
        "    max-height: 18px;"
        "}"
        "QPushButton:hover {"
        "    background-color: #4A4A4A;"
        "}"
        "QPushButton:pressed {"
        "    background-color: #353535;"
        "}";

    m_addLayerButton->setStyleSheet(layerButtonStyle);
    m_removeLayerButton->setStyleSheet(layerButtonStyle);

    layerButtonsLayout->addWidget(m_addLayerButton);
    layerButtonsLayout->addWidget(m_removeLayerButton);
    layerButtonsLayout->addStretch();
    layerPanelLayout->addLayout(layerButtonsLayout);

    QWidget* layerPanel = new QWidget;
    layerPanel->setLayout(layerPanelLayout);
    layerPanel->setMaximumWidth(m_layerPanelWidth);
    layerPanel->setStyleSheet("background-color: #2A2A2A; border-right: 1px solid #555555;");

    timelineLayout->addWidget(layerPanel);

    // Timeline drawing area
    m_drawingArea = new TimelineDrawingArea;
    m_drawingArea->setTimeline(this);
    m_drawingArea->setStyleSheet("background-color: #202020;");

    // Scroll area for timeline
    m_scrollArea = new QScrollArea;
    m_scrollArea->setWidget(m_drawingArea);
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_scrollArea->setStyleSheet(
        "QScrollArea {"
        "    background-color: #202020;"
        "    border: none;"
        "    border-left: 1px solid #555555;"
        "}"
        "QScrollBar:horizontal {"
        "    background-color: #303030;"
        "    height: 15px;"
        "    border: none;"
        "}"
        "QScrollBar::handle:horizontal {"
        "    background-color: #606060;"
        "    border-radius: 2px;"
        "    min-width: 20px;"
        "}"
        "QScrollBar::handle:horizontal:hover {"
        "    background-color: #707070;"
        "}"
        "QScrollBar:vertical {"
        "    background-color: #303030;"
        "    width: 15px;"
        "    border: none;"
        "}"
        "QScrollBar::handle:vertical {"
        "    background-color: #606060;"
        "    border-radius: 2px;"
        "    min-height: 20px;"
        "}"
        "QScrollBar::handle:vertical:hover {"
        "    background-color: #707070;"
        "}"
    );

    timelineLayout->addWidget(m_scrollArea);

    m_mainLayout->addLayout(timelineLayout, 1);

    // Connect layer buttons
    connect(m_addLayerButton, &QPushButton::clicked, [this]() {
        addLayer(QString("Layer %1").arg(m_layers.size() + 1));
        });

    connect(m_removeLayerButton, &QPushButton::clicked, [this]() {
        if (m_selectedLayer >= 0 && m_selectedLayer < m_layers.size()) {
            removeLayer(m_selectedLayer);
        }
        });

    // Add default layer
    addLayer("Layer 1");

    updateLayout();
}

void Timeline::setupControls()
{
    m_controlsLayout = new QHBoxLayout;
    m_controlsLayout->setContentsMargins(6, 4, 6, 4);
    m_controlsLayout->setSpacing(6);

    // Playback controls
    m_firstFrameButton = new QPushButton("⏮");
    m_prevFrameButton = new QPushButton("⏪");
    m_playButton = new QPushButton("▶");
    m_stopButton = new QPushButton("⏹");
    m_nextFrameButton = new QPushButton("⏩");
    m_lastFrameButton = new QPushButton("⏭");

    // Style buttons with Flash-like appearance
    QString buttonStyle =
        "QPushButton {"
        "    background-color: #404040;"
        "    color: #FFFFFF;"
        "    border: 1px solid #555555;"
        "    border-radius: 3px;"
        "    padding: 4px 8px;"
        "    font-size: 11px;"
        "    font-weight: bold;"
        "    min-width: 28px;"
        "    min-height: 22px;"
        "}"
        "QPushButton:hover {"
        "    background-color: #4A4A4A;"
        "    border: 1px solid #4A90E2;"
        "}"
        "QPushButton:pressed {"
        "    background-color: #353535;"
        "}"
        "QPushButton:checked {"
        "    background-color: #4A90E2;"
        "    border: 1px solid #6AA8F0;"
        "}";

    m_firstFrameButton->setStyleSheet(buttonStyle);
    m_prevFrameButton->setStyleSheet(buttonStyle);
    m_playButton->setStyleSheet(buttonStyle);
    m_stopButton->setStyleSheet(buttonStyle);
    m_nextFrameButton->setStyleSheet(buttonStyle);
    m_lastFrameButton->setStyleSheet(buttonStyle);

    m_controlsLayout->addWidget(m_firstFrameButton);
    m_controlsLayout->addWidget(m_prevFrameButton);
    m_controlsLayout->addWidget(m_playButton);
    m_controlsLayout->addWidget(m_stopButton);
    m_controlsLayout->addWidget(m_nextFrameButton);
    m_controlsLayout->addWidget(m_lastFrameButton);

    m_controlsLayout->addSpacing(15);

    // Frame controls
    m_frameLabel = new QLabel("Frame:");
    m_frameLabel->setStyleSheet("color: #CCCCCC; font-size: 11px; font-weight: bold;");

    m_frameSpinBox = new QSpinBox;
    m_frameSpinBox->setRange(1, m_totalFrames);
    m_frameSpinBox->setValue(m_currentFrame);
    m_frameSpinBox->setStyleSheet(
        "QSpinBox {"
        "    background-color: #353535;"
        "    color: #FFFFFF;"
        "    border: 1px solid #555555;"
        "    border-radius: 2px;"
        "    padding: 2px 4px;"
        "    font-size: 11px;"
        "    min-width: 40px;"
        "    max-width: 60px;"
        "}"
        "QSpinBox::up-button, QSpinBox::down-button {"
        "    background-color: #404040;"
        "    border: 1px solid #555555;"
        "    width: 12px;"
        "}"
        "QSpinBox::up-button:hover, QSpinBox::down-button:hover {"
        "    background-color: #4A4A4A;"
        "}"
    );

    m_totalFramesLabel = new QLabel(QString("/ %1").arg(m_totalFrames));
    m_totalFramesLabel->setStyleSheet("color: #999999; font-size: 11px;");

    m_controlsLayout->addWidget(m_frameLabel);
    m_controlsLayout->addWidget(m_frameSpinBox);
    m_controlsLayout->addWidget(m_totalFramesLabel);

    m_controlsLayout->addSpacing(15);

    // Frame rate
    QLabel* fpsLabel = new QLabel("FPS:");
    fpsLabel->setStyleSheet("color: #CCCCCC; font-size: 11px; font-weight: bold;");

    m_frameRateCombo = new QComboBox;
    m_frameRateCombo->addItems({ "12", "15", "24", "30", "60" });
    m_frameRateCombo->setCurrentText("24");
    m_frameRateCombo->setStyleSheet(
        "QComboBox {"
        "    background-color: #353535;"
        "    color: #FFFFFF;"
        "    border: 1px solid #555555;"
        "    border-radius: 2px;"
        "    padding: 2px 6px;"
        "    font-size: 11px;"
        "    min-width: 40px;"
        "}"
        "QComboBox::drop-down {"
        "    border: none;"
        "    width: 15px;"
        "}"
        "QComboBox::down-arrow {"
        "    image: none;"
        "    border-left: 4px solid transparent;"
        "    border-right: 4px solid transparent;"
        "    border-top: 4px solid #CCCCCC;"
        "}"
        "QComboBox QAbstractItemView {"
        "    background-color: #353535;"
        "    color: #FFFFFF;"
        "    border: 1px solid #555555;"
        "    selection-background-color: #4A90E2;"
        "}"
    );

    m_controlsLayout->addWidget(fpsLabel);
    m_controlsLayout->addWidget(m_frameRateCombo);

    m_controlsLayout->addStretch();

    // Frame slider
    m_frameSlider = new QSlider(Qt::Horizontal);
    m_frameSlider->setRange(1, m_totalFrames);
    m_frameSlider->setValue(m_currentFrame);
    m_frameSlider->setMinimumWidth(200);
    m_frameSlider->setStyleSheet(
        "QSlider::groove:horizontal {"
        "    background-color: #353535;"
        "    border: 1px solid #555555;"
        "    height: 6px;"
        "    border-radius: 3px;"
        "}"
        "QSlider::handle:horizontal {"
        "    background-color: #4A90E2;"
        "    border: 1px solid #6AA8F0;"
        "    width: 12px;"
        "    margin: -4px 0;"
        "    border-radius: 3px;"
        "}"
        "QSlider::handle:horizontal:hover {"
        "    background-color: #5AA0F2;"
        "}"
    );

    m_controlsLayout->addWidget(m_frameSlider);

    // Create controls widget
    QWidget* controlsWidget = new QWidget;
    controlsWidget->setLayout(m_controlsLayout);
    controlsWidget->setStyleSheet(
        "QWidget {"
        "    background-color: #404040;"
        "    border-bottom: 1px solid #555555;"
        "}"
    );
    controlsWidget->setMaximumHeight(36);

    m_mainLayout->addWidget(controlsWidget);

    // Connect playback buttons
    connect(m_firstFrameButton, &QPushButton::clicked, [this]() {
        setCurrentFrame(1);
        });

    connect(m_prevFrameButton, &QPushButton::clicked, [this]() {
        if (m_currentFrame > 1) {
            setCurrentFrame(m_currentFrame - 1);
        }
        });

    connect(m_nextFrameButton, &QPushButton::clicked, [this]() {
        if (m_currentFrame < m_totalFrames) {
            setCurrentFrame(m_currentFrame + 1);
        }
        });

    connect(m_lastFrameButton, &QPushButton::clicked, [this]() {
        setCurrentFrame(m_totalFrames);
        });

    connect(m_playButton, &QPushButton::clicked, [this]() {
        setPlaying(!m_isPlaying);
        });

    connect(m_stopButton, &QPushButton::clicked, [this]() {
        setPlaying(false);
        setCurrentFrame(1);
        });
}

// Rest of the methods (drawing, frame management, etc.)
void Timeline::drawTimelineBackground(QPainter* painter, const QRect& rect)
{
    painter->fillRect(rect, m_backgroundColor);
}

void Timeline::drawFrameRuler(QPainter* painter, const QRect& rect)
{
    QRect rulerRect(m_layerPanelWidth, 0, rect.width() - m_layerPanelWidth, m_rulerHeight);

    // Fill ruler background
    painter->fillRect(rulerRect, m_rulerColor);

    // Draw ruler border
    painter->setPen(QPen(QColor(85, 85, 85), 1));
    painter->drawLine(rulerRect.bottomLeft(), rulerRect.bottomRight());

    // Draw frame numbers and ticks
    painter->setPen(QPen(QColor(220, 220, 220), 1));
    painter->setFont(QFont("Arial", 9));

    int frameWidth = static_cast<int>(m_frameWidth * m_zoomLevel);
    int startFrame = qMax(1, m_scrollX / frameWidth);
    int endFrame = qMin(m_totalFrames, startFrame + rulerRect.width() / frameWidth + 1);

    for (int frame = startFrame; frame <= endFrame; ++frame) {
        int x = m_layerPanelWidth + (frame - 1) * frameWidth - m_scrollX;

        if (frame % 5 == 1 || frame == 1) {
            // Major tick and number
            painter->drawLine(x, rulerRect.bottom() - 12, x, rulerRect.bottom());
            painter->drawText(x + 2, rulerRect.bottom() - 14, QString::number(frame));
        }
        else {
            // Minor tick
            painter->drawLine(x, rulerRect.bottom() - 6, x, rulerRect.bottom());
        }

        // Draw frame separator
        painter->setPen(QPen(QColor(64, 64, 64), 1));
        painter->drawLine(x, m_rulerHeight, x, rect.bottom());
        painter->setPen(QPen(QColor(220, 220, 220), 1));
    }
}

void Timeline::drawLayers(QPainter* painter, const QRect& rect)
{
    for (int i = 0; i < m_layers.size(); ++i) {
        QRect layerRect = getLayerRect(i);
        layerRect.setLeft(m_layerPanelWidth);
        layerRect.setRight(rect.width());

        // Alternate layer colors
        QColor layerBg = (i % 2 == 0) ? m_layerColor : m_alternateLayerColor;
        painter->fillRect(layerRect, layerBg);

        // Draw layer separator
        painter->setPen(QPen(QColor(85, 85, 85), 1));
        painter->drawLine(layerRect.bottomLeft(), layerRect.bottomRight());
    }
}

void Timeline::drawKeyframes(QPainter* painter, const QRect& rect)
{
    int frameWidth = static_cast<int>(m_frameWidth * m_zoomLevel);

    for (const auto& keyframe : m_keyframes) {
        QRect frameRect = getFrameRect(keyframe.frame);
        QRect layerRect = getLayerRect(keyframe.layer);

        if (frameRect.isEmpty() || layerRect.isEmpty()) continue;

        int x = m_layerPanelWidth + (keyframe.frame - 1) * frameWidth - m_scrollX;
        int y = layerRect.center().y();

        QColor color = keyframe.selected ? m_selectedKeyframeColor : m_keyframeColor;

        // Draw keyframe diamond (Flash-style)
        painter->setBrush(QBrush(color));
        painter->setPen(QPen(color.darker(120), 1));

        QPolygon diamond;
        diamond << QPoint(x, y - 6)      // Top
            << QPoint(x + 6, y)      // Right
            << QPoint(x, y + 6)      // Bottom
            << QPoint(x - 6, y);     // Left

        painter->drawPolygon(diamond);
    }
}

void Timeline::drawPlayhead(QPainter* painter, const QRect& rect)
{
    int frameWidth = static_cast<int>(m_frameWidth * m_zoomLevel);
    int x = m_layerPanelWidth + (m_currentFrame - 1) * frameWidth - m_scrollX;

    // Draw playhead line
    painter->setPen(QPen(m_playheadColor, 2));
    painter->drawLine(x, m_rulerHeight, x, rect.height());

    // Draw playhead handle
    QPolygon handle;
    handle << QPoint(x - 8, m_rulerHeight - 2)
        << QPoint(x + 8, m_rulerHeight - 2)
        << QPoint(x, m_rulerHeight + 10);

    painter->setBrush(QBrush(m_playheadColor));
    painter->setPen(QPen(m_playheadColor.darker(120), 1));
    painter->drawPolygon(handle);
}

void Timeline::drawSelection(QPainter* painter, const QRect& rect)
{
    if (m_selectedLayer >= 0 && m_selectedLayer < m_layers.size()) {
        QRect layerRect = getLayerRect(m_selectedLayer);
        layerRect.setLeft(0);
        layerRect.setRight(m_layerPanelWidth);

        painter->fillRect(layerRect, QColor(74, 144, 226, 60));
        painter->setPen(QPen(QColor(74, 144, 226), 2));
        painter->drawRect(layerRect);
    }
}

// Implementation of other methods continues...
QRect Timeline::getFrameRect(int frame) const
{
    int frameWidth = static_cast<int>(m_frameWidth * m_zoomLevel);
    int x = m_layerPanelWidth + (frame - 1) * frameWidth - m_scrollX;
    return QRect(x, m_rulerHeight, frameWidth, height() - m_rulerHeight);
}

QRect Timeline::getLayerRect(int layer) const
{
    if (layer < 0 || layer >= m_layers.size()) return QRect();

    int y = m_rulerHeight + layer * m_layerHeight - m_scrollY;
    return QRect(0, y, width(), m_layerHeight);
}

QRect Timeline::getDrawingAreaRect() const
{
    return QRect(m_layerPanelWidth, m_rulerHeight, width() - m_layerPanelWidth, height() - m_rulerHeight);
}

int Timeline::getFrameFromX(int x) const
{
    int frameWidth = static_cast<int>(m_frameWidth * m_zoomLevel);
    int adjustedX = x - m_layerPanelWidth + m_scrollX;
    return qMax(1, qMin(m_totalFrames, adjustedX / frameWidth + 1));
}

int Timeline::getLayerFromY(int y) const
{
    int adjustedY = y - m_rulerHeight + m_scrollY;
    int layer = adjustedY / m_layerHeight;
    return qMax(0, qMin(static_cast<int>(m_layers.size()) - 1, layer));
}

void Timeline::setCurrentFrame(int frame)
{
    if (frame != m_currentFrame && frame >= 1 && frame <= m_totalFrames) {
        m_currentFrame = frame;
        m_frameSpinBox->setValue(frame);
        m_frameSlider->setValue(frame);
        if (m_drawingArea) {
            m_drawingArea->update();
        }
        emit frameChanged(frame);
    }
}

int Timeline::getCurrentFrame() const
{
    return m_currentFrame;
}

void Timeline::setTotalFrames(int frames)
{
    if (frames != m_totalFrames && frames > 0) {
        m_totalFrames = frames;
        m_frameSpinBox->setRange(1, frames);
        m_frameSlider->setRange(1, frames);
        m_totalFramesLabel->setText(QString("/ %1").arg(frames));
        updateLayout();
        if (m_drawingArea) {
            m_drawingArea->update();
        }
    }
}

int Timeline::getTotalFrames() const
{
    return m_totalFrames;
}

void Timeline::setFrameRate(int fps)
{
    if (fps != m_frameRate && fps > 0) {
        m_frameRate = fps;
        m_frameRateCombo->setCurrentText(QString::number(fps));
        emit frameRateChanged(fps);
    }
}

int Timeline::getFrameRate() const
{
    return m_frameRate;
}

void Timeline::setPlaying(bool playing)
{
    if (playing != m_isPlaying) {
        m_isPlaying = playing;
        m_playButton->setText(playing ? "⏸" : "▶");
    }
}

bool Timeline::isPlaying() const
{
    return m_isPlaying;
}

void Timeline::addKeyframe(int layer, int frame)
{
    if (layer >= 0 && layer < m_layers.size() && frame >= 1 && frame <= m_totalFrames) {
        for (const auto& kf : m_keyframes) {
            if (kf.layer == layer && kf.frame == frame) {
                return; // Already exists
            }
        }

        Keyframe keyframe;
        keyframe.layer = layer;
        keyframe.frame = frame;
        keyframe.selected = false;
        keyframe.color = m_keyframeColor;

        m_keyframes.push_back(keyframe);
        if (m_drawingArea) {
            m_drawingArea->update();
        }
        emit keyframeAdded(layer, frame);
    }
}

void Timeline::removeKeyframe(int layer, int frame)
{
    auto it = std::remove_if(m_keyframes.begin(), m_keyframes.end(),
        [layer, frame](const Keyframe& kf) {
            return kf.layer == layer && kf.frame == frame;
        });

    if (it != m_keyframes.end()) {
        m_keyframes.erase(it, m_keyframes.end());
        if (m_drawingArea) {
            m_drawingArea->update();
        }
        emit keyframeRemoved(layer, frame);
    }
}

bool Timeline::hasKeyframe(int layer, int frame) const
{
    for (const auto& kf : m_keyframes) {
        if (kf.layer == layer && kf.frame == frame) {
            return true;
        }
    }
    return false;
}

void Timeline::addLayer(const QString& name)
{
    Layer layer;
    layer.name = name;
    layer.visible = true;
    layer.locked = false;
    layer.color = (m_layers.size() % 2 == 0) ? m_layerColor : m_alternateLayerColor;

    m_layers.push_back(layer);

    QListWidgetItem* item = new QListWidgetItem(name);
    item->setFlags(item->flags() | Qt::ItemIsEditable);
    m_layerList->addItem(item);

    updateLayout();
    if (m_drawingArea) {
        m_drawingArea->update();
    }
}

void Timeline::removeLayer(int index)
{
    if (index >= 0 && index < m_layers.size()) {
        auto it = std::remove_if(m_keyframes.begin(), m_keyframes.end(),
            [index](const Keyframe& kf) {
                return kf.layer == index;
            });
        m_keyframes.erase(it, m_keyframes.end());

        for (auto& kf : m_keyframes) {
            if (kf.layer > index) {
                kf.layer--;
            }
        }

        m_layers.erase(m_layers.begin() + index);
        delete m_layerList->takeItem(index);

        if (m_selectedLayer >= m_layers.size()) {
            m_selectedLayer = m_layers.size() - 1;
        }

        updateLayout();
        if (m_drawingArea) {
            m_drawingArea->update();
        }
    }
}

int Timeline::getLayerCount() const
{
    return m_layers.size();
}

void Timeline::setZoomLevel(double zoom)
{
    m_zoomLevel = zoom;
    updateLayout();
    if (m_drawingArea) {
        m_drawingArea->update();
    }
}

double Timeline::getZoomLevel() const
{
    return m_zoomLevel;
}

void Timeline::updateLayout()
{
    if (!m_drawingArea) return;

    int frameWidth = static_cast<int>(m_frameWidth * m_zoomLevel);
    int totalWidth = m_totalFrames * frameWidth + 100;
    int totalHeight = m_rulerHeight + m_layers.size() * m_layerHeight + 50;

    m_drawingArea->setMinimumSize(totalWidth, totalHeight);
}

void Timeline::onFrameSliderChanged(int value)
{
    setCurrentFrame(value);
}

void Timeline::onFrameSpinBoxChanged(int value)
{
    setCurrentFrame(value);
}

void Timeline::onFrameRateChanged(int index)
{
    QString fpsText = m_frameRateCombo->itemText(index);
    int fps = fpsText.toInt();
    setFrameRate(fps);
}

void Timeline::onLayerSelectionChanged()
{
    m_selectedLayer = m_layerList->currentRow();
    emit layerSelected(m_selectedLayer);
    if (m_drawingArea) {
        m_drawingArea->update();
    }
}

// Add the remaining methods as needed...
void Timeline::selectKeyframe(int layer, int frame) {}
void Timeline::clearKeyframeSelection() {}
void Timeline::setLayerName(int index, const QString& name) {}
void Timeline::setLayerVisible(int index, bool visible) {}
void Timeline::setLayerLocked(int index, bool locked) {}
void Timeline::scrollToFrame(int frame) {}
