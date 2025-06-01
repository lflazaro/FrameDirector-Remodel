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
    , m_layerHeight(20)
    , m_rulerHeight(30)
    , m_layerPanelWidth(120)
    , m_dragging(false)
    , m_selectedLayer(-1)
{
    // Initialize colors
    m_backgroundColor = QColor(45, 45, 48);
    m_frameColor = QColor(60, 60, 63);
    m_keyframeColor = QColor(255, 140, 0);
    m_selectedKeyframeColor = QColor(255, 200, 100);
    m_playheadColor = QColor(255, 0, 0);
    m_rulerColor = QColor(80, 80, 85);
    m_layerColor = QColor(62, 62, 66);
    m_alternateLayerColor = QColor(70, 70, 75);

    setupUI();
    setMinimumHeight(150);
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
    layersLabel->setStyleSheet("background-color: #3E3E42; color: white; padding: 4px; font-weight: bold;");
    layersLabel->setAlignment(Qt::AlignCenter);
    layerPanelLayout->addWidget(layersLabel);

    m_layerList = new QListWidget;
    m_layerList->setMaximumWidth(m_layerPanelWidth);
    m_layerList->setMinimumWidth(m_layerPanelWidth);
    m_layerList->setStyleSheet(
        "QListWidget {"
        "    background-color: #2D2D30;"
        "    color: white;"
        "    border: 1px solid #5A5A5C;"
        "    selection-background-color: #007ACC;"
        "}"
        "QListWidget::item {"
        "    padding: 4px;"
        "    border-bottom: 1px solid #3E3E42;"
        "}"
        "QListWidget::item:selected {"
        "    background-color: #007ACC;"
        "}"
    );
    layerPanelLayout->addWidget(m_layerList);

    // Layer buttons
    QHBoxLayout* layerButtonsLayout = new QHBoxLayout;
    m_addLayerButton = new QPushButton("+");
    m_removeLayerButton = new QPushButton("-");
    m_addLayerButton->setMaximumSize(30, 20);
    m_removeLayerButton->setMaximumSize(30, 20);

    layerButtonsLayout->addWidget(m_addLayerButton);
    layerButtonsLayout->addWidget(m_removeLayerButton);
    layerButtonsLayout->addStretch();
    layerPanelLayout->addLayout(layerButtonsLayout);

    QWidget* layerPanel = new QWidget;
    layerPanel->setLayout(layerPanelLayout);
    layerPanel->setMaximumWidth(m_layerPanelWidth);
    layerPanel->setStyleSheet("background-color: #3E3E42;");

    timelineLayout->addWidget(layerPanel);

    // Timeline widget (custom painted)
    m_timelineWidget = new QWidget;
    m_timelineWidget->setMinimumSize(800, 100);
    m_timelineWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    // Scroll area for timeline
    m_scrollArea = new QScrollArea;
    m_scrollArea->setWidget(m_timelineWidget);
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_scrollArea->setStyleSheet(
        "QScrollArea {"
        "    background-color: #2D2D30;"
        "    border: 1px solid #5A5A5C;"
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
}

void Timeline::setupControls()
{
    m_controlsLayout = new QHBoxLayout;
    m_controlsLayout->setContentsMargins(4, 4, 4, 4);

    // Playback controls
    m_firstFrameButton = new QPushButton("⏮");
    m_prevFrameButton = new QPushButton("⏪");
    m_playButton = new QPushButton("▶");
    m_stopButton = new QPushButton("⏹");
    m_nextFrameButton = new QPushButton("⏩");
    m_lastFrameButton = new QPushButton("⏭");

    // Style buttons
    QString buttonStyle =
        "QPushButton {"
        "    background-color: #3E3E42;"
        "    color: white;"
        "    border: 1px solid #5A5A5C;"
        "    padding: 4px 8px;"
        "    font-size: 12px;"
        "}"
        "QPushButton:hover {"
        "    background-color: #4A4A4F;"
        "}"
        "QPushButton:pressed {"
        "    background-color: #007ACC;"
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

    m_controlsLayout->addSpacing(10);

    // Frame controls
    m_frameLabel = new QLabel("Frame:");
    m_frameLabel->setStyleSheet("color: white;");
    m_frameSpinBox = new QSpinBox;
    m_frameSpinBox->setRange(1, m_totalFrames);
    m_frameSpinBox->setValue(m_currentFrame);
    m_frameSpinBox->setStyleSheet(
        "QSpinBox {"
        "    background-color: #2D2D30;"
        "    color: white;"
        "    border: 1px solid #5A5A5C;"
        "    padding: 2px;"
        "}"
    );

    m_totalFramesLabel = new QLabel(QString("/ %1").arg(m_totalFrames));
    m_totalFramesLabel->setStyleSheet("color: #CCCCCC;");

    m_controlsLayout->addWidget(m_frameLabel);
    m_controlsLayout->addWidget(m_frameSpinBox);
    m_controlsLayout->addWidget(m_totalFramesLabel);

    m_controlsLayout->addSpacing(10);

    // Frame rate
    QLabel* fpsLabel = new QLabel("FPS:");
    fpsLabel->setStyleSheet("color: white;");
    m_frameRateCombo = new QComboBox;
    m_frameRateCombo->addItems({ "12", "15", "24", "30", "60" });
    m_frameRateCombo->setCurrentText("24");
    m_frameRateCombo->setStyleSheet(
        "QComboBox {"
        "    background-color: #3E3E42;"
        "    color: white;"
        "    border: 1px solid #5A5A5C;"
        "    padding: 2px;"
        "}"
        "QComboBox::drop-down {"
        "    border: none;"
        "}"
        "QComboBox QAbstractItemView {"
        "    background-color: #3E3E42;"
        "    color: white;"
        "    border: 1px solid #5A5A5C;"
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
        "    border: 1px solid #5A5A5C;"
        "    height: 4px;"
        "    background: #2D2D30;"
        "}"
        "QSlider::handle:horizontal {"
        "    background: #007ACC;"
        "    border: 1px solid #005A9B;"
        "    width: 8px;"
        "    margin: -4px 0;"
        "    border-radius: 2px;"
        "}"
    );

    m_controlsLayout->addWidget(m_frameSlider);

    // Create controls widget
    QWidget* controlsWidget = new QWidget;
    controlsWidget->setLayout(m_controlsLayout);
    controlsWidget->setStyleSheet("background-color: #3E3E42; border-bottom: 1px solid #5A5A5C;");
    controlsWidget->setMaximumHeight(40);

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

void Timeline::setCurrentFrame(int frame)
{
    if (frame != m_currentFrame && frame >= 1 && frame <= m_totalFrames) {
        m_currentFrame = frame;
        m_frameSpinBox->setValue(frame);
        m_frameSlider->setValue(frame);
        update();
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
        update();
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
        // Check if keyframe already exists
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
        update();
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
        update();
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

    update();
}

void Timeline::removeLayer(int index)
{
    if (index >= 0 && index < m_layers.size()) {
        // Remove all keyframes for this layer
        auto it = std::remove_if(m_keyframes.begin(), m_keyframes.end(),
            [index](const Keyframe& kf) {
                return kf.layer == index;
            });
        m_keyframes.erase(it, m_keyframes.end());

        // Adjust layer indices for remaining keyframes
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

        update();
    }
}

int Timeline::getLayerCount() const
{
    return m_layers.size();
}

void Timeline::paintEvent(QPaintEvent* event)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, false);

    drawTimelineBackground(&painter);
    drawFrameRuler(&painter);
    drawLayers(&painter);
    drawKeyframes(&painter);
    drawPlayhead(&painter);
    drawSelection(&painter);
}

void Timeline::drawTimelineBackground(QPainter* painter)
{
    painter->fillRect(rect(), m_backgroundColor);
}

void Timeline::drawFrameRuler(QPainter* painter)
{
    if (!m_timelineWidget) return;

    QRect timelineRect = m_scrollArea->widget()->geometry();
    QRect rulerRect(m_layerPanelWidth, 0, timelineRect.width() - m_layerPanelWidth, m_rulerHeight);

    painter->fillRect(rulerRect, m_rulerColor);

    painter->setPen(QPen(Qt::white, 1));
    painter->setFont(QFont("Arial", 8));

    int frameWidth = static_cast<int>(m_frameWidth * m_zoomLevel);
    int startFrame = qMax(1, m_scrollX / frameWidth);
    int endFrame = qMin(m_totalFrames, startFrame + rulerRect.width() / frameWidth + 1);

    for (int frame = startFrame; frame <= endFrame; ++frame) {
        int x = m_layerPanelWidth + (frame - 1) * frameWidth - m_scrollX;

        if (frame % 5 == 1) {
            painter->drawLine(x, rulerRect.bottom() - 10, x, rulerRect.bottom());
            painter->drawText(x + 2, rulerRect.bottom() - 12, QString::number(frame));
        }
        else {
            painter->drawLine(x, rulerRect.bottom() - 5, x, rulerRect.bottom());
        }
    }
}

void Timeline::drawLayers(QPainter* painter)
{
    if (!m_timelineWidget) return;

    QRect timelineRect = m_scrollArea->widget()->geometry();

    for (int i = 0; i < m_layers.size(); ++i) {
        QRect layerRect = getLayerRect(i);
        layerRect.setLeft(m_layerPanelWidth);
        layerRect.setRight(timelineRect.width());

        painter->fillRect(layerRect, m_layers[i].color);

        // Draw layer separator
        painter->setPen(QPen(QColor(90, 90, 95), 1));
        painter->drawLine(layerRect.bottomLeft(), layerRect.bottomRight());

        // Draw frame separators
        painter->setPen(QPen(QColor(50, 50, 55), 1));
        int frameWidth = static_cast<int>(m_frameWidth * m_zoomLevel);
        int startFrame = qMax(1, m_scrollX / frameWidth);
        int endFrame = qMin(m_totalFrames, startFrame + layerRect.width() / frameWidth + 1);

        for (int frame = startFrame; frame <= endFrame; ++frame) {
            int x = m_layerPanelWidth + (frame - 1) * frameWidth - m_scrollX;
            painter->drawLine(x, layerRect.top(), x, layerRect.bottom());
        }
    }
}

void Timeline::drawKeyframes(QPainter* painter)
{
    int frameWidth = static_cast<int>(m_frameWidth * m_zoomLevel);

    for (const auto& keyframe : m_keyframes) {
        QRect frameRect = getFrameRect(keyframe.frame);
        QRect layerRect = getLayerRect(keyframe.layer);

        if (frameRect.isEmpty() || layerRect.isEmpty()) continue;

        QRect keyframeRect(frameRect.center().x() - 4, layerRect.center().y() - 4, 8, 8);

        QColor color = keyframe.selected ? m_selectedKeyframeColor : keyframe.color;
        painter->setBrush(QBrush(color));
        painter->setPen(QPen(color.darker(150), 1));
        painter->drawEllipse(keyframeRect);
    }
}

void Timeline::drawPlayhead(QPainter* painter)
{
    QRect frameRect = getFrameRect(m_currentFrame);
    if (frameRect.isEmpty()) return;

    int x = frameRect.left();
    QRect timelineRect = m_scrollArea->widget()->geometry();

    painter->setPen(QPen(m_playheadColor, 2));
    painter->drawLine(x, m_rulerHeight, x, timelineRect.height());

    // Draw playhead triangle
    QPolygon triangle;
    triangle << QPoint(x - 6, m_rulerHeight - 2)
        << QPoint(x + 6, m_rulerHeight - 2)
        << QPoint(x, m_rulerHeight + 8);

    painter->setBrush(QBrush(m_playheadColor));
    painter->drawPolygon(triangle);
}

void Timeline::drawSelection(QPainter* painter)
{
    // Draw selection highlight for selected layer
    if (m_selectedLayer >= 0 && m_selectedLayer < m_layers.size()) {
        QRect layerRect = getLayerRect(m_selectedLayer);
        layerRect.setLeft(0);
        layerRect.setRight(m_layerPanelWidth);

        painter->fillRect(layerRect, QColor(0, 122, 204, 50));
        painter->setPen(QPen(QColor(0, 122, 204), 2));
        painter->drawRect(layerRect);
    }
}

QRect Timeline::getFrameRect(int frame) const
{
    if (!m_timelineWidget) return QRect();

    int frameWidth = static_cast<int>(m_frameWidth * m_zoomLevel);
    int x = m_layerPanelWidth + (frame - 1) * frameWidth - m_scrollX;
    int y = m_rulerHeight;
    QRect timelineRect = m_scrollArea->widget()->geometry();
    int height = timelineRect.height() - m_rulerHeight;

    return QRect(x, y, frameWidth, height);
}

QRect Timeline::getLayerRect(int layer) const
{
    if (layer < 0 || layer >= m_layers.size()) return QRect();

    int y = m_rulerHeight + layer * m_layerHeight - m_scrollY;
    return QRect(0, y, width(), m_layerHeight);
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

void Timeline::mousePressEvent(QMouseEvent* event)
{
    int mouseX = event->position().toPoint().x();
    int mouseY = event->position().toPoint().y();
    if (event->button() == Qt::LeftButton) {
        int frame = getFrameFromX(mouseX);
        int layer = getLayerFromY(mouseY);

        if (mouseX > m_layerPanelWidth && mouseY > m_rulerHeight) {
            // Timeline area click
            if (event->modifiers() & Qt::ControlModifier) {
                // Add/remove keyframe
                if (hasKeyframe(layer, frame)) {
                    removeKeyframe(layer, frame);
                }
                else {
                    addKeyframe(layer, frame);
                }
            }
            else {
                // Set current frame
                setCurrentFrame(frame);
                m_selectedLayer = layer;
                emit layerSelected(layer);
                update();
            }
        }
    }

    QWidget::mousePressEvent(event);
}

void Timeline::mouseMoveEvent(QMouseEvent* event)
{
    int mouseX = event->position().toPoint().x();
    if (event->buttons() & Qt::LeftButton && mouseX > m_layerPanelWidth) {
        int frame = getFrameFromX(mouseX);
        setCurrentFrame(frame);
    }

    QWidget::mouseMoveEvent(event);
}

void Timeline::mouseReleaseEvent(QMouseEvent* event)
{
    QWidget::mouseReleaseEvent(event);
}

void Timeline::wheelEvent(QWheelEvent* event)
{
    if (event->modifiers() & Qt::ControlModifier) {
        // Zoom
        double delta = event->angleDelta().y() / 120.0;
        double newZoom = m_zoomLevel * (1.0 + delta * 0.1);
        setZoomLevel(qBound(0.5, newZoom, 3.0));
    }
    else {
        // Scroll
        QWidget::wheelEvent(event);
    }
}

void Timeline::setZoomLevel(double zoom)
{
    m_zoomLevel = zoom;
    updateLayout();
    update();
}

double Timeline::getZoomLevel() const
{
    return m_zoomLevel;
}

void Timeline::updateLayout()
{
    if (!m_timelineWidget) return;

    int frameWidth = static_cast<int>(m_frameWidth * m_zoomLevel);
    int totalWidth = m_totalFrames * frameWidth + m_layerPanelWidth + 100;
    int totalHeight = m_rulerHeight + m_layers.size() * m_layerHeight + 50;

    m_timelineWidget->setMinimumSize(totalWidth, totalHeight);
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
    update();
}

void Timeline::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    updateLayout();

    if (m_timelineWidget) {
        int frameWidth = static_cast<int>(m_frameWidth * m_zoomLevel);
        int totalWidth = m_totalFrames * frameWidth + m_layerPanelWidth + 100;
        int totalHeight = m_rulerHeight + m_layers.size() * m_layerHeight + 50;

        m_timelineWidget->setMinimumSize(totalWidth, totalHeight);
    }

    if (m_scrollArea) {
        m_scrollArea->updateGeometry();
    }

    update();
}