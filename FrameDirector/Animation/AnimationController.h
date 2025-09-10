#ifndef ANIMATIONCONTROLLER_H
#define ANIMATIONCONTROLLER_H

#include <QObject>
#include <QTimer>
#include <QPropertyAnimation>
#include <QEasingCurve>
#include <vector>
#include <memory>

class MainWindow;
class AnimationLayer;
class Timeline;

class AnimationController : public QObject
{
    Q_OBJECT

public:
    explicit AnimationController(MainWindow* parent = nullptr);
    ~AnimationController();

    // Playback control
    void play();
    void pause();
    void stop();
    void setFrameRate(int fps);
    void setCurrentFrame(int frame);
    void nextFrame();
    void previousFrame();
    void firstFrame();
    void lastFrame();

    // Animation properties
    int getCurrentFrame() const;
    int getTotalFrames() const;
    void setTotalFrames(int frames);
    int getFrameRate() const;
    bool isPlaying() const;

    // Layers
    void addLayer(std::unique_ptr<AnimationLayer> layer);
    void removeLayer(int index);
    AnimationLayer* getLayer(int index) const;
    int getLayerCount() const;
    void setCurrentLayer(int index);
    int getCurrentLayer() const;

    // Keyframes
    void addKeyframe();
    void addKeyframe(int layer, int frame);
    void removeKeyframe(int layer, int frame);
    void copyKeyframe(int fromLayer, int fromFrame, int toLayer, int toFrame);
    void moveKeyframe(int fromFrame, int toFrame);

    // Export
    void exportAnimation(const QString& filename, const QString& format, int quality = 80, bool loop = true);
    void exportFrame(int frame, const QString& filename);

signals:
    void frameChanged(int frame);
    void playbackStateChanged(bool playing);
    void totalFramesChanged(int frames);
    void frameRateChanged(int fps);
    void layerAdded(int index);
    void layerRemoved(int index);
    void keyframeAdded(int layer, int frame);
    void keyframeRemoved(int layer, int frame);
    // Emitted during export to allow external progress UI
    void exportProgress(int value, int maximum);

private slots:
    void onPlaybackTimer();

private:
    void updateAllLayers();
    void updateLayerAtFrame(AnimationLayer* layer, int frame);
    bool exportToGif(const QStringList& frameFiles, const QString& filename, bool loop);
    bool exportToMp4(const QStringList& frameFiles, const QString& filename, int quality);
    MainWindow* m_mainWindow;
    Timeline* m_timeline;

    QTimer* m_playbackTimer;
    int m_currentFrame;
    int m_totalFrames;
    int m_frameRate;
    bool m_isPlaying;
    int m_currentLayer;

    std::vector<std::unique_ptr<AnimationLayer>> m_layers;
};
#endif