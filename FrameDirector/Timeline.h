// Timeline.h - Enhanced with frame extension visualization
#ifndef TIMELINE_H
#define TIMELINE_H

#include <QWidget>
#include <QScrollArea>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSlider>
#include <QSpinBox>
#include <QComboBox>
#include <QListWidget>
#include <QPainter>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QTimer>
#include <QColor>
#include <QBrush>
#include <QPen>
#include <QFont>
#include <QMenu>
#include <QAction>
#include <vector>
#include <map>

class MainWindow;
class TimelineHeader;
class TimelineTrack;
class TimelineRuler;
class AnimationKeyframe;
class LayerGraphicsGroup;

// FIXED: Include Canvas.h to get the actual enum definitions
#include "Canvas.h"

// Frame visualization types
enum class FrameVisualType {
    Empty,
    Keyframe,
    ExtendedFrame,
    EndFrame  // Last frame of an extension
};

// Custom widget for timeline drawing area
class TimelineDrawingArea : public QWidget
{
    Q_OBJECT

public:
    explicit TimelineDrawingArea(QWidget* parent = nullptr);
    void setTimeline(class Timeline* timeline) { m_timeline = timeline; }

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;

private:
    class Timeline* m_timeline;
};

class Timeline : public QWidget
{
    Q_OBJECT

public:
    explicit Timeline(MainWindow* parent = nullptr);
    ~Timeline();
    TimelineDrawingArea* m_drawingArea;

    // Frame management
    void setCurrentFrame(int frame);
    int getCurrentFrame() const;
    void setTotalFrames(int frames);
    int getTotalFrames() const;

    // Playback
    void setFrameRate(int fps);
    int getFrameRate() const;
    void setPlaying(bool playing);
    bool isPlaying() const;

    // ENHANCED: Keyframe and frame extension management
    void addKeyframe(int layer, int frame);
    void addExtendedFrame(int layer, int frame);
    void addBlankKeyframe(int layer, int frame);
    void removeKeyframe(int layer, int frame);
    bool hasKeyframe(int layer, int frame) const;
    bool hasContent(int layer, int frame) const;
    FrameVisualType getFrameVisualType(int layer, int frame) const;

    void selectKeyframe(int layer, int frame);
    void clearKeyframeSelection();
    void toggleKeyframe(int layer, int frame);

    // Layers
    void addLayer(const QString& name);
    void removeLayer(int index);
    void setLayerName(int index, const QString& name);
    void setLayerVisible(int index, bool visible);
    void setLayerLocked(int index, bool locked);
    int getLayerCount() const;
    void updateLayersFromCanvas();

    // View
    void setZoomLevel(double zoom);
    double getZoomLevel() const;
    void scrollToFrame(int frame);

    // ENHANCED: Drawing methods with frame extension visualization
    void drawTimelineBackground(QPainter* painter, const QRect& rect);
    void drawFrameRuler(QPainter* painter, const QRect& rect);
    void drawLayers(QPainter* painter, const QRect& rect);
    void drawKeyframes(QPainter* painter, const QRect& rect);
    void drawFrameExtensions(QPainter* painter, const QRect& rect);  // NEW
    void drawPlayhead(QPainter* painter, const QRect& rect);
    void drawSelection(QPainter* painter, const QRect& rect);

    // FIXED: Properly declare tweening methods with correct TweenType
    void drawTweening(QPainter* painter, const QRect& rect);
    void drawTweenSpan(QPainter* painter, int layer, int startFrame, int endFrame, TweenType type);
    void drawTweenArrow(QPainter* painter, int x, int y, const QColor& color);
    void drawTweenTypeIndicator(QPainter* painter, int x, int y, TweenType type);

    // Helper methods for drawing area
    QRect getFrameRect(int frame) const;
    QRect getLayerRect(int layer) const;
    int getFrameFromX(int x) const;
    int getLayerFromY(int y) const;
    QRect getDrawingAreaRect() const;
    void showContextMenu(const QPoint& position, int layer, int frame);
    bool canApplyTweening(int layer, int frame) const;
    bool hasTweening(int layer, int frame) const;
    MainWindow* m_mainWindow;

signals:
    void frameChanged(int frame);
    void frameRateChanged(int fps);
    void keyframeAdded(int layer, int frame);
    void keyframeRemoved(int layer, int frame);
    void frameExtended(int layer, int frame);  // NEW
    void keyframeSelected(int layer, int frame);
    void layerSelected(int layer);

    // FIXED: Use proper TweenType in signal declarations
    void tweeningRequested(int layer, int startFrame, int endFrame, TweenType type);
    void tweeningRemovalRequested(int layer, int startFrame, int endFrame);

private slots:
    void onFrameSliderChanged(int value);
    void onFrameSpinBoxChanged(int value);
    void onFrameRateChanged(int index);
    void onLayerSelectionChanged();
    void onKeyframeCreated(int frame);
    void onFrameExtended(int fromFrame, int toFrame);  // NEW
    void onCreateMotionTween();
    void onCreateClassicTween();
    void onRemoveTween();

    // FIXED: Use proper TweenType in slot declaration
    void onTweeningApplied(int layer, int startFrame, int endFrame, TweenType type);

private:
    void setupUI();
    void setupControls();
    void updateLayout();
    void setupContextMenu();
    void updateContextMenuActions();
    QList<int> findTweenableSpan(int layer, int frame) const;

    // ENHANCED: Frame extension visualization helpers
    void drawFrameSpan(QPainter* painter, int layer, int startFrame, int endFrame);
    void drawKeyframeSymbol(QPainter* painter, int x, int y, FrameVisualType type, bool selected, bool hasTweening);
    QColor getFrameExtensionColor(int layer) const;

    // UI Components
    QVBoxLayout* m_mainLayout;
    QHBoxLayout* m_controlsLayout;
    QScrollArea* m_scrollArea;

    // Controls
    QPushButton* m_playButton;
    QPushButton* m_stopButton;
    QPushButton* m_firstFrameButton;
    QPushButton* m_prevFrameButton;
    QPushButton* m_nextFrameButton;
    QPushButton* m_lastFrameButton;
    QSlider* m_frameSlider;
    QSpinBox* m_frameSpinBox;
    QComboBox* m_frameRateCombo;
    QLabel* m_frameLabel;
    QLabel* m_totalFramesLabel;

    // Layer panel
    QListWidget* m_layerList;
    QPushButton* m_addLayerButton;
    QPushButton* m_removeLayerButton;

    // Timeline properties
    int m_currentFrame;
    int m_totalFrames;
    int m_frameRate;
    bool m_isPlaying;
    double m_zoomLevel;
    int m_scrollX;
    int m_scrollY;

    // Layout properties
    int m_frameWidth;
    int m_layerHeight;
    int m_rulerHeight;
    int m_layerPanelWidth;

    // ENHANCED: Colors for frame extension visualization
    QColor m_backgroundColor;
    QColor m_frameColor;
    QColor m_keyframeColor;
    QColor m_selectedKeyframeColor;
    QColor m_playheadColor;
    QColor m_rulerColor;
    QColor m_layerColor;
    QColor m_alternateLayerColor;
    QColor m_frameExtensionColor;     // NEW: Orange color for extensions
    QColor m_extendedFrameColor;      // NEW: Lighter color for extended frames

    // Data structures
    struct Layer {
        QString name;
        bool visible;
        bool locked;
        QColor color;
    };

    struct Keyframe {
        int layer;
        int frame;
        bool selected;
        QColor color;
        FrameVisualType visualType;  // NEW: Track visual type
    };

    std::vector<Layer> m_layers;
    std::vector<Keyframe> m_keyframes;

    // Selection and interaction
    bool m_dragging;
    QPoint m_dragStart;
    int m_selectedLayer;
    std::vector<int> m_selectedKeyframes;

    // NEW: Context menu components
    QMenu* m_contextMenu;
    QAction* m_createMotionTweenAction;
    QAction* m_createClassicTweenAction;
    QAction* m_removeTweenAction;
    QAction* m_insertKeyframeAction;
    QAction* m_insertFrameAction;
    QAction* m_clearFrameAction;

    // Context menu state
    int m_contextMenuLayer;
    int m_contextMenuFrame;
};

#endif