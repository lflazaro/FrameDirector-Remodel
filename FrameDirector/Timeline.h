// Timeline.h
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
#include <vector>
#include <map>

class MainWindow;
class TimelineHeader;
class TimelineTrack;
class TimelineRuler;
class AnimationKeyframe;

class Timeline : public QWidget
{
    Q_OBJECT

public:
    explicit Timeline(MainWindow* parent = nullptr);
    ~Timeline();

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

    // Keyframes
    void addKeyframe(int layer, int frame);
    void removeKeyframe(int layer, int frame);
    bool hasKeyframe(int layer, int frame) const;
    void selectKeyframe(int layer, int frame);
    void clearKeyframeSelection();

    // Layers
    void addLayer(const QString& name);
    void removeLayer(int index);
    void setLayerName(int index, const QString& name);
    void setLayerVisible(int index, bool visible);
    void setLayerLocked(int index, bool locked);
    int getLayerCount() const;

    // View
    void setZoomLevel(double zoom);
    double getZoomLevel() const;
    void scrollToFrame(int frame);

signals:
    void frameChanged(int frame);
    void frameRateChanged(int fps);
    void keyframeAdded(int layer, int frame);
    void keyframeRemoved(int layer, int frame);
    void keyframeSelected(int layer, int frame);
    void layerSelected(int layer);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private slots:
    void onFrameSliderChanged(int value);
    void onFrameSpinBoxChanged(int value);
    void onFrameRateChanged(int fps);
    void onLayerSelectionChanged();

private:
    void setupUI();
    void setupControls();
    void updateLayout();
    void drawTimelineBackground(QPainter* painter);
    void drawFrameRuler(QPainter* painter);
    void drawLayers(QPainter* painter);
    void drawKeyframes(QPainter* painter);
    void drawPlayhead(QPainter* painter);
    void drawSelection(QPainter* painter);

    QRect getFrameRect(int frame) const;
    QRect getLayerRect(int layer) const;
    int getFrameFromX(int x) const;
    int getLayerFromY(int y) const;
    void updateScrollbars();

    MainWindow* m_mainWindow;

    // UI Components
    QVBoxLayout* m_mainLayout;
    QHBoxLayout* m_controlsLayout;
    QScrollArea* m_scrollArea;
    QWidget* m_timelineWidget;

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

    // Colors and styling
    QColor m_backgroundColor;
    QColor m_frameColor;
    QColor m_keyframeColor;
    QColor m_selectedKeyframeColor;
    QColor m_playheadColor;
    QColor m_rulerColor;
    QColor m_layerColor;
    QColor m_alternateLayerColor;

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
    };

    std::vector<Layer> m_layers;
    std::vector<Keyframe> m_keyframes;

    // Selection and interaction
    bool m_dragging;
    QPoint m_dragStart;
    int m_selectedLayer;
    std::vector<int> m_selectedKeyframes;
};
#endif