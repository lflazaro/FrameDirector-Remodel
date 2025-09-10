// Animation/AnimationController.cpp
#include "AnimationController.h"
#include "AnimationLayer.h"
#include "AnimationKeyframe.h"
#include "../MainWindow.h"
#include "../Timeline.h"
#include "../Canvas.h"
#include <QTimer>
#include <QPropertyAnimation>
#include <QEasingCurve>
#include <QFileDialog>
#include <QMessageBox>
#include <QApplication>
#include <QRegularExpression>
#include <QFileInfo>
#include <QGraphicsScene>
#include <QGraphicsItem>
#include <QPainter>
#include <QSvgGenerator>
#include <QImageWriter>
#include <QDir>
#include <QProcess>
#include <QStandardPaths>

AnimationController::AnimationController(MainWindow* parent)
    : QObject(parent)
    , m_mainWindow(parent)
    , m_timeline(nullptr)
    , m_currentFrame(1)
    , m_totalFrames(100)
    , m_frameRate(24)
    , m_isPlaying(false)
    , m_currentLayer(0)
{
    // Setup playback timer
    m_playbackTimer = new QTimer(this);
    m_playbackTimer->setSingleShot(false);
    connect(m_playbackTimer, &QTimer::timeout, this, &AnimationController::onPlaybackTimer);

    // Set initial timer interval based on frame rate
    setFrameRate(m_frameRate);

    // Find timeline component
    m_timeline = m_mainWindow->findChild<Timeline*>();

    // Connect timeline signals if available
    if (m_timeline) {
        connect(this, &AnimationController::frameChanged, m_timeline, &Timeline::setCurrentFrame);
        connect(this, &AnimationController::totalFramesChanged, m_timeline, &Timeline::setTotalFrames);
        connect(this, &AnimationController::frameRateChanged, m_timeline, &Timeline::setFrameRate);
    }
}

AnimationController::~AnimationController()
{
    stop();
}

void AnimationController::play()
{
    if (!m_isPlaying) {
        m_isPlaying = true;
        m_playbackTimer->start();
        emit playbackStateChanged(true);
    }
}

void AnimationController::pause()
{
    if (m_isPlaying) {
        m_isPlaying = false;
        m_playbackTimer->stop();
        emit playbackStateChanged(false);
    }
}

void AnimationController::stop()
{
    if (m_isPlaying) {
        m_isPlaying = false;
        m_playbackTimer->stop();
        emit playbackStateChanged(false);
    }

    // Reset to first frame
    setCurrentFrame(1);
}

void AnimationController::setFrameRate(int fps)
{
    if (fps > 0 && fps != m_frameRate) {
        m_frameRate = fps;

        // Update timer interval
        int interval = 1000 / fps;
        m_playbackTimer->setInterval(interval);

        emit frameRateChanged(fps);
    }
}

void AnimationController::setCurrentFrame(int frame)
{
    if (frame != m_currentFrame && frame >= 1 && frame <= m_totalFrames) {
        m_currentFrame = frame;

        // Update all layers to show the current frame
        updateAllLayers();

        emit frameChanged(frame);
    }
}

void AnimationController::nextFrame()
{
    if (m_currentFrame < m_totalFrames) {
        setCurrentFrame(m_currentFrame + 1);
    }
    else {
        // Loop back to first frame
        setCurrentFrame(1);
    }
}

void AnimationController::previousFrame()
{
    if (m_currentFrame > 1) {
        setCurrentFrame(m_currentFrame - 1);
    }
    else {
        // Loop to last frame
        setCurrentFrame(m_totalFrames);
    }
}

void AnimationController::firstFrame()
{
    setCurrentFrame(1);
}

void AnimationController::lastFrame()
{
    setCurrentFrame(m_totalFrames);
}

int AnimationController::getCurrentFrame() const
{
    return m_currentFrame;
}

int AnimationController::getTotalFrames() const
{
    return m_totalFrames;
}

void AnimationController::setTotalFrames(int frames)
{
    if (frames > 0 && frames != m_totalFrames) {
        m_totalFrames = frames;

        // Ensure current frame is within bounds
        if (m_currentFrame > frames) {
            setCurrentFrame(frames);
        }

        emit totalFramesChanged(frames);
    }
}

int AnimationController::getFrameRate() const
{
    return m_frameRate;
}

bool AnimationController::isPlaying() const
{
    return m_isPlaying;
}

void AnimationController::addLayer(std::unique_ptr<AnimationLayer> layer)
{
    if (layer) {
        int index = m_layers.size();
        m_layers.push_back(std::move(layer));
        emit layerAdded(index);

        // Update the newly added layer to current frame
        updateLayerAtFrame(m_layers.back().get(), m_currentFrame);
    }
}

void AnimationController::removeLayer(int index)
{
    if (index >= 0 && index < static_cast<int>(m_layers.size())) {
        m_layers.erase(m_layers.begin() + index);

        // Adjust current layer if necessary
        if (m_currentLayer >= static_cast<int>(m_layers.size())) {
            m_currentLayer = static_cast<int>(m_layers.size()) - 1;
        }
        if (m_currentLayer < 0 && !m_layers.empty()) {
            m_currentLayer = 0;
        }

        emit layerRemoved(index);
    }
}

AnimationLayer* AnimationController::getLayer(int index) const
{
    if (index >= 0 && index < static_cast<int>(m_layers.size())) {
        return m_layers[index].get();
    }
    return nullptr;
}

int AnimationController::getLayerCount() const
{
    return static_cast<int>(m_layers.size());
}

void AnimationController::setCurrentLayer(int index)
{
    if (index >= 0 && index < static_cast<int>(m_layers.size())) {
        m_currentLayer = index;
    }
}

int AnimationController::getCurrentLayer() const
{
    return m_currentLayer;
}

void AnimationController::addKeyframe()
{
    addKeyframe(m_currentLayer, m_currentFrame);
}

void AnimationController::addKeyframe(int layer, int frame)
{
    AnimationLayer* animLayer = getLayer(layer);
    if (animLayer && frame >= 1 && frame <= m_totalFrames) {
        // Create keyframe with current state of items
        auto keyframe = std::make_unique<AnimationKeyframe>(frame);

        // Capture current state of all items in the layer
        QList<QGraphicsItem*> items = animLayer->getItems();
        for (QGraphicsItem* item : items) {
            keyframe->captureItemState(item);
        }

        animLayer->addKeyframe(frame, std::move(keyframe));
        emit keyframeAdded(layer, frame);
    }
}

void AnimationController::removeKeyframe(int layer, int frame)
{
    AnimationLayer* animLayer = getLayer(layer);
    if (animLayer) {
        animLayer->removeKeyframe(frame);
        emit keyframeRemoved(layer, frame);
    }
}

void AnimationController::copyKeyframe(int fromLayer, int fromFrame, int toLayer, int toFrame)
{
    AnimationLayer* sourceLayer = getLayer(fromLayer);
    AnimationLayer* targetLayer = getLayer(toLayer);

    if (sourceLayer && targetLayer) {
        AnimationKeyframe* sourceKeyframe = sourceLayer->getKeyframe(fromFrame);
        if (sourceKeyframe) {
            // Create a copy of the keyframe
            auto newKeyframe = std::make_unique<AnimationKeyframe>(toFrame);
            newKeyframe->setEasing(sourceKeyframe->getEasing());
            newKeyframe->setType(sourceKeyframe->getType());

            // Copy item states from source to target
            QList<QGraphicsItem*> sourceItems = sourceLayer->getItems();
            QList<QGraphicsItem*> targetItems = targetLayer->getItems();

            // For simplicity, copy states for items at the same index
            int minItems = qMin(sourceItems.size(), targetItems.size());
            for (int i = 0; i < minItems; ++i) {
                if (sourceKeyframe->hasItemState(sourceItems[i])) {
                    // Apply source state to target item and capture it
                    sourceKeyframe->applyItemState(targetItems[i]);
                    newKeyframe->captureItemState(targetItems[i]);
                }
            }

            targetLayer->addKeyframe(toFrame, std::move(newKeyframe));
            emit keyframeAdded(toLayer, toFrame);
        }
    }
}

void AnimationController::moveKeyframe(int fromFrame, int toFrame)
{
    if (fromFrame != toFrame && m_currentLayer >= 0) {
        AnimationLayer* layer = getLayer(m_currentLayer);
        if (layer) {
            AnimationKeyframe* keyframe = layer->getKeyframe(fromFrame);
            if (keyframe) {
                // Create new keyframe at target frame
                auto newKeyframe = std::make_unique<AnimationKeyframe>(toFrame);
                newKeyframe->setEasing(keyframe->getEasing());
                newKeyframe->setType(keyframe->getType());

                // Copy all item states
                QList<QGraphicsItem*> items = layer->getItems();
                for (QGraphicsItem* item : items) {
                    if (keyframe->hasItemState(item)) {
                        keyframe->applyItemState(item);
                        newKeyframe->captureItemState(item);
                    }
                }

                // Remove old keyframe and add new one
                layer->removeKeyframe(fromFrame);
                layer->addKeyframe(toFrame, std::move(newKeyframe));

                emit keyframeRemoved(m_currentLayer, fromFrame);
                emit keyframeAdded(m_currentLayer, toFrame);
            }
        }
    }
}

bool AnimationController::exportAnimation(const QString& filename, const QString& format, int quality, bool loop)
{
    if (filename.isEmpty()) {
        return false;
    }

    // Get canvas for rendering
    Canvas* canvas = m_mainWindow->findChild<Canvas*>();
    if (!canvas || !canvas->scene()) {
        QMessageBox::warning(m_mainWindow, "Export Error", "Cannot access canvas for export.");
        return false;
    }

    QGraphicsScene* scene = canvas->scene();
    QRectF sceneRect = scene->itemsBoundingRect();
    if (sceneRect.isEmpty()) {
        sceneRect = scene->sceneRect();
    }

    // Create temporary directory for frames
    QString tempDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation) + "/framedirector_export";
    QDir().mkpath(tempDir);

    QStringList frameFiles;

    emit exportProgress(0, m_totalFrames);

    // Render each frame
    for (int frame = 1; frame <= m_totalFrames; ++frame) {
        emit exportProgress(frame, m_totalFrames);
        QApplication::processEvents();

        // Set animation to this frame
        int originalFrame = m_currentFrame;
        setCurrentFrame(frame);

        // Render frame
        QImage frameImage(sceneRect.size().toSize(), QImage::Format_ARGB32);
        frameImage.fill(Qt::transparent);

        QPainter painter(&frameImage);
        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.setRenderHint(QPainter::SmoothPixmapTransform, true);

        scene->render(&painter, frameImage.rect(), sceneRect);
        painter.end();

        // Save frame
        QString frameFile = QString("%1/frame_%2.png").arg(tempDir).arg(frame, 4, 10, QChar('0'));
        frameImage.save(frameFile, "PNG");
        frameFiles.append(frameFile);

        // Restore original frame
        setCurrentFrame(originalFrame);
    }

    bool success = false;

    // Convert frames to final format
    if (format.toLower() == "gif") {
        success = exportToGif(frameFiles, filename, loop);
    }
    else if (format.toLower() == "mp4") {
        success = exportToMp4(frameFiles, filename, quality);
    }
    else {
        QMessageBox::warning(m_mainWindow, "Export Error", "Unsupported export format: " + format);
    }

    emit exportProgress(m_totalFrames, m_totalFrames);

    // Clean up temp files only when export succeeded
    if (success) {
        for (const QString& file : frameFiles) {
            QFile::remove(file);
        }
        QDir().rmdir(tempDir);
    } else {
        QMessageBox::warning(m_mainWindow, "Export Error", "Export failed. Frames have been left in:\n" + tempDir);
    }

    return success;
}

void AnimationController::exportFrame(int frame, const QString& filename)
{
    if (filename.isEmpty() || frame < 1 || frame > m_totalFrames) {
        return;
    }

    Canvas* canvas = m_mainWindow->findChild<Canvas*>();
    if (!canvas || !canvas->scene()) {
        QMessageBox::warning(m_mainWindow, "Export Error", "Cannot access canvas for export.");
        return;
    }

    // Set to specified frame
    int originalFrame = m_currentFrame;
    setCurrentFrame(frame);

    QGraphicsScene* scene = canvas->scene();
    QRectF sceneRect = scene->itemsBoundingRect();
    if (sceneRect.isEmpty()) {
        sceneRect = scene->sceneRect();
    }

    // Determine format from filename
    QFileInfo fileInfo(filename);
    QString format = fileInfo.suffix().toUpper();

    if (format == "SVG") {
        // Export as SVG
        QSvgGenerator generator;
        generator.setFileName(filename);
        generator.setSize(sceneRect.size().toSize());
        generator.setViewBox(sceneRect);
        generator.setTitle("FrameDirector Export");
        generator.setDescription(QString("Frame %1").arg(frame));

        QPainter painter(&generator);
        scene->render(&painter, sceneRect, sceneRect);
        painter.end();
    }
    else {
        // Export as raster image
        QImage image(sceneRect.size().toSize() * 2, QImage::Format_ARGB32); // 2x for better quality
        image.fill(Qt::transparent);

        QPainter painter(&image);
        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.setRenderHint(QPainter::SmoothPixmapTransform, true);

        scene->render(&painter, image.rect(), sceneRect);
        painter.end();

        // Scale down for final image
        QImage finalImage = image.scaled(sceneRect.size().toSize(), Qt::KeepAspectRatio, Qt::SmoothTransformation);

        if (!finalImage.save(filename, format.toLatin1().data())) {
            QMessageBox::warning(m_mainWindow, "Export Error", "Failed to save frame image.");
        }
    }

    // Restore original frame
    setCurrentFrame(originalFrame);
}

void AnimationController::onPlaybackTimer()
{
    nextFrame();
}

void AnimationController::updateAllLayers()
{
    for (auto& layer : m_layers) {
        updateLayerAtFrame(layer.get(), m_currentFrame);
    }
}

void AnimationController::updateLayerAtFrame(AnimationLayer* layer, int frame)
{
    if (layer) {
        layer->setCurrentFrame(frame);
    }
}

bool AnimationController::exportToGif(const QStringList& frameFiles, const QString& filename, bool loop)
{
    // This requires an external tool like ImageMagick
    QMessageBox::information(m_mainWindow, "GIF Export",
        "GIF export requires ImageMagick to be installed.\n\n"
        "Individual frames have been saved. You can use ImageMagick with:\n"
        "convert -delay " + QString::number(100 / m_frameRate) + " frame_*.png " + filename);

    QProcess process;
    QStringList arguments;
    arguments << "-delay" << QString::number(100 / m_frameRate);
    arguments << "-loop" << (loop ? "0" : "1");
    arguments << frameFiles;
    arguments << filename;
    process.setWorkingDirectory(QFileInfo(frameFiles.first()).absolutePath());

    process.start("convert", arguments);
    if (!process.waitForStarted(3000)) {
        QMessageBox::warning(m_mainWindow, "Export Error", "ImageMagick (convert) not found.");
        return false;
    }
    process.waitForFinished(-1);
    if (process.exitCode() == 0) {
        QMessageBox::information(m_mainWindow, "Export Complete",
            "GIF animation exported successfully to:\n" + filename);
        return true;
    }
    QMessageBox::warning(m_mainWindow, "Export Error",
        "ImageMagick conversion failed:\n" + process.readAllStandardError());
    return false;
}

bool AnimationController::exportToMp4(const QStringList& frameFiles, const QString& filename, int quality)
{
    // This requires FFmpeg
    QMessageBox::information(m_mainWindow, "MP4 Export",
        "MP4 export requires FFmpeg to be installed.\n\n"
        "Individual frames have been saved. You can use FFmpeg with:\n"
        "ffmpeg -framerate " + QString::number(m_frameRate) +
        " -i frame_%04d.png -vf pad=ceil(iw/2)*2:ceil(ih/2)*2 -c:v libx264 -pix_fmt yuv420p " + filename);

    QProcess process;
    QStringList arguments;
    arguments << "-framerate" << QString::number(m_frameRate);
    QString pattern = frameFiles.first();
    pattern.replace(QRegularExpression("frame_\\d{4}\\.png"), "frame_%04d.png");
    arguments << "-i" << pattern;
    arguments << "-vf" << "pad=ceil(iw/2)*2:ceil(ih/2)*2";
    arguments << "-c:v" << "libx264";
    int crf = 51 - (quality * 51) / 100;
    arguments << "-crf" << QString::number(crf);
    arguments << "-pix_fmt" << "yuv420p";
    arguments << "-y"; // Overwrite output file
    arguments << filename;
    process.setWorkingDirectory(QFileInfo(frameFiles.first()).absolutePath());

    process.start("ffmpeg", arguments);
    if (!process.waitForStarted(3000)) {
        QMessageBox::warning(m_mainWindow, "Export Error", "FFmpeg not found.");
        return false;
    }

    // Wait until encoding finishes
    process.waitForFinished(-1);
    if (process.exitCode() == 0) {
        QMessageBox::information(m_mainWindow, "Export Complete",
            "MP4 video exported successfully to:\n" + filename);
        return true;
    }
    QMessageBox::warning(m_mainWindow, "Export Error",
        "FFmpeg encoding failed:\n" + process.readAllStandardError());
    return false;
}
