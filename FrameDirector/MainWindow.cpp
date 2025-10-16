#include "MainWindow.h"
#include "Canvas.h"
#include "Timeline.h"
#include "Panels/LayerManager.h"
#include "Panels/PropertiesPanel.h"
#include "Panels/ToolsPanel.h"
#include "Panels/ColorPanel.h"
#include "Panels/AlignmentPanel.h"
#include "Tools/Tool.h"
#include "Tools/SelectionTool.h"
#include "Tools/DrawingTool.h"
#include "Tools/LineTool.h"          
#include "Tools/RectangleTool.h"     
#include "Tools/EllipseTool.h"       
#include "Tools/TextTool.h"          
#include "BucketFillTool.h"
#include "Tools/GradientFillTool.h"
#include "Tools/EraseTool.h"
#include "Commands/UndoCommands.h"
#include "Animation/AnimationLayer.h"
#include "Animation/AnimationKeyframe.h"
#include "Animation/AnimationController.h"
#include "Dialogs/ExportDialog.h"
#include "Import/ORAImporter.h"
#include "VectorGraphics/VectorGraphicsItem.h"

#include <QApplication>
#include <QMenuBar>
#include <QToolBar>
#include <QStatusBar>
#include <QSplitter>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QPushButton>
#include <QSlider>
#include <QSpinBox>
#include <QComboBox>
#include <QColorDialog>
#include <QtGlobal>
#include <QFontDialog>
#include <QFileDialog>
#include <QMessageBox>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSettings>
#include <QCloseEvent>
#include <QKeyEvent>
#include <QTimer>
#include <QUndoStack>
#include <QActionGroup>
#include <QDockWidget>
#include <QTabWidget>
#include <QDebug>
#include <QFileInfo>
#include <QPixmap>
#include <QSvgRenderer>
#include <QSvgGenerator>
#include <QGraphicsPixmapItem>
#include <QGraphicsSvgItem>
#include <QPainter>
#include <QBuffer>
#include <QIODevice>
#include <algorithm>
#include <cmath>
#include <QImageReader>
#include <QSvgWidget>
#include <QDir>
#include <QStandardPaths>
#include <qinputdialog.h>
#include <QUrl>
#include <QAudioDecoder>
#include <QAudioBuffer>
#include <QEventLoop>
#include <QImage>
#include <QVector>

namespace {
constexpr int SvgRawDataRole = Qt::UserRole + 200;
constexpr int SvgSourcePathRole = Qt::UserRole + 201;

QRectF resolveSvgBounds(const QGraphicsSvgItem* svgItem, QSvgRenderer* renderer)
{
    if (!renderer) {
        return QRectF();
    }

    const QString elementId = svgItem ? svgItem->elementId() : QString();
    if (!elementId.isEmpty()) {
        QRectF elementBounds = renderer->boundsOnElement(elementId);
        if (elementBounds.isValid() && !elementBounds.isEmpty()) {
            return elementBounds;
        }
    }

    QRectF bounds = svgItem ? svgItem->boundingRect() : QRectF();
    if (!bounds.isValid() || bounds.isEmpty()) {
        QRectF viewBox = renderer->viewBoxF();
        if (viewBox.isValid() && !viewBox.isEmpty()) {
            bounds = viewBox;
        }
    }

    if ((!bounds.isValid() || bounds.isEmpty()) && renderer) {
        QSize defaultSize = renderer->defaultSize();
        if (!defaultSize.isEmpty()) {
            bounds = QRectF(QPointF(0, 0), QSizeF(defaultSize));
        }
    }

    if (!bounds.isValid() || bounds.isEmpty()) {
        bounds = QRectF(0, 0, 100, 100);
    }

    return bounds;
}

QSize ensureValidSvgSize(const QSizeF& size)
{
    int width = static_cast<int>(std::ceil(size.width()));
    int height = static_cast<int>(std::ceil(size.height()));

    width = std::max(1, width);
    height = std::max(1, height);

    return QSize(width, height);
}

QByteArray captureSvgData(const QGraphicsSvgItem* svgItem)
{
    if (!svgItem) {
        return QByteArray();
    }

    QVariant rawData = svgItem->data(SvgRawDataRole);
    if (rawData.canConvert<QByteArray>()) {
        QByteArray stored = rawData.toByteArray();
        if (!stored.isEmpty()) {
            return stored;
        }
    }

    QSvgRenderer* renderer = svgItem->renderer();
    if (!renderer) {
        return QByteArray();
    }

    QByteArray svgData;
    QBuffer buffer(&svgData);
    if (!buffer.open(QIODevice::WriteOnly)) {
        return QByteArray();
    }

    QSvgGenerator generator;
    generator.setOutputDevice(&buffer);

    QRectF bounds = resolveSvgBounds(svgItem, renderer);
    generator.setViewBox(bounds);
    generator.setSize(ensureValidSvgSize(bounds.size()));

    QPainter painter(&generator);
    const QString elementId = svgItem->elementId();
    if (!elementId.isEmpty()) {
        renderer->render(&painter, elementId, bounds);
    }
    else {
        renderer->render(&painter, bounds);
    }
    painter.end();

    buffer.close();

    if (!svgData.isEmpty() && (!rawData.isValid() || rawData.toByteArray().isEmpty())) {
        auto* mutableItem = const_cast<QGraphicsSvgItem*>(svgItem);
        mutableItem->setData(SvgRawDataRole, svgData);
    }

    return svgData;
}

QGraphicsSvgItem* createSvgItemFromData(const QByteArray& svgData, const QVariant& sourceData)
{
    auto* svgItem = new QGraphicsSvgItem();
    if (!svgData.isEmpty()) {
        if (QSvgRenderer* renderer = svgItem->renderer()) {
            renderer->load(svgData);
        }
        svgItem->setData(SvgRawDataRole, svgData);
    }

    if (sourceData.isValid()) {
        svgItem->setData(SvgSourcePathRole, sourceData);
    }

    return svgItem;
}

QGraphicsSvgItem* deepCopySvgItem(const QGraphicsSvgItem* svgItem)
{
    if (!svgItem) {
        return nullptr;
    }

    QByteArray svgData = captureSvgData(svgItem);
    if (svgData.isEmpty()) {
        return nullptr;
    }

    QVariant sourceData = svgItem->data(SvgSourcePathRole);
    QGraphicsSvgItem* copy = createSvgItemFromData(svgData, sourceData);
    if (!copy) {
        return nullptr;
    }

    copy->setElementId(svgItem->elementId());
    return copy;
}
}

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , m_canvas(nullptr)
    , m_timeline(nullptr)
    , m_layerManager(nullptr)
    , m_toolsPanel(nullptr)
    , m_propertiesPanel(nullptr)
    , m_colorPanel(nullptr)
    , m_alignmentPanel(nullptr)
    , m_currentTool(SelectTool)
    , m_currentFile("")
    , m_isModified(false)
    , m_currentFrame(1)
    , m_totalFrames(150)
    , m_currentZoom(1.0)
    , m_frameRate(24)
    , m_isPlaying(false)
    , m_playbackTimer(new QTimer(this))
    , m_audioPlayer(new QMediaPlayer(this))
    , m_audioOutput(new QAudioOutput(this))
    , m_audioFrameLength(0)
    , m_undoStack(new QUndoStack(this))
    , m_currentLayerIndex(0)
    , m_currentStrokeColor(Qt::black)
    , m_currentFillColor(Qt::transparent)
    , m_currentStrokeWidth(2.0)
    , m_currentOpacity(1.0)
{
    setWindowTitle("FrameDirector");
    setMinimumSize(1200, 800);
    resize(1600, 1000);

    // Apply dark theme
    setupStyleSheet();

    // Setup undo/redo system
    m_undoStack->setUndoLimit(50);

    // Setup playback timer
    m_playbackTimer->setSingleShot(false);
    connect(m_playbackTimer, &QTimer::timeout, this, &MainWindow::onPlaybackTimer);

    // Setup audio playback
    m_audioPlayer->setAudioOutput(m_audioOutput);
    connect(m_audioPlayer, &QMediaPlayer::durationChanged,
        this, &MainWindow::onAudioDurationChanged);

    // Create UI components
    createActions();
    createMenus();
    createToolBars();
    createDockWindows();
    createStatusBar();

    // Setup central widget layout
    QWidget* centralWidget = new QWidget;
    setCentralWidget(centralWidget);

    m_mainSplitter = new QSplitter(Qt::Horizontal, this);

    // Create canvas
    m_canvas = new Canvas(this);
    m_canvas->setMinimumSize(400, 300);
    connect(m_canvas, &Canvas::selectionChanged, this, &MainWindow::onSelectionChanged);
    connect(m_canvas, &Canvas::mousePositionChanged, this, &MainWindow::onCanvasMouseMove);
    connect(m_canvas, &Canvas::zoomChanged, this, &MainWindow::onZoomChanged);

    // Create timeline dock
    m_timelineDock = new QDockWidget("Timeline", this);
    m_timeline = new Timeline(this);
    m_timeline->setMinimumHeight(200);
    m_timelineDock->setWidget(m_timeline);
    m_timelineDock->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);
    addDockWidget(Qt::BottomDockWidgetArea, m_timelineDock);
    m_timeline->setTotalFrames(m_totalFrames);
    connect(m_timeline, &Timeline::frameChanged, this, &MainWindow::onFrameChanged);
    connect(m_timeline, &Timeline::keyframeAdded, this, &MainWindow::addKeyframe);
    connect(m_timeline, &Timeline::totalFramesChanged, this, &MainWindow::onTotalFramesChanged);
    connect(m_canvas, &Canvas::layerChanged, m_timeline, &Timeline::setSelectedLayer);

    // Setup main layout
    QVBoxLayout* mainLayout = new QVBoxLayout(centralWidget);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->addWidget(m_mainSplitter);

    m_mainSplitter->addWidget(m_canvas);
    m_mainSplitter->setSizes({ 800, 300 });

    // Connect tool actions
    connect(m_toolActionGroup, QOverload<QAction*>::of(&QActionGroup::triggered),
        this, [this](QAction* action) {
            ToolType tool = static_cast<ToolType>(action->data().toInt());
            setTool(tool);
        });

    // Connect undo/redo system
    connect(m_undoStack, &QUndoStack::canUndoChanged, m_undoAction, &QAction::setEnabled);
    connect(m_undoStack, &QUndoStack::canRedoChanged, m_redoAction, &QAction::setEnabled);

    // Initial setup
    updateUI();
    readSettings();

    // Create default layer
    addLayer();
    setupTools();
    setupAnimationSystem();

    // Connect tools and canvas after everything is set up
    connectToolsAndCanvas();
    setupColorConnections();
    connectLayerManager();

    // Set default tool
    setTool(SelectTool);

    // Optional: Create a test shape to verify everything is working
    // Remove this line after confirming the canvas works

    qDebug() << "MainWindow setup complete";
}

MainWindow::~MainWindow()
{
    qDebug() << "MainWindow destructor called";

    // FIXED: Critical cleanup order to prevent crashes

    // 1. First, clear the undo stack to delete all commands safely
    if (m_undoStack) {
        qDebug() << "Clearing undo stack...";
        m_undoStack->clear();
    }

    // 2. Clean up tools that might have preview items
    qDebug() << "Cleaning up tools...";
    for (auto& toolPair : m_tools) {
        Tool* tool = toolPair.second.get();
        if (tool) {
            // Special cleanup for eraser tool
            if (toolPair.first == EraseTool) {
                ::EraseTool* eraserTool = dynamic_cast<::EraseTool*>(tool);
                if (eraserTool) {
                    eraserTool->cleanup();
                }
            }
            // Add cleanup for other tools as needed
        }
    }

    // 3. Clear the canvas before it's destroyed
    if (m_canvas) {
        qDebug() << "Clearing canvas...";
        m_canvas->clear();
    }

    // 4. Stop any running timers
    if (m_playbackTimer) {
        m_playbackTimer->stop();
    }

    qDebug() << "MainWindow destructor completed";

    // Qt will handle the rest of the cleanup in the correct order
}


void MainWindow::connectToolsAndCanvas()
{
    // Make sure canvas has the right colors and settings
    if (m_canvas) {
        m_canvas->setStrokeColor(m_currentStrokeColor);
        m_canvas->setFillColor(m_currentFillColor);
        m_canvas->setStrokeWidth(m_currentStrokeWidth);

        // Connect tool signals for item creation
        for (auto& toolPair : m_tools) {
            Tool* tool = toolPair.second.get();
            if (tool) {
                connect(tool, &Tool::itemCreated, [this](QGraphicsItem* item) {
                    if (item && m_canvas && m_canvas->scene()) {
                        onSelectionChanged();
                        m_statusLabel->setText("Item created");
                        m_isModified = true;
                    }
                    });
                qDebug() << "Connected tool:" << static_cast<int>(toolPair.first) << "Tool object:" << tool;
            }
        }

        qDebug() << "Tools and canvas connected successfully";
    }

    // FIXED: Connect tools panel for bucket fill tool
    if (m_toolsPanel) {
        connect(m_toolsPanel, &ToolsPanel::drawingToolSettingsRequested,
            this, &MainWindow::showDrawingToolSettings);

        connect(m_toolsPanel, &ToolsPanel::quickStrokeWidthChanged,
            this, &MainWindow::setDrawingToolStrokeWidth);

        connect(m_toolsPanel, &ToolsPanel::quickColorChanged,
            this, &MainWindow::setDrawingToolColor);
    }

    // Make sure the select tool is active by default
    if (m_toolsPanel) {
        m_toolsPanel->setActiveTool(SelectTool);
    }

    // Debug: Print all available tools
    qDebug() << "Available tools:" << m_tools.size();
    for (auto& toolPair : m_tools) {
        qDebug() << "Tool type:" << static_cast<int>(toolPair.first) << "Tool:" << toolPair.second.get();
    }

    if (m_propertiesPanel && m_canvas) {
        connect(m_canvas, &Canvas::selectionChanged,
            m_propertiesPanel, &PropertiesPanel::onSelectionChanged);

        connect(m_propertiesPanel, &PropertiesPanel::propertyChanged, [this]() {
            if (m_canvas) {
                m_canvas->storeCurrentFrameState();
                m_isModified = true;
            }
            });

        qDebug() << "Properties panel connected to canvas successfully";
    }
    else {
        qDebug() << "Warning: Could not connect properties panel - panel or canvas is null";
    }

    // Make sure undo stack is accessible to all tools
    for (auto& toolPair : m_tools) {
        Tool* tool = toolPair.second.get();
        if (tool) {
            qDebug() << "Tool" << static_cast<int>(toolPair.first) << "has access to undo stack";
        }
    }
}
void MainWindow::setupColorConnections()
{
    if (m_colorPanel && m_canvas) {
        // Set initial colors
        m_colorPanel->setStrokeColor(m_currentStrokeColor);
        m_colorPanel->setFillColor(m_currentFillColor);

        // Connect color changes to canvas AND all tools
        connect(m_colorPanel, &ColorPanel::strokeColorChanged, [this](const QColor& color) {
            m_currentStrokeColor = color;
            if (m_canvas) {
                m_canvas->setStrokeColor(color);
                updateSelectedItemsStroke(color);
            }

            // FIXED: Update drawing tool color
            updateDrawingToolColor(color);

            m_statusLabel->setText("Stroke color changed");
            });

        connect(m_colorPanel, &ColorPanel::fillColorChanged, [this](const QColor& color) {
            m_currentFillColor = color;
            if (m_canvas) {
                m_canvas->setFillColor(color);
                updateSelectedItemsFill(color);
            }

            // FIXED: Update bucket fill tool color
            updateBucketFillToolColor(color);

            m_statusLabel->setText("Fill color changed");
            });

        qDebug() << "Color connections established";
    }
}

void MainWindow::updateDrawingToolColor(const QColor& color)
{
    auto it = m_tools.find(DrawTool);
    if (it != m_tools.end()) {
        DrawingTool* drawingTool = dynamic_cast<DrawingTool*>(it->second.get());
        if (drawingTool) {
            drawingTool->setStrokeColor(color);
        }
    }
}

void MainWindow::updateBucketFillToolColor(const QColor& color)
{
    auto it = m_tools.find(BucketFillTool);
    if (it != m_tools.end()) {
        ::BucketFillTool* bucketTool = dynamic_cast<::BucketFillTool*>(it->second.get());
        if (bucketTool) {
            bucketTool->setFillColor(color);
            qDebug() << "Updated bucket fill tool color to:" << color.name();
        }
    }
}



// Add these new methods to update selected items
void MainWindow::updateSelectedItemsStroke(const QColor& color)
{
    if (!m_canvas || !m_canvas->scene()) return;

    QList<QGraphicsItem*> selectedItems = m_canvas->scene()->selectedItems();
    for (QGraphicsItem* item : selectedItems) {
        // Try to cast to different item types and update their stroke
        if (auto rectItem = qgraphicsitem_cast<QGraphicsRectItem*>(item)) {
            QPen pen = rectItem->pen();
            pen.setColor(color);
            rectItem->setPen(pen);
        }
        else if (auto ellipseItem = qgraphicsitem_cast<QGraphicsEllipseItem*>(item)) {
            QPen pen = ellipseItem->pen();
            pen.setColor(color);
            ellipseItem->setPen(pen);
        }
        else if (auto lineItem = qgraphicsitem_cast<QGraphicsLineItem*>(item)) {
            QPen pen = lineItem->pen();
            pen.setColor(color);
            lineItem->setPen(pen);
        }
        else if (auto pathItem = qgraphicsitem_cast<QGraphicsPathItem*>(item)) {
            QPen pen = pathItem->pen();
            pen.setColor(color);
            pathItem->setPen(pen);
        }
    }
}

void MainWindow::updateSelectedItemsFill(const QColor& color)
{
    if (!m_canvas || !m_canvas->scene()) return;

    QList<QGraphicsItem*> selectedItems = m_canvas->scene()->selectedItems();
    for (QGraphicsItem* item : selectedItems) {
        // Try to cast to different item types and update their fill
        if (auto rectItem = qgraphicsitem_cast<QGraphicsRectItem*>(item)) {
            rectItem->setBrush(QBrush(color));
        }
        else if (auto ellipseItem = qgraphicsitem_cast<QGraphicsEllipseItem*>(item)) {
            ellipseItem->setBrush(QBrush(color));
        }
        else if (auto pathItem = qgraphicsitem_cast<QGraphicsPathItem*>(item)) {
            pathItem->setBrush(QBrush(color));
        }
    }
}


void MainWindow::createTestShape()
{
    if (m_canvas && m_canvas->scene()) {
        // Create a test rectangle to verify the canvas is working
        QGraphicsRectItem* testRect = new QGraphicsRectItem(0, 0, 100, 100);
        testRect->setPen(QPen(Qt::red, 2));
        testRect->setBrush(QBrush(Qt::blue));
        testRect->setFlags(QGraphicsItem::ItemIsSelectable | QGraphicsItem::ItemIsMovable);
        testRect->setPos(0, 0);

        m_canvas->scene()->addItem(testRect);

        m_statusLabel->setText("Test shape created - canvas is working!");
        qDebug() << "Test shape created at scene center";
    }
}
void MainWindow::createActions()
{
    // File Menu Actions
    m_newAction = new QAction("&New", this);
    m_newAction->setIcon(QIcon(":/icons/new.png"));
    m_newAction->setShortcut(QKeySequence::New);
    m_newAction->setStatusTip("Create a new animation project");
    connect(m_newAction, &QAction::triggered, this, &MainWindow::newFile);

    m_openAction = new QAction("&Open", this);
    m_openAction->setIcon(QIcon(":/icons/open.png"));
    m_openAction->setShortcut(QKeySequence::Open);
    m_openAction->setStatusTip("Open an existing project");
    connect(m_openAction, &QAction::triggered, this, &MainWindow::open);

    m_saveAction = new QAction("&Save", this);
    m_saveAction->setIcon(QIcon(":/icons/save.png"));
    m_saveAction->setShortcut(QKeySequence::Save);
    m_saveAction->setStatusTip("Save the current project");
    connect(m_saveAction, &QAction::triggered, this, &MainWindow::save);

    m_saveAsAction = new QAction("Save &As...", this);
    m_saveAsAction->setIcon(QIcon(":/icons/save-as.png"));
    m_saveAsAction->setShortcut(QKeySequence::SaveAs);
    m_saveAsAction->setStatusTip("Save the project with a new name");
    connect(m_saveAsAction, &QAction::triggered, this, &MainWindow::saveAs);

    m_importImageAction = new QAction("Import &Image", this);
    m_importImageAction->setIcon(QIcon(":/icons/import.png"));
    m_importImageAction->setStatusTip("Import an image file");
    connect(m_importImageAction, &QAction::triggered, this, &MainWindow::importImage);

    m_importLayeredImageAction = new QAction("Import &Layered Image", this);
    m_importLayeredImageAction->setIcon(QIcon(":/icons/import.png"));
    m_importLayeredImageAction->setStatusTip("Import a layered image file");
    connect(m_importLayeredImageAction, &QAction::triggered, this, &MainWindow::importLayeredImage);

    m_importVectorAction = new QAction("Import &Vector", this);
    m_importVectorAction->setIcon(QIcon(":/icons/import.png"));
    m_importVectorAction->setStatusTip("Import a vector file");
    connect(m_importVectorAction, &QAction::triggered, this, &MainWindow::importVector);

    m_importAudioAction = new QAction("Import &Audio", this);
    m_importAudioAction->setIcon(QIcon(":/icons/import.png"));
    m_importAudioAction->setStatusTip("Import an audio file");
    connect(m_importAudioAction, &QAction::triggered, this, &MainWindow::importAudio);

    m_exportAnimationAction = new QAction("Export &Animation", this);
    m_exportAnimationAction->setIcon(QIcon(":/icons/export.png"));
    m_exportAnimationAction->setStatusTip("Export as video/GIF");
    connect(m_exportAnimationAction, &QAction::triggered, this, &MainWindow::exportAnimation);

    m_exportFrameAction = new QAction("Export &Frame", this);
    m_exportFrameAction->setIcon(QIcon(":/icons/export.png"));
    m_exportFrameAction->setStatusTip("Export current frame as image");
    connect(m_exportFrameAction, &QAction::triggered, this, &MainWindow::exportFrame);

    //m_exportSVGAction = new QAction("Export &SVG", this);
    //m_exportSVGAction->setIcon(QIcon(":/icons/export.png"));
    //m_exportSVGAction->setStatusTip("Export as SVG file");
    //connect(m_exportSVGAction, &QAction::triggered, this, &MainWindow::exportSVG);

    m_exitAction = new QAction("E&xit", this);
    m_exitAction->setIcon(QIcon(":/icons/exit.png"));
    m_exitAction->setShortcut(QKeySequence::Quit);
    m_exitAction->setStatusTip("Exit FrameDirector");
    connect(m_exitAction, &QAction::triggered, this, &QWidget::close);

    // Edit Menu Actions
    m_undoAction = new QAction("&Undo", this);
    m_undoAction->setIcon(QIcon(":/icons/undo.png"));
    m_undoAction->setShortcut(QKeySequence::Undo);
    m_undoAction->setEnabled(false);
    connect(m_undoAction, &QAction::triggered, this, &MainWindow::undo);

    m_redoAction = new QAction("&Redo", this);
    m_redoAction->setIcon(QIcon(":/icons/redo.png"));
    m_redoAction->setShortcut(QKeySequence::Redo);
    m_redoAction->setEnabled(false);
    connect(m_redoAction, &QAction::triggered, this, &MainWindow::redo);

    m_cutAction = new QAction("Cu&t", this);
    m_cutAction->setIcon(QIcon(":/icons/Cut.png"));
    m_cutAction->setShortcut(QKeySequence::Cut);
    connect(m_cutAction, &QAction::triggered, this, &MainWindow::cut);

    m_copyAction = new QAction("&Copy", this);
    m_copyAction->setIcon(QIcon(":/icons/Copy.png"));
    m_copyAction->setShortcut(QKeySequence::Copy);
    connect(m_copyAction, &QAction::triggered, this, &MainWindow::copy);

    m_pasteAction = new QAction("&Paste", this);
    m_pasteAction->setIcon(QIcon(":/icons/Paste.png"));
    m_pasteAction->setShortcut(QKeySequence::Paste);
    connect(m_pasteAction, &QAction::triggered, this, &MainWindow::paste);

    m_selectAllAction = new QAction("Select &All", this);
    m_selectAllAction->setIcon(QIcon(":/icons/select-all.png"));
    m_selectAllAction->setShortcut(QKeySequence::SelectAll);
    connect(m_selectAllAction, &QAction::triggered, this, &MainWindow::selectAll);

    m_groupAction = new QAction("&Group", this);
    m_groupAction->setIcon(QIcon(":/icons/group.png"));
    m_groupAction->setShortcut(QKeySequence("Ctrl+G"));
    connect(m_groupAction, &QAction::triggered, this, &MainWindow::group);

    m_ungroupAction = new QAction("&Ungroup", this);
    m_ungroupAction->setIcon(QIcon(":/icons/ungroup.png"));
    m_ungroupAction->setShortcut(QKeySequence("Ctrl+Shift+G"));
    connect(m_ungroupAction, &QAction::triggered, this, &MainWindow::ungroup);

    // View Menu Actions
    m_zoomInAction = new QAction("Zoom &In", this);
    m_zoomInAction->setIcon(QIcon(":/icons/zoom-in.png"));
    m_zoomInAction->setShortcut(QKeySequence::ZoomIn);
    connect(m_zoomInAction, &QAction::triggered, this, &MainWindow::zoomIn);

    m_zoomOutAction = new QAction("Zoom &Out", this);
    m_zoomOutAction->setIcon(QIcon(":/icons/zoom-out.png"));
    m_zoomOutAction->setShortcut(QKeySequence::ZoomOut);
    connect(m_zoomOutAction, &QAction::triggered, this, &MainWindow::zoomOut);

    m_zoomToFitAction = new QAction("Zoom to &Fit", this);
    m_zoomToFitAction->setIcon(QIcon(":/icons/zoom-fit.png"));
    m_zoomToFitAction->setShortcut(QKeySequence("Ctrl+0"));
    connect(m_zoomToFitAction, &QAction::triggered, this, &MainWindow::zoomToFit);

    m_toggleGridAction = new QAction("Show &Grid", this);
    m_toggleGridAction->setIcon(QIcon(":/icons/Grid.png"));
    m_toggleGridAction->setCheckable(true);
    m_toggleGridAction->setChecked(true);
    connect(m_toggleGridAction, &QAction::triggered, this, &MainWindow::toggleGrid);

    m_toggleSnapAction = new QAction("&Snap to Grid", this);
    m_toggleSnapAction->setIcon(QIcon(":/icons/snap.png"));
    m_toggleSnapAction->setCheckable(true);
    connect(m_toggleSnapAction, &QAction::triggered, this, &MainWindow::toggleSnapToGrid);

    m_toggleRulersAction = new QAction("Show &Rulers", this);
    m_toggleRulersAction->setIcon(QIcon(":/icons/rulers.png"));
    m_toggleRulersAction->setCheckable(true);
    connect(m_toggleRulersAction, &QAction::triggered, this, &MainWindow::toggleRulers);

    // Animation Menu Actions
    m_playAction = new QAction("&Play", this);
    m_playAction->setIcon(QIcon(":/icons/Play.png"));
    m_playAction->setShortcut(QKeySequence("Space"));
    m_playAction->setStatusTip("Play animation");
    connect(m_playAction, &QAction::triggered, this, &MainWindow::play);

    m_stopAction = new QAction("&Stop", this);
    m_stopAction->setIcon(QIcon(":/icons/stop.png"));
    m_stopAction->setShortcut(QKeySequence("Shift+Space"));
    m_stopAction->setStatusTip("Stop animation");
    connect(m_stopAction, &QAction::triggered, this, &MainWindow::stop);

    m_nextFrameAction = new QAction("&Next Frame", this);
    m_nextFrameAction->setIcon(QIcon(":/icons/arrow-right.png"));
    m_nextFrameAction->setShortcut(QKeySequence("Right"));
    connect(m_nextFrameAction, &QAction::triggered, this, &MainWindow::nextFrame);

    m_prevFrameAction = new QAction("&Previous Frame", this);
    m_prevFrameAction->setIcon(QIcon(":/icons/arrow-left.png"));
    m_prevFrameAction->setShortcut(QKeySequence("Left"));
    connect(m_prevFrameAction, &QAction::triggered, this, &MainWindow::previousFrame);

    m_firstFrameAction = new QAction("&First Frame", this);
    m_firstFrameAction->setIcon(QIcon(":/icons/double-arrow-left.png"));
    m_firstFrameAction->setShortcut(QKeySequence("Home"));
    connect(m_firstFrameAction, &QAction::triggered, this, &MainWindow::firstFrame);

    m_lastFrameAction = new QAction("&Last Frame", this);
    m_lastFrameAction->setIcon(QIcon(":/icons/double-arrow-right.png"));
    m_lastFrameAction->setShortcut(QKeySequence("End"));
    connect(m_lastFrameAction, &QAction::triggered, this, &MainWindow::lastFrame);

    // ENHANCED: Existing addKeyframe action with F6 shortcut
    m_addKeyframeAction = new QAction("Add &Keyframe", this);
    m_addKeyframeAction->setIcon(QIcon(":/icons/branch-open.png"));
    m_addKeyframeAction->setShortcut(QKeySequence("F6"));  // NEW: Enhanced with F6
    m_addKeyframeAction->setStatusTip("Insert keyframe with current content");  // NEW: Enhanced tooltip
    connect(m_addKeyframeAction, &QAction::triggered, this, &MainWindow::addKeyframe);

    m_copyFrameAction = new QAction("&Copy Frame", this);
    m_copyFrameAction->setIcon(QIcon(":/icons/Copy.png"));
    m_copyFrameAction->setShortcut(QKeySequence("Ctrl+Shift+C"));
    m_copyFrameAction->setStatusTip("Copy current frame content");
    connect(m_copyFrameAction, &QAction::triggered, this, &MainWindow::copyCurrentFrame);

    m_pasteFrameAction = new QAction("&Paste Frame", this);
    m_pasteFrameAction->setIcon(QIcon(":/icons/Paste.png"));
    m_pasteFrameAction->setShortcut(QKeySequence("Ctrl+Shift+V"));
    m_pasteFrameAction->setStatusTip("Paste copied frame content to current frame");
    m_pasteFrameAction->setEnabled(false); // Initially disabled
    connect(m_pasteFrameAction, &QAction::triggered, this, &MainWindow::pasteFrame);

    m_blankKeyframeAction = new QAction("Create &Blank Keyframe", this);
    m_blankKeyframeAction->setIcon(QIcon(":/icons/add-empty.png"));
    m_blankKeyframeAction->setShortcut(QKeySequence("Ctrl+Shift+K"));
    m_blankKeyframeAction->setStatusTip("Create blank keyframe (clear current frame)");
    connect(m_blankKeyframeAction, &QAction::triggered, this, &MainWindow::createBlankKeyframe);

    // NEW: Enhanced frame creation actions
    m_insertFrameAction = new QAction("Insert &Frame", this);
    m_insertFrameAction->setIcon(QIcon(":/icons/arrow-right.png"));  // Reuse existing icon
    m_insertFrameAction->setShortcut(QKeySequence("F5"));
    m_insertFrameAction->setStatusTip("Insert frame extending from previous keyframe");
    connect(m_insertFrameAction, &QAction::triggered, this, &MainWindow::insertFrame);

    m_insertBlankKeyframeAction = new QAction("Insert &Blank Keyframe", this);
    m_insertBlankKeyframeAction->setIcon(QIcon(":/icons/add-empty.png"));  // Reuse existing icon
    m_insertBlankKeyframeAction->setShortcut(QKeySequence("F7"));
    m_insertBlankKeyframeAction->setStatusTip("Insert blank keyframe (clears content)");
    connect(m_insertBlankKeyframeAction, &QAction::triggered, this, &MainWindow::insertBlankKeyframe);

    m_clearFrameAction = new QAction("&Clear Frame", this);
    m_clearFrameAction->setIcon(QIcon(":/icons/close.png"));  // Reuse stop icon for "clear"
    m_clearFrameAction->setShortcut(QKeySequence("Shift+F5"));
    m_clearFrameAction->setStatusTip("Clear current frame content");
    connect(m_clearFrameAction, &QAction::triggered, this, &MainWindow::clearCurrentFrame);

    m_convertToKeyframeAction = new QAction("Convert to &Keyframe", this);
    m_convertToKeyframeAction->setIcon(QIcon(":/icons/branch-open.png"));  // Reuse keyframe icon
    m_convertToKeyframeAction->setShortcut(QKeySequence("F8"));
    m_convertToKeyframeAction->setStatusTip("Convert extended frame to keyframe");
    connect(m_convertToKeyframeAction, &QAction::triggered, this, &MainWindow::convertToKeyframe);

    // NEW: Enhanced keyframe navigation actions
    m_nextKeyframeAction = new QAction("Next &Keyframe", this);
    QPixmap nextKeyframePixmap = QIcon(":/icons/arrow-right.png").pixmap(16, 16);
    m_nextKeyframeAction->setIcon(QIcon(nextKeyframePixmap));
    m_nextKeyframeAction->setShortcut(QKeySequence("Ctrl+Right"));
    m_nextKeyframeAction->setStatusTip("Go to next keyframe");
    connect(m_nextKeyframeAction, &QAction::triggered, this, &MainWindow::nextKeyframe);

    m_prevKeyframeAction = new QAction("Previous &Keyframe", this);
    m_prevKeyframeAction->setIcon(QIcon(":/icons/arrow-left.png"));
    m_prevKeyframeAction->setShortcut(QKeySequence("Ctrl+Left"));
    m_prevKeyframeAction->setStatusTip("Go to previous keyframe");
    connect(m_prevKeyframeAction, &QAction::triggered, this, &MainWindow::previousKeyframe);

    m_setTimelineLengthAction = new QAction("Set Timeline &Length...", this);
    m_setTimelineLengthAction->setStatusTip("Set the total number of frames in the timeline");
    connect(m_setTimelineLengthAction, &QAction::triggered, this, &MainWindow::setTimelineLength);

    // Tool Actions
    m_toolActionGroup = new QActionGroup(this);

    m_selectToolAction = new QAction("&Select Tool", this);
    m_selectToolAction->setIcon(QIcon(":/icons/tool-select.png"));
    m_selectToolAction->setShortcut(QKeySequence("V"));
    m_selectToolAction->setCheckable(true);
    m_selectToolAction->setChecked(true);
    m_selectToolAction->setData(static_cast<int>(SelectTool));
    m_toolActionGroup->addAction(m_selectToolAction);

    m_drawToolAction = new QAction("&Draw Tool", this);
    m_drawToolAction->setIcon(QIcon(":/icons/tool-draw.png"));
    m_drawToolAction->setShortcut(QKeySequence("P"));
    m_drawToolAction->setCheckable(true);
    m_drawToolAction->setData(static_cast<int>(DrawTool));
    m_toolActionGroup->addAction(m_drawToolAction);

    m_lineToolAction = new QAction("&Line Tool", this);
    m_lineToolAction->setIcon(QIcon(":/icons/tool-line.png"));
    m_lineToolAction->setShortcut(QKeySequence("L"));
    m_lineToolAction->setCheckable(true);
    m_lineToolAction->setData(static_cast<int>(LineTool));
    m_toolActionGroup->addAction(m_lineToolAction);

    m_rectangleToolAction = new QAction("&Rectangle Tool", this);
    m_rectangleToolAction->setIcon(QIcon(":/icons/tool-rectangle.png"));
    m_rectangleToolAction->setShortcut(QKeySequence("R"));
    m_rectangleToolAction->setCheckable(true);
    m_rectangleToolAction->setData(static_cast<int>(RectangleTool));
    m_toolActionGroup->addAction(m_rectangleToolAction);

    m_ellipseToolAction = new QAction("&Ellipse Tool", this);
    m_ellipseToolAction->setIcon(QIcon(":/icons/tool-ellipse.png"));
    m_ellipseToolAction->setShortcut(QKeySequence("O"));
    m_ellipseToolAction->setCheckable(true);
    m_ellipseToolAction->setData(static_cast<int>(EllipseTool));
    m_toolActionGroup->addAction(m_ellipseToolAction);

    m_textToolAction = new QAction("&Text Tool", this);
    m_textToolAction->setIcon(QIcon(":/icons/tool-text.png"));
    m_textToolAction->setShortcut(QKeySequence("T"));
    m_textToolAction->setCheckable(true);
    m_textToolAction->setData(static_cast<int>(TextTool));
    m_toolActionGroup->addAction(m_textToolAction);

    // Alignment Actions
    m_alignLeftAction = new QAction("Align &Left", this);
    m_alignLeftAction->setIcon(QIcon(":/icons/arrow-left.png"));
    connect(m_alignLeftAction, &QAction::triggered, this, &MainWindow::alignLeft);

    m_alignCenterAction = new QAction("Align &Center", this);
    m_alignCenterAction->setIcon(QIcon(":/icons/select-all.png")); // Repurpose for center align
    connect(m_alignCenterAction, &QAction::triggered, this, &MainWindow::alignCenter);

    m_alignRightAction = new QAction("Align &Right", this);
    m_alignRightAction->setIcon(QIcon(":/icons/arrow-right.png"));
    connect(m_alignRightAction, &QAction::triggered, this, &MainWindow::alignRight);

    m_alignTopAction = new QAction("Align &Top", this);
    m_alignTopAction->setIcon(QIcon(":/icons/up-arrow.png"));
    connect(m_alignTopAction, &QAction::triggered, this, &MainWindow::alignTop);

    m_alignMiddleAction = new QAction("Align &Middle", this);
    m_alignMiddleAction->setIcon(QIcon(":/icons/select-all.png"));
    connect(m_alignMiddleAction, &QAction::triggered, this, &MainWindow::alignMiddle);

    m_alignBottomAction = new QAction("Align &Bottom", this);
    m_alignBottomAction->setIcon(QIcon(":/icons/down-arrow.png"));
    connect(m_alignBottomAction, &QAction::triggered, this, &MainWindow::alignBottom);

    m_distributeHorizontallyAction = new QAction("Distribute &Horizontally", this);
    m_distributeHorizontallyAction->setIcon(QIcon(":/icons/arrow-right.png"));
    connect(m_distributeHorizontallyAction, &QAction::triggered, this, &MainWindow::distributeHorizontally);

    m_distributeVerticallyAction = new QAction("Distribute &Vertically", this);
    m_distributeVerticallyAction->setIcon(QIcon(":/icons/up-arrow.png"));
    connect(m_distributeVerticallyAction, &QAction::triggered, this, &MainWindow::distributeVertically);

    // Transform Actions
    m_bringToFrontAction = new QAction("Bring to &Front", this);
    m_bringToFrontAction->setIcon(QIcon(":/icons/up-arrow.png"));
    m_bringToFrontAction->setShortcut(QKeySequence("Ctrl+Shift+]"));
    connect(m_bringToFrontAction, &QAction::triggered, this, &MainWindow::bringToFront);

    m_bringForwardAction = new QAction("Bring &Forward", this);
    m_bringForwardAction->setIcon(QIcon(":/icons/up-arrow.png"));
    m_bringForwardAction->setShortcut(QKeySequence("Ctrl+]"));
    connect(m_bringForwardAction, &QAction::triggered, this, &MainWindow::bringForward);

    m_sendBackwardAction = new QAction("Send &Backward", this);
    m_sendBackwardAction->setIcon(QIcon(":/icons/down-arrow.png"));
    m_sendBackwardAction->setShortcut(QKeySequence("Ctrl+["));
    connect(m_sendBackwardAction, &QAction::triggered, this, &MainWindow::sendBackward);

    m_sendToBackAction = new QAction("Send to &Back", this);
    m_sendToBackAction->setIcon(QIcon(":/icons/down-arrow.png"));
    m_sendToBackAction->setShortcut(QKeySequence("Ctrl+Shift+["));
    connect(m_sendToBackAction, &QAction::triggered, this, &MainWindow::sendToBack);

    m_flipHorizontalAction = new QAction("Flip &Horizontal", this);
    m_flipHorizontalAction->setIcon(QIcon(":/icons/arrow-right.png"));
    connect(m_flipHorizontalAction, &QAction::triggered, this, &MainWindow::flipHorizontal);

    m_flipVerticalAction = new QAction("Flip &Vertical", this);
    m_flipVerticalAction->setIcon(QIcon(":/icons/up-arrow.png"));
    connect(m_flipVerticalAction, &QAction::triggered, this, &MainWindow::flipVertical);

    m_rotateClockwiseAction = new QAction("Rotate &Clockwise", this);
    m_rotateClockwiseAction->setIcon(QIcon(":/icons/redo.png")); // Redo icon looks like rotation
    connect(m_rotateClockwiseAction, &QAction::triggered, this, &MainWindow::rotateClockwise);

    m_rotateCounterClockwiseAction = new QAction("Rotate &Counter-Clockwise", this);
    m_rotateCounterClockwiseAction->setIcon(QIcon(":/icons/undo.png")); // Undo icon for counter-rotation
    connect(m_rotateCounterClockwiseAction, &QAction::triggered, this, &MainWindow::rotateCounterClockwise);
}
void MainWindow::createMenus()
{
    m_fileMenu = menuBar()->addMenu("&File");
    m_fileMenu->addAction(m_newAction);
    m_fileMenu->addAction(m_openAction);
    m_fileMenu->addSeparator();
    m_fileMenu->addAction(m_saveAction);
    m_fileMenu->addAction(m_saveAsAction);
    m_fileMenu->addSeparator();

    m_importMenu = m_fileMenu->addMenu("&Import");
    m_importMenu->addAction(m_importImageAction);
    m_importMenu->addAction(m_importLayeredImageAction);
    m_importMenu->addAction(m_importVectorAction);
    m_importMenu->addAction(m_importAudioAction); // NEW

    m_exportMenu = m_fileMenu->addMenu("&Export");
    m_exportMenu->addAction(m_exportAnimationAction);
    m_exportMenu->addAction(m_exportFrameAction);
    m_exportMenu->addAction(m_exportSVGAction);

    m_fileMenu->addSeparator();
    m_fileMenu->addAction(m_exitAction);

    // Edit Menu
    m_editMenu = menuBar()->addMenu("&Edit");
    m_editMenu->addAction(m_undoAction);
    m_editMenu->addAction(m_redoAction);
    m_editMenu->addSeparator();
    m_editMenu->addAction(m_cutAction);
    m_editMenu->addAction(m_copyAction);
    m_editMenu->addAction(m_pasteAction);
    m_editMenu->addSeparator();
    m_editMenu->addAction(m_selectAllAction);
    m_editMenu->addSeparator();
    m_editMenu->addAction(m_groupAction);
    m_editMenu->addAction(m_ungroupAction);

    // Object Menu
    m_objectMenu = menuBar()->addMenu("&Object");

    m_alignMenu = m_objectMenu->addMenu("&Align");
    m_alignMenu->addAction(m_alignLeftAction);
    m_alignMenu->addAction(m_alignCenterAction);
    m_alignMenu->addAction(m_alignRightAction);
    m_alignMenu->addSeparator();
    m_alignMenu->addAction(m_alignTopAction);
    m_alignMenu->addAction(m_alignMiddleAction);
    m_alignMenu->addAction(m_alignBottomAction);
    m_alignMenu->addSeparator();
    m_alignMenu->addAction(m_distributeHorizontallyAction);
    m_alignMenu->addAction(m_distributeVerticallyAction);

    m_arrangeMenu = m_objectMenu->addMenu("A&rrange");
    m_arrangeMenu->addAction(m_bringToFrontAction);
    m_arrangeMenu->addAction(m_bringForwardAction);
    m_arrangeMenu->addAction(m_sendBackwardAction);
    m_arrangeMenu->addAction(m_sendToBackAction);

    m_transformMenu = m_objectMenu->addMenu("&Transform");
    m_transformMenu->addAction(m_flipHorizontalAction);
    m_transformMenu->addAction(m_flipVerticalAction);
    m_transformMenu->addAction(m_rotateClockwiseAction);
    m_transformMenu->addAction(m_rotateCounterClockwiseAction);

    // View Menu
    m_viewMenu = menuBar()->addMenu("&View");
    m_viewMenu->addAction(m_zoomInAction);
    m_viewMenu->addAction(m_zoomOutAction);
    m_viewMenu->addAction(m_zoomToFitAction);
    m_viewMenu->addSeparator();
    m_viewMenu->addAction(m_toggleGridAction);
    m_viewMenu->addAction(m_toggleSnapAction);
    m_viewMenu->addAction(m_toggleRulersAction);

    // ENHANCED: Animation Menu with frame extension support
    m_animationMenu = menuBar()->addMenu("&Animation");

    // Playback controls
    m_animationMenu->addAction(m_playAction);
    m_animationMenu->addAction(m_stopAction);
    m_animationMenu->addSeparator();
    m_animationMenu->addAction(m_applyTweeningAction);
    m_animationMenu->addAction(m_removeTweeningAction);

    // Frame navigation submenu
    QMenu* navigationMenu = m_animationMenu->addMenu("&Navigation");
    navigationMenu->addAction(m_firstFrameAction);
    navigationMenu->addAction(m_prevFrameAction);
    navigationMenu->addAction(m_nextFrameAction);
    navigationMenu->addAction(m_lastFrameAction);
    navigationMenu->addSeparator();
    navigationMenu->addAction(m_prevKeyframeAction);   // NEW: Enhanced keyframe navigation
    navigationMenu->addAction(m_nextKeyframeAction);   // NEW: Enhanced keyframe navigation

    // Quick access to common navigation (keep for compatibility)
    m_animationMenu->addAction(m_nextFrameAction);
    m_animationMenu->addAction(m_prevFrameAction);
    m_animationMenu->addSeparator();
    m_animationMenu->addAction(m_firstFrameAction);
    m_animationMenu->addAction(m_lastFrameAction);
    m_animationMenu->addSeparator();

    // Frame creation submenu - NEW: Enhanced frame management
    QMenu* frameMenu = m_animationMenu->addMenu("&Frames");
    //frameMenu->addAction(m_insertFrameAction);         // NEW: F5 - Insert extended frame
    frameMenu->addAction(m_addKeyframeAction);         // ENHANCED: F6 - Insert keyframe (existing)
    frameMenu->addAction(m_insertBlankKeyframeAction); // NEW: F7 - Insert blank keyframe
    frameMenu->addSeparator();
    frameMenu->addAction(m_clearFrameAction);          // NEW: Shift+F5 - Clear frame
    //frameMenu->addAction(m_convertToKeyframeAction);   // NEW: F8 - Convert to keyframe
    frameMenu->addSeparator();
    frameMenu->addAction(m_copyFrameAction);           // Existing: Copy frame content
    frameMenu->addAction(m_pasteFrameAction);           // NEW: Paste frame content
    m_animationMenu->addSeparator();
    m_animationMenu->addAction(m_setTimelineLengthAction);  // FIX: Add timeline length setting

    // Help Menu
    m_helpMenu = menuBar()->addMenu("&Help");
    m_helpMenu->addAction("&About", this, [this]() {
        QMessageBox::about(this, "About FrameDirector",
            "FrameDirector v1.0\n"
            "https://intelligencecasino.neocities.org/");
        });
}

void MainWindow::createToolBars()
{
    // File Toolbar
    m_fileToolBar = addToolBar("File");
    m_fileToolBar->addAction(m_newAction);
    m_fileToolBar->addAction(m_openAction);
    m_fileToolBar->addAction(m_saveAction);
    m_fileToolBar->addSeparator();
    m_fileToolBar->addAction(m_undoAction);
    m_fileToolBar->addAction(m_redoAction);

    // Tools Toolbar
    m_toolsToolBar = addToolBar("Tools");
    m_toolsToolBar->addAction(m_selectToolAction);
    m_toolsToolBar->addAction(m_drawToolAction);
    m_toolsToolBar->addAction(m_lineToolAction);
    m_toolsToolBar->addAction(m_rectangleToolAction);
    m_toolsToolBar->addAction(m_ellipseToolAction);
    m_toolsToolBar->addAction(m_textToolAction);

    // View Toolbar
    m_viewToolBar = addToolBar("View");
    m_viewToolBar->addAction(m_zoomInAction);
    m_viewToolBar->addAction(m_zoomOutAction);
    m_viewToolBar->addAction(m_zoomToFitAction);

    // ENHANCED: Animation Toolbar with frame extension support
    m_animationToolBar = addToolBar("Animation");

    // Playback controls
    m_animationToolBar->addAction(m_firstFrameAction);
    m_animationToolBar->addAction(m_prevFrameAction);
    m_animationToolBar->addAction(m_playAction);
    m_animationToolBar->addAction(m_stopAction);
    m_animationToolBar->addAction(m_nextFrameAction);
    m_animationToolBar->addAction(m_lastFrameAction);

    m_animationToolBar->addSeparator();

    // NEW: Enhanced keyframe navigation
    m_animationToolBar->addAction(m_prevKeyframeAction);  // NEW: Ctrl+Left - Previous keyframe
    m_animationToolBar->addAction(m_nextKeyframeAction);  // NEW: Ctrl+Right - Next keyframe

    m_animationToolBar->addSeparator();

    // NEW: Enhanced frame creation tools
    //m_animationToolBar->addAction(m_insertFrameAction);         // NEW: F5 - Insert extended frame
    m_animationToolBar->addAction(m_addKeyframeAction);         // ENHANCED: F6 - Insert keyframe (existing)
    m_animationToolBar->addAction(m_insertBlankKeyframeAction); // NEW: F7 - Insert blank keyframe

    m_animationToolBar->addSeparator();

    // NEW: Additional frame operations
    //m_animationToolBar->addAction(m_convertToKeyframeAction);   // NEW: F8 - Convert to keyframe
    m_animationToolBar->addAction(m_clearFrameAction);          // NEW: Shift+F5 - Clear frame
}

void MainWindow::setupTools()
{
    qDebug() << "Setting up tools...";

    try {
        m_tools[SelectTool] = std::make_unique<SelectionTool>(this);
        qDebug() << "Created SelectionTool:" << m_tools[SelectTool].get();

        m_tools[DrawTool] = std::make_unique<DrawingTool>(this);
        qDebug() << "Created DrawingTool:" << m_tools[DrawTool].get();

        m_tools[LineTool] = std::make_unique<::LineTool>(this);
        qDebug() << "Created LineTool:" << m_tools[LineTool].get();

        m_tools[RectangleTool] = std::make_unique<::RectangleTool>(this);
        qDebug() << "Created RectangleTool:" << m_tools[RectangleTool].get();

        m_tools[EllipseTool] = std::make_unique<::EllipseTool>(this);
        qDebug() << "Created EllipseTool:" << m_tools[EllipseTool].get();

        m_tools[TextTool] = std::make_unique<::TextTool>(this);
        qDebug() << "Created TextTool:" << m_tools[TextTool].get();

        // FIXED: Create bucket fill tool
        m_tools[BucketFillTool] = std::make_unique<::BucketFillTool>(this);
        qDebug() << "Created BucketFillTool:" << m_tools[BucketFillTool].get();

        m_tools[GradientFillTool] = std::make_unique<::GradientFillTool>(this);
        qDebug() << "Created GradientFillTool:" << m_tools[GradientFillTool].get();

        m_tools[EraseTool] = std::make_unique<::EraseTool>(this);
        qDebug() << "Created EraseTool:" << m_tools[EraseTool].get();

        qDebug() << "All tools created successfully. Total tools:" << m_tools.size();

        // FIXED: Initialize tool colors after creation
        initializeToolColors();
    }
    catch (const std::exception& e) {
        qDebug() << "Error creating tools:" << e.what();
    }
}

void MainWindow::initializeToolColors()
{
    // Set initial colors for all tools
    updateDrawingToolColor(m_currentStrokeColor);
    updateBucketFillToolColor(m_currentFillColor);

    qDebug() << "Tool colors initialized";
}


void MainWindow::setupAnimationSystem()
{
    // Set up default animation properties
    m_playbackTimer->setInterval(1000 / m_frameRate);

    // Initialize keyframe system
    m_keyframes.clear();

    // Connect timeline signals
    if (m_timeline) {
        connect(m_timeline, &Timeline::frameChanged, this, &MainWindow::onFrameChanged);
        connect(m_timeline, &Timeline::frameRateChanged, this, &MainWindow::setFrameRate);
    }
}

void MainWindow::setupStyleSheet()
{
    setStyleSheet(
        "QMainWindow {"
        "    background-color: #2D2D30;"
        "    color: #FFFFFF;"
        "}"
        "QMenuBar {"
        "    background-color: #3E3E42;"
        "    color: #FFFFFF;"
        "    border: none;"
        "}"
        "QMenuBar::item {"
        "    background-color: transparent;"
        "    padding: 6px 8px;"
        "}"
        "QMenuBar::item:selected {"
        "    background-color: #4A4A4F;"
        "}"
        "QMenu {"
        "    background-color: #3E3E42;"
        "    color: #FFFFFF;"
        "    border: 1px solid #5A5A5C;"
        "}"
        "QMenu::item {"
        "    padding: 6px 20px;"
        "}"
        "QMenu::item:selected {"
        "    background-color: #4A4A4F;"
        "}"
        "QToolBar {"
        "    background-color: #3E3E42;"
        "    border: none;"
        "    spacing: 2px;"
        "    padding: 2px;"
        "}"
        "QToolButton {"
        "    background-color: #3E3E42;"
        "    color: #FFFFFF;"
        "    border: 1px solid #5A5A5C;"
        "    padding: 4px;"
        "    margin: 1px;"
        "}"
        "QToolButton:hover {"
        "    background-color: #4A4A4F;"
        "    border: 1px solid #007ACC;"
        "}"
        "QToolButton:pressed {"
        "    background-color: #0E639C;"
        "}"
        "QToolButton:checked {"
        "    background-color: #007ACC;"
        "}"
        "QDockWidget {"
        "    background-color: #2D2D30;"
        "    color: #FFFFFF;"
        "}"
        "QDockWidget::title {"
        "    background-color: #3E3E42;"
        "    color: #FFFFFF;"
        "    padding: 4px;"
        "    text-align: center;"
        "}"
        "QTabWidget::pane {"
        "    border: 1px solid #5A5A5C;"
        "    background-color: #2D2D30;"
        "}"
        "QTabBar::tab {"
        "    background-color: #3E3E42;"
        "    color: #FFFFFF;"
        "    padding: 6px 12px;"
        "    margin-right: 2px;"
        "}"
        "QTabBar::tab:selected {"
        "    background-color: #007ACC;"
        "}"
        "QTabBar::tab:hover {"
        "    background-color: #4A4A4F;"
        "}"
        "QStatusBar {"
        "    background-color: #3E3E42;"
        "    color: #CCCCCC;"
        "    border-top: 1px solid #5A5A5C;"
        "}"
    );
}

void MainWindow::connectLayerManager()
{
    if (m_layerManager && m_canvas) {
        // Connect layer manager signals to canvas
        connect(m_layerManager, &LayerManager::layerAdded, [this]() {
            // FIXED: Preserve existing layer properties when adding new layers
            if (m_timeline) {
                m_timeline->updateLayersFromCanvas();
            }
            qDebug() << "Layer added, timeline updated";
            });

        connect(m_layerManager, &LayerManager::layerRemoved, [this](int index) {
            if (m_timeline) {
                m_timeline->updateLayersFromCanvas();
            }
            qDebug() << "Layer removed, timeline updated";
            });

        connect(m_layerManager, &LayerManager::currentLayerChanged, [this](int index) {
            m_canvas->setCurrentLayer(index);
            qDebug() << "Current layer changed to:" << index;
            });

        connect(m_layerManager, &LayerManager::layerVisibilityChanged, [this](int index, bool visible) {
            m_canvas->setLayerVisible(index, visible);
            qDebug() << "Layer" << index << "visibility changed to:" << visible;
            });

        connect(m_layerManager, &LayerManager::layerLockChanged, [this](int index, bool locked) {
            m_canvas->setLayerLocked(index, locked);
            qDebug() << "Layer" << index << "locked state changed to:" << locked;
            });

        connect(m_layerManager, &LayerManager::layerOpacityChanged, [this](int index, int opacity) {
            m_canvas->setLayerOpacity(index, opacity / 100.0);
            qDebug() << "Layer" << index << "opacity changed to:" << opacity << "%";
            });


        // Connect canvas signals to layer manager
        connect(m_canvas, &Canvas::layerChanged, m_layerManager, &LayerManager::setCurrentLayer);

        qDebug() << "Layer manager connections established";
    }
}


// File operations
void MainWindow::newFile()
{
    if (!maybeSave())
        return;

    if (m_canvas) {
        m_canvas->clear();
    }

    m_layers.clear();
    m_keyframes.clear();
    m_currentFrame = 1;
    m_totalFrames = 150;
    m_currentFile.clear();
    m_isModified = false;

    // Reset audio state for a completely blank project
    m_audioFile.clear();
    m_audioFrameLength = 0;
    m_audioWaveform = QPixmap();
    if (m_audioPlayer) {
        m_audioPlayer->stop();
        m_audioPlayer->setSource(QUrl());
    }

    if (m_timeline) {
        m_timeline->resetForNewProject();
        m_timeline->setTotalFrames(m_totalFrames);
        m_timeline->updateLayersFromCanvas();

        int defaultLayer = 0;
        if (m_canvas && m_canvas->getLayerCount() > 1) {
            defaultLayer = 1; // Prefer the primary drawing layer when it exists
        }
        m_timeline->setSelectedLayer(defaultLayer);
    }

    addLayer();
    updateUI();
    setWindowTitle("FrameDirector - Untitled");
}

void MainWindow::open()
{
    if (maybeSave()) {
        QString fileName = QFileDialog::getOpenFileName(this,
            "Open Project", "", "FrameDirector Files (*.fdr)");
        if (!fileName.isEmpty()) {
            loadFile(fileName);
        }
    }
}

void MainWindow::save()
{
    if (m_currentFile.isEmpty()) {
        saveAs();
    }
    else {
        saveFile(m_currentFile);
    }
}

void MainWindow::saveAs()
{
    QString fileName = QFileDialog::getSaveFileName(this,
        "Save Project", "", "FrameDirector Files (*.fdr)");
    if (!fileName.isEmpty()) {
        saveFile(fileName);
    }
}

void MainWindow::importImage()
{
    // Get supported image formats
    QStringList formats;
    QList<QByteArray> supportedFormats = QImageReader::supportedImageFormats();
    for (const QByteArray& format : supportedFormats) {
        formats << QString("*.%1").arg(QString::fromLatin1(format).toLower());
    }

    QString filter = QString("Image Files (%1)").arg(formats.join(" "));

    QString fileName = QFileDialog::getOpenFileName(this,
        "Import Image",
        QStandardPaths::writableLocation(QStandardPaths::PicturesLocation),
        filter);

    if (!fileName.isEmpty()) {
        QFileInfo fileInfo(fileName);

        // Load the image
        QPixmap pixmap(fileName);
        if (pixmap.isNull()) {
            QMessageBox::warning(this, "Import Error",
                QString("Could not load image file:\n%1").arg(fileName));
            return;
        }

        // Scale down large images to reasonable size
        const int maxSize = 800;
        if (pixmap.width() > maxSize || pixmap.height() > maxSize) {
            pixmap = pixmap.scaled(maxSize, maxSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        }

        // Create graphics item
        QGraphicsPixmapItem* pixmapItem = new QGraphicsPixmapItem(pixmap);
        pixmapItem->setFlag(QGraphicsItem::ItemIsSelectable, true);
        pixmapItem->setFlag(QGraphicsItem::ItemIsMovable, true);

        // Position at center of canvas
        if (m_canvas) {
            QRectF canvasRect = m_canvas->getCanvasRect();
            QRectF itemRect = pixmapItem->boundingRect();
            QPointF centerPos = canvasRect.center() - itemRect.center();
            pixmapItem->setPos(centerPos);

            // Add to current layer with undo command
            QUndoCommand* command = new AddItemCommand(m_canvas, pixmapItem);
            m_undoStack->push(command);
        }

        m_statusLabel->setText(QString("Image imported: %1").arg(fileInfo.fileName()));
        m_isModified = true;
    }
}

void MainWindow::importLayeredImage()
{
    QString fileName = QFileDialog::getOpenFileName(this,
        "Import Layered Image",
        QStandardPaths::writableLocation(QStandardPaths::PicturesLocation),
        "OpenRaster Files (*.ora)");

    if (fileName.isEmpty())
        return;

    QFileInfo fileInfo(fileName);
    if (fileInfo.suffix().toLower() != "ora") {
        QMessageBox::warning(this, "Import Error",
            "Unsupported layered image format.");
        return;
    }

    QList<LayerData> layers = ORAImporter::importORA(fileName);

    if (layers.isEmpty()) {
        QMessageBox::warning(this, "Import Error",
            "No layers were imported from the file.");
        return;
    }

    if (!m_canvas)
        return;

    int prevLayer = m_canvas->getCurrentLayer();
    int prevFrame = std::max(1, m_canvas->getCurrentFrame());

    for (const LayerData& layer : layers) {
        int idx = m_canvas->addLayer(layer.name, layer.visible, layer.opacity, layer.blendMode);
        m_canvas->setCurrentLayer(idx);
        m_canvas->setCurrentFrame(prevFrame);

        auto animLayer = std::make_unique<AnimationLayer>(layer.name);
        animLayer->setVisible(layer.visible);
        animLayer->setOpacity(layer.opacity);
        animLayer->setBlendMode(layer.blendMode);

        // Ahora iteramos sobre los items de la capa importada
        for (QGraphicsItem* item : layer.items) {
            if (!item) continue;
            item->setFlag(QGraphicsItem::ItemIsSelectable, true);
            item->setFlag(QGraphicsItem::ItemIsMovable, true);
            item->setData(2, static_cast<int>(layer.blendMode));

            QUndoCommand* command = new AddItemCommand(m_canvas, item);
            m_undoStack->push(command);

            animLayer->addItem(item);
        }
        m_layers.push_back(std::move(animLayer));
    }

    m_canvas->setCurrentLayer(prevLayer);
    m_canvas->setCurrentFrame(prevFrame);
    m_currentLayerIndex = prevLayer;

    if (m_layerManager)
        m_layerManager->updateLayers();
    if (m_timeline)
        m_timeline->updateLayersFromCanvas();

    m_statusLabel->setText(QString("Layered image imported: %1").arg(fileInfo.fileName()));
    m_isModified = true;
}

void MainWindow::importVector()
{
    QString fileName = QFileDialog::getOpenFileName(this,
        "Import Vector",
        QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation),
        "Vector Files (*.svg);;All Files (*.*)");

    if (!fileName.isEmpty()) {
        QFileInfo fileInfo(fileName);

        // Check if it's an SVG file
        if (fileInfo.suffix().toLower() != "svg") {
            QMessageBox::warning(this, "Import Error",
                "Only SVG files are currently supported for vector import.");
            return;
        }

        QFile file(fileName);
        if (!file.open(QIODevice::ReadOnly)) {
            QMessageBox::warning(this, "Import Error",
                QString("Could not open SVG file:\n%1\n%2").arg(fileName, file.errorString()));
            return;
        }

        QByteArray svgData = file.readAll();
        file.close();

        if (svgData.isEmpty()) {
            QMessageBox::warning(this, "Import Error",
                QString("SVG file was empty or unreadable:\n%1").arg(fileName));
            return;
        }

        // Test if the SVG can be loaded
        QSvgRenderer renderer(svgData);
        if (!renderer.isValid()) {
            QMessageBox::warning(this, "Import Error",
                QString("Could not load SVG file:\n%1\nThe file may be corrupted or use unsupported features.").arg(fileName));
            return;
        }

        // Create SVG graphics item
        QGraphicsSvgItem* svgItem = createSvgItemFromData(svgData, fileInfo.absoluteFilePath());
        if (!svgItem) {
            QMessageBox::warning(this, "Import Error",
                QString("Failed to create SVG item from file:\n%1").arg(fileName));
            return;
        }
        svgItem->setFlag(QGraphicsItem::ItemIsSelectable, true);
        svgItem->setFlag(QGraphicsItem::ItemIsMovable, true);

        // Scale down large SVGs to reasonable size
        QRectF svgRect = svgItem->boundingRect();
        const qreal maxSize = 400;
        if (svgRect.width() > maxSize || svgRect.height() > maxSize) {
            qreal scaleFactor = qMin(maxSize / svgRect.width(), maxSize / svgRect.height());
            svgItem->setScale(scaleFactor);
        }

        // Position at center of canvas
        if (m_canvas) {
            QRectF canvasRect = m_canvas->getCanvasRect();
            QRectF itemRect = svgItem->boundingRect();
            QPointF centerPos = canvasRect.center() - itemRect.center();
            svgItem->setPos(centerPos);

            // Add to current layer with undo command
            QUndoCommand* command = new AddItemCommand(m_canvas, svgItem);
            m_undoStack->push(command);
        }

        m_statusLabel->setText(QString("SVG imported: %1").arg(fileInfo.fileName()));
        m_isModified = true;
    }
}

void MainWindow::importMultipleFiles()
{
    // Get supported formats
    QStringList imageFormats;
    QList<QByteArray> supportedFormats = QImageReader::supportedImageFormats();
    for (const QByteArray& format : supportedFormats) {
        imageFormats << QString("*.%1").arg(QString::fromLatin1(format).toLower());
    }

    QString imageFilter = QString("Image Files (%1)").arg(imageFormats.join(" "));
    QString svgFilter = "SVG Files (*.svg)";
    QString allFilter = QString("All Supported (%1 *.svg)").arg(imageFormats.join(" "));

    QString filter = QString("%1;;%2;;%3").arg(allFilter, imageFilter, svgFilter);

    QStringList fileNames = QFileDialog::getOpenFileNames(this,
        "Import Multiple Files",
        QStandardPaths::writableLocation(QStandardPaths::PicturesLocation),
        filter);

    if (!fileNames.isEmpty()) {
        QProgressDialog progress("Importing files...", "Cancel", 0, fileNames.size(), this);
        progress.setWindowModality(Qt::WindowModal);
        progress.show();

        int imported = 0;
        int failed = 0;
        QStringList failedFiles;

        for (int i = 0; i < fileNames.size(); ++i) {
            if (progress.wasCanceled()) {
                break;
            }

            progress.setValue(i);
            progress.setLabelText(QString("Importing %1...").arg(QFileInfo(fileNames[i]).fileName()));
            QApplication::processEvents();

            QString fileName = fileNames[i];
            QFileInfo fileInfo(fileName);
            bool success = false;

            if (fileInfo.suffix().toLower() == "svg") {
                // Import SVG
                QFile file(fileName);
                if (!file.open(QIODevice::ReadOnly)) {
                    failedFiles << fileInfo.fileName();
                    ++failed;
                    continue;
                }

                QByteArray svgData = file.readAll();
                file.close();

                if (svgData.isEmpty()) {
                    failedFiles << fileInfo.fileName();
                    ++failed;
                    continue;
                }

                QSvgRenderer renderer(svgData);
                if (!renderer.isValid()) {
                    failedFiles << fileInfo.fileName();
                    ++failed;
                    continue;
                }

                QGraphicsSvgItem* svgItem = createSvgItemFromData(svgData, fileInfo.absoluteFilePath());
                if (!svgItem) {
                    failedFiles << fileInfo.fileName();
                    ++failed;
                    continue;
                }

                svgItem->setFlag(QGraphicsItem::ItemIsSelectable, true);
                svgItem->setFlag(QGraphicsItem::ItemIsMovable, true);

                // Position items in a grid
                int gridX = (imported % 5) * 150;
                int gridY = (imported / 5) * 150;
                svgItem->setPos(gridX, gridY);

                if (m_canvas) {
                    QUndoCommand* command = new AddItemCommand(m_canvas, svgItem);
                    m_undoStack->push(command);
                    success = true;
                }
            }
            else {
                // Import image
                QPixmap pixmap(fileName);
                if (!pixmap.isNull()) {
                    // Scale down large images
                    const int maxSize = 200;
                    if (pixmap.width() > maxSize || pixmap.height() > maxSize) {
                        pixmap = pixmap.scaled(maxSize, maxSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
                    }

                    QGraphicsPixmapItem* pixmapItem = new QGraphicsPixmapItem(pixmap);
                    pixmapItem->setFlag(QGraphicsItem::ItemIsSelectable, true);
                    pixmapItem->setFlag(QGraphicsItem::ItemIsMovable, true);

                    // Position items in a grid
                    int gridX = (imported % 5) * 150;
                    int gridY = (imported / 5) * 150;
                    pixmapItem->setPos(gridX, gridY);

                    if (m_canvas) {
                        QUndoCommand* command = new AddItemCommand(m_canvas, pixmapItem);
                        m_undoStack->push(command);
                        success = true;
                    }
                }
            }

            if (success) {
                imported++;
            }
            else {
                failed++;
                failedFiles << fileInfo.fileName();
            }
        }

        progress.close();

        QString message = QString("Import complete:\n%1 files imported successfully").arg(imported);
        if (failed > 0) {
            message += QString("\n%1 files failed to import").arg(failed);
            if (failedFiles.size() <= 5) {
                message += QString(":\n%1").arg(failedFiles.join("\n"));
            }
        }

        QMessageBox::information(this, "Import Results", message);

        if (imported > 0) {
            m_statusLabel->setText(QString("%1 files imported").arg(imported));
            m_isModified = true;
        }
    }
}

void MainWindow::showSupportedFormats()
{
    QStringList imageFormats;
    QList<QByteArray> supportedFormats = QImageReader::supportedImageFormats();
    for (const QByteArray& format : supportedFormats) {
        imageFormats << QString::fromLatin1(format).toUpper();
    }

    QString message = "Supported Import Formats:\n\n";
    message += "Images: " + imageFormats.join(", ") + "\n";
    message += "Vectors: SVG\n\n";
    message += "Note: Large images are automatically scaled down for performance.";

    QMessageBox::information(this, "Supported Formats", message);
}

void MainWindow::importAudio()
{
    QString fileName = QFileDialog::getOpenFileName(this,
        "Import Audio", "",
        "Audio Files (*.wav *.mp3 *.aac *.ogg *.flac);;All Files (*.*)");

    if (!fileName.isEmpty()) {
        m_audioPlayer->setSource(QUrl::fromLocalFile(fileName));
        m_audioFile = fileName;
        m_statusLabel->setText(QString("Audio imported: %1").arg(QFileInfo(fileName).fileName()));

        // If duration already available, update timeline immediately
        if (m_audioPlayer->duration() > 0) {
            onAudioDurationChanged(m_audioPlayer->duration());
        }
    }
}

QString MainWindow::getAudioFile() const
{
    return m_audioFile;
}

void MainWindow::onAudioDurationChanged(qint64 duration)
{
    if (duration <= 0)
        return;

    m_audioFrameLength = static_cast<int>((duration / 1000.0) * m_frameRate);
    if (m_timeline) {
        m_audioWaveform = createAudioWaveform(m_audioFile, m_audioFrameLength);
        m_timeline->setAudioTrack(m_audioFrameLength, m_audioWaveform, QFileInfo(m_audioFile).fileName());
    }
}

void MainWindow::onTotalFramesChanged(int frames)
{
    m_totalFrames = frames;
    if (m_frameLabel)
        m_frameLabel->setText(QString("Frame: %1 / %2").arg(m_currentFrame).arg(m_totalFrames));
}

QPixmap MainWindow::createAudioWaveform(const QString& fileName, int samples, int height)
{
    if (fileName.isEmpty() || samples <= 0)
        return QPixmap();

    QAudioDecoder decoder;
    decoder.setSource(QUrl::fromLocalFile(fileName));

    QVector<double> pcm;
    pcm.reserve(samples);
    bool decodeError = false;

    QObject::connect(&decoder, &QAudioDecoder::bufferReady, [&]() {
        const QAudioBuffer buffer = decoder.read();
        const QAudioFormat format = buffer.format();
        if (!format.isValid())
            return;

        const int channelCount = qMax(1, format.channelCount());
        const int frameCount = buffer.frameCount();
        const int sampleCount = frameCount * channelCount;

        auto appendSamples = [&](auto dataPtr, double scale) {
            for (int i = 0; i < sampleCount; ++i)
                pcm.append(qBound(-1.0, static_cast<double>(dataPtr[i]) * scale, 1.0));
        };

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
        switch (format.sampleFormat()) {
        case QAudioFormat::Int16:
            appendSamples(buffer.constData<qint16>(), 1.0 / 32768.0);
            break;
        case QAudioFormat::UInt8: {
            const quint8* data = buffer.constData<quint8>();
            for (int i = 0; i < sampleCount; ++i)
                pcm.append(qBound(-1.0, (static_cast<double>(data[i]) - 128.0) / 128.0, 1.0));
            break;
        }
        case QAudioFormat::Int32:
            appendSamples(buffer.constData<qint32>(), 1.0 / 2147483648.0);
            break;
        case QAudioFormat::Float:
            appendSamples(buffer.constData<float>(), 1.0);
            break;
        default:
            break;
        }
#else
        switch (format.sampleType()) {
        case QAudioFormat::SignedInt:
            if (format.sampleSize() == 16)
                appendSamples(buffer.constData<qint16>(), 1.0 / 32768.0);
            else if (format.sampleSize() == 32)
                appendSamples(buffer.constData<qint32>(), 1.0 / 2147483648.0);
            break;
        case QAudioFormat::UnSignedInt:
            if (format.sampleSize() == 8) {
                const quint8* data = buffer.constData<quint8>();
                for (int i = 0; i < sampleCount; ++i)
                    pcm.append(qBound(-1.0, (static_cast<double>(data[i]) - 128.0) / 128.0, 1.0));
            }
            break;
        case QAudioFormat::Float:
            appendSamples(buffer.constData<float>(), 1.0);
            break;
        default:
            break;
        }
#endif
    });

    QEventLoop loop;
    QObject::connect(&decoder, &QAudioDecoder::finished, &loop, &QEventLoop::quit);
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    // Qt6: Usa QAudioDecoder::error(QAudioDecoder::Error)
    QObject::connect(&decoder, QOverload<QAudioDecoder::Error>::of(&QAudioDecoder::error), &loop, [&](QAudioDecoder::Error error) {
        if (error != QAudioDecoder::NoError) {
            decodeError = true;
            loop.quit();
        }
        });
#else
    // Qt5: Usa QAudioDecoder::error(QAudioDecoder::Error)
    QObject::connect(&decoder, QOverload<QAudioDecoder::Error>::of(&QAudioDecoder::error), &loop, [&](QAudioDecoder::Error error) {
        if (error != QAudioDecoder::NoError) {
            decodeError = true;
            loop.quit();
        }
        });
#endif

    decoder.start();
    loop.exec();

    if (decodeError || pcm.isEmpty())
        return QPixmap();

    QImage image(samples, height, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    painter.setPen(QPen(QColor(60, 110, 60)));

    const int totalSamples = pcm.size();
    const int step = qMax(1, totalSamples / samples);
    const double halfHeight = height / 2.0;

    for (int x = 0; x < samples; ++x) {
        const int start = x * step;
        const int end = qMin(start + step, totalSamples);
        double minVal = 0.0;
        double maxVal = 0.0;
        for (int i = start; i < end; ++i) {
            const double val = pcm[i];
            if (val < minVal)
                minVal = val;
            if (val > maxVal)
                maxVal = val;
        }

        const int centerY = height / 2;
        const int y1 = qBound(0, static_cast<int>(centerY - maxVal * halfHeight), height - 1);
        const int y2 = qBound(0, static_cast<int>(centerY - minVal * halfHeight), height - 1);
        painter.drawLine(x, y1, x, y2);
    }

    painter.end();
    return QPixmap::fromImage(image);
}

void MainWindow::exportAnimation()
{
	// Show export options dialog.
    ExportDialog options(this);
    if (options.exec() != QDialog::Accepted)
        return;

    QString format = options.getFormat();
    QString filter = (format == "gif") ? "GIF Files (*.gif)" : "Video Files (*.mp4)";
    QString fileName = QFileDialog::getSaveFileName(this,
        "Export Animation", "", filter);
    if (fileName.isEmpty())
        return;

    if (!fileName.endsWith('.' + format, Qt::CaseInsensitive))
        fileName += '.' + format;

    int originalFrame = m_timeline ? m_timeline->getCurrentFrame() : 1;
    if (m_timeline)
        m_timeline->setCurrentFrame(1);

    AnimationController controller(this);
    int totalFrames = m_timeline ? m_timeline->getTotalFrames() : 0;
    if (m_timeline)
        controller.setTotalFrames(totalFrames);

    // Respect the project's FPS (timeline if available, otherwise MainWindow setting)
    int exportFps = (m_timeline ? m_timeline->getFrameRate() : m_frameRate);
    controller.setFrameRate(exportFps);

    connect(&controller, &AnimationController::exportProgress, &options, &ExportDialog::updateProgress);
    options.show();
    QApplication::processEvents();

    bool ok = controller.exportAnimation(fileName, format, options.getQuality(), options.getLoop());
    options.close();
    m_statusLabel->setText(ok ? "Animation exported" : "Export failed");

    if (m_timeline)
        m_timeline->setCurrentFrame(originalFrame);
}

void MainWindow::exportFrame()
{
    QString fileName = QFileDialog::getSaveFileName(this,
        "Export Frame", "", "Image Files (*.png *.jpg *.jpeg *.svg)");
    if (fileName.isEmpty())
        return;

    // Ensure the filename has an extension; default to PNG if none provided
    QFileInfo info(fileName);
    if (info.suffix().isEmpty())
        fileName += ".png";

    // Determine the frame to export (timeline is 1-based)
    int frame = m_timeline ? m_timeline->getCurrentFrame() : 1;

    AnimationController controller(this);
    if (m_timeline)
        controller.setTotalFrames(m_timeline->getTotalFrames());

    controller.exportFrame(frame, fileName);
    m_statusLabel->setText("Frame exported");
}

void MainWindow::exportSVG()
{
    QString fileName = QFileDialog::getSaveFileName(this,
        "Export SVG", "", "SVG Files (*.svg)");
    if (!fileName.isEmpty()) {
        // Implementation for exporting as SVG
        m_statusLabel->setText("SVG exported");
    }
}

// Edit operations
void MainWindow::undo()
{
    m_undoStack->undo();
}

void MainWindow::redo()
{
    m_undoStack->redo();
}

void MainWindow::cut()
{
    if (m_canvas && m_canvas->hasSelection()) {
        copy();
        m_canvas->deleteSelected();
        m_statusLabel->setText("Items cut to clipboard");
    }
}
void MainWindow::copy()
{
    if (!m_canvas || !m_canvas->scene()) return;

    QList<QGraphicsItem*> selectedItems = m_canvas->scene()->selectedItems();
    if (selectedItems.isEmpty()) return;

    // Clear previous clipboard
    m_clipboardItems.clear();

    // Find the center point of selected items for offset calculation
    QRectF boundingRect;
    for (QGraphicsItem* item : selectedItems) {
        boundingRect = boundingRect.united(item->sceneBoundingRect());
    }
    m_clipboardOffset = boundingRect.center();

    // Create copies of selected items
    for (QGraphicsItem* item : selectedItems) {
        QGraphicsItem* copy = nullptr;

        // Create copies based on item type with FULL property preservation
        if (auto rectItem = qgraphicsitem_cast<QGraphicsRectItem*>(item)) {
            auto newRect = new QGraphicsRectItem(rectItem->rect());
            newRect->setPen(rectItem->pen()); // This preserves stroke width
            newRect->setBrush(rectItem->brush());
            newRect->setTransform(rectItem->transform());
            newRect->setPos(rectItem->pos());
            copy = newRect;
        }
        else if (auto ellipseItem = qgraphicsitem_cast<QGraphicsEllipseItem*>(item)) {
            auto newEllipse = new QGraphicsEllipseItem(ellipseItem->rect());
            newEllipse->setPen(ellipseItem->pen()); // This preserves stroke width
            newEllipse->setBrush(ellipseItem->brush());
            newEllipse->setTransform(ellipseItem->transform());
            newEllipse->setPos(ellipseItem->pos());
            copy = newEllipse;
        }
        else if (auto lineItem = qgraphicsitem_cast<QGraphicsLineItem*>(item)) {
            auto newLine = new QGraphicsLineItem(lineItem->line());
            newLine->setPen(lineItem->pen()); // This preserves stroke width
            newLine->setTransform(lineItem->transform());
            newLine->setPos(lineItem->pos());
            copy = newLine;
        }
        else if (auto pathItem = qgraphicsitem_cast<QGraphicsPathItem*>(item)) {
            auto newPath = new QGraphicsPathItem(pathItem->path());
            // FIX: Explicitly preserve pen properties for path items (drawing tool output)
            QPen originalPen = pathItem->pen();
            newPath->setPen(originalPen); // This should preserve stroke width
            newPath->setBrush(pathItem->brush());
            newPath->setTransform(pathItem->transform());
            newPath->setPos(pathItem->pos());
            copy = newPath;
        }
        else if (auto textItem = qgraphicsitem_cast<QGraphicsTextItem*>(item)) {
            auto newText = new QGraphicsTextItem(textItem->toPlainText());
            newText->setFont(textItem->font());
            newText->setDefaultTextColor(textItem->defaultTextColor());
            newText->setTransform(textItem->transform());
            newText->setPos(textItem->pos());
            copy = newText;
        }
        else if (auto pixmapItem = qgraphicsitem_cast<QGraphicsPixmapItem*>(item)) {
            auto newPixmap = new QGraphicsPixmapItem(pixmapItem->pixmap());
            newPixmap->setOffset(pixmapItem->offset());
            newPixmap->setTransformationMode(pixmapItem->transformationMode());
            newPixmap->setTransform(pixmapItem->transform());
            newPixmap->setPos(pixmapItem->pos());
            copy = newPixmap;
        }
        else if (auto svgItem = qgraphicsitem_cast<QGraphicsSvgItem*>(item)) {
            auto newSvg = deepCopySvgItem(svgItem);
            if (newSvg) {
                newSvg->setTransform(svgItem->transform());
                newSvg->setPos(svgItem->pos());
                copy = newSvg;
            }
        }

        if (copy) {
            copy->setFlags(item->flags());
            copy->setZValue(item->zValue());
            // FIX: Preserve opacity and other properties
            copy->setOpacity(item->opacity());
            copy->setVisible(item->isVisible());
            m_clipboardItems.append(copy);
        }
    }

    m_pasteAction->setEnabled(!m_clipboardItems.isEmpty());
    m_statusLabel->setText(QString("Copied %1 items to clipboard").arg(m_clipboardItems.size()));
}

void MainWindow::paste()
{
    if (m_clipboardItems.isEmpty() || !m_canvas) return;

    QList<QGraphicsItem*> pastedItems;

    // Calculate paste offset (slightly offset from original position)
    QPointF pasteOffset(20, 20);

    m_undoStack->beginMacro("Paste Items");

    for (QGraphicsItem* clipboardItem : m_clipboardItems) {
        QGraphicsItem* pastedItem = nullptr;

        // Create new copies of clipboard items with FULL property preservation
        if (auto rectItem = qgraphicsitem_cast<QGraphicsRectItem*>(clipboardItem)) {
            auto newRect = new QGraphicsRectItem(rectItem->rect());
            newRect->setPen(rectItem->pen()); // Preserves stroke width
            newRect->setBrush(rectItem->brush());
            newRect->setTransform(rectItem->transform());
            pastedItem = newRect;
        }
        else if (auto ellipseItem = qgraphicsitem_cast<QGraphicsEllipseItem*>(clipboardItem)) {
            auto newEllipse = new QGraphicsEllipseItem(ellipseItem->rect());
            newEllipse->setPen(ellipseItem->pen()); // Preserves stroke width
            newEllipse->setBrush(ellipseItem->brush());
            newEllipse->setTransform(ellipseItem->transform());
            pastedItem = newEllipse;
        }
        else if (auto lineItem = qgraphicsitem_cast<QGraphicsLineItem*>(clipboardItem)) {
            auto newLine = new QGraphicsLineItem(lineItem->line());
            newLine->setPen(lineItem->pen()); // Preserves stroke width
            newLine->setTransform(lineItem->transform());
            pastedItem = newLine;
        }
        else if (auto pathItem = qgraphicsitem_cast<QGraphicsPathItem*>(clipboardItem)) {
            auto newPath = new QGraphicsPathItem(pathItem->path());
            // FIX: Explicitly copy pen to preserve stroke width for drawing tool items
            QPen originalPen = pathItem->pen();
            newPath->setPen(originalPen); // This preserves stroke width
            newPath->setBrush(pathItem->brush());
            newPath->setTransform(pathItem->transform());
            pastedItem = newPath;
        }
        else if (auto textItem = qgraphicsitem_cast<QGraphicsTextItem*>(clipboardItem)) {
            auto newText = new QGraphicsTextItem(textItem->toPlainText());
            newText->setFont(textItem->font());
            newText->setDefaultTextColor(textItem->defaultTextColor());
            newText->setTransform(textItem->transform());
            pastedItem = newText;
        }
        else if (auto pixmapItem = qgraphicsitem_cast<QGraphicsPixmapItem*>(clipboardItem)) {
            auto newPixmap = new QGraphicsPixmapItem(pixmapItem->pixmap());
            newPixmap->setOffset(pixmapItem->offset());
            newPixmap->setTransformationMode(pixmapItem->transformationMode());
            newPixmap->setTransform(pixmapItem->transform());
            pastedItem = newPixmap;
        }
        else if (auto svgItem = qgraphicsitem_cast<QGraphicsSvgItem*>(clipboardItem)) {
            auto newSvg = deepCopySvgItem(svgItem);
            if (newSvg) {
                newSvg->setTransform(svgItem->transform());
                pastedItem = newSvg;
            }
        }

        if (pastedItem) {
            pastedItem->setPos(clipboardItem->pos() + pasteOffset);
            pastedItem->setFlags(clipboardItem->flags());
            pastedItem->setZValue(clipboardItem->zValue());
            // FIX: Preserve opacity and visibility
            pastedItem->setOpacity(clipboardItem->opacity());
            pastedItem->setVisible(clipboardItem->isVisible());

            AddItemCommand* addCommand = new AddItemCommand(m_canvas, pastedItem);
            m_undoStack->push(addCommand);

            pastedItems.append(pastedItem);
        }
    }

    m_undoStack->endMacro();

    // Select the pasted items
    m_canvas->scene()->clearSelection();
    for (QGraphicsItem* item : pastedItems) {
        item->setSelected(true);
    }

    m_statusLabel->setText(QString("Pasted %1 items").arg(pastedItems.size()));
    m_isModified = true;
}

void MainWindow::selectAll()
{
    if (m_canvas) {
        m_canvas->selectAll();
    }
}

void MainWindow::group()
{
    if (m_canvas) {
        m_canvas->groupSelectedItems();
    }
}

void MainWindow::ungroup()
{
    if (m_canvas) {
        m_canvas->ungroupSelectedItems();
    }
}

// View operations
void MainWindow::zoomIn()
{
    if (m_canvas) {
        m_canvas->zoomIn();
    }
}

void MainWindow::zoomOut()
{
    if (m_canvas) {
        m_canvas->zoomOut();
    }
}

void MainWindow::zoomToFit()
{
    if (m_canvas) {
        m_canvas->zoomToFit();
    }
}

void MainWindow::toggleGrid()
{
    if (m_canvas) {
        m_canvas->setGridVisible(m_toggleGridAction->isChecked());
    }
}

void MainWindow::toggleSnapToGrid()
{
    if (m_canvas) {
        m_canvas->setSnapToGrid(m_toggleSnapAction->isChecked());
    }
}

void MainWindow::toggleRulers()
{
    if (m_canvas) {
        m_canvas->setRulersVisible(m_toggleRulersAction->isChecked());
    }
}

// Animation operations
void MainWindow::play()
{
    if (!m_isPlaying) {
        m_isPlaying = true;
        m_playbackTimer->start();
        m_playAction->setText("Pause");
        m_statusLabel->setText("Playing");
        emit playbackStateChanged(true); // Add this line

        if (!m_audioFile.isEmpty()) {
            qint64 pos = static_cast<qint64>((m_currentFrame - 1) * 1000.0 / m_frameRate);
            m_audioPlayer->setPosition(pos);
            m_audioPlayer->play();
        }
    }
    else {
        stop();
    }
}


void MainWindow::stop()
{
    if (m_isPlaying) {
        m_isPlaying = false;
        m_playbackTimer->stop();
        m_playAction->setText("Play");
        m_statusLabel->setText("Stopped");
        emit playbackStateChanged(false); // Add this line
        if (m_audioPlayer)
            m_audioPlayer->stop();
    }
}

void MainWindow::nextFrame()
{
    if (m_currentFrame < m_totalFrames) {
        onFrameChanged(m_currentFrame + 1);
    }
}

void MainWindow::previousFrame()
{
    if (m_currentFrame > 1) {
        onFrameChanged(m_currentFrame - 1);
    }
}

void MainWindow::nextKeyframe()
{
    if (m_canvas) {
        int nextKeyframe = m_canvas->getNextKeyframeAfter(m_currentFrame);
        if (nextKeyframe != -1) {
            onFrameChanged(nextKeyframe);
            m_statusLabel->setText(QString("Jumped to keyframe at frame %1").arg(nextKeyframe));
        }
        else {
            m_statusLabel->setText("No keyframes after current frame");
        }
    }
}

void MainWindow::previousKeyframe()
{
    if (m_canvas) {
        int prevKeyframe = m_canvas->getLastKeyframeBefore(m_currentFrame);
        if (prevKeyframe != -1) {
            onFrameChanged(prevKeyframe);
            m_statusLabel->setText(QString("Jumped to keyframe at frame %1").arg(prevKeyframe));
        }
        else {
            m_statusLabel->setText("No keyframes before current frame");
        }
    }
}

void MainWindow::firstFrame()
{
    onFrameChanged(1);
    if (m_isPlaying && !m_audioFile.isEmpty())
        m_audioPlayer->setPosition(0);
}

void MainWindow::lastFrame()
{
    onFrameChanged(m_totalFrames);
}


void MainWindow::addKeyframe()
{
    if (m_canvas) {
        int layer = m_canvas->getCurrentLayer();
        if (m_undoStack) {
            m_undoStack->push(new AddKeyframeCommand(m_canvas, layer, m_currentFrame));
        }
        else {
            m_canvas->createKeyframe(m_currentFrame);
        }

        if (m_timeline) {
            m_timeline->updateLayersFromCanvas();
        }

        updateFrameActions();
        showFrameTypeIndicator();
        m_statusLabel->setText(QString("Keyframe created at frame %1").arg(m_currentFrame));
        m_isModified = true;
    }
}


void MainWindow::insertFrame()
{
    if (m_canvas) {
        m_canvas->createExtendedFrame(m_currentFrame);

        if (m_timeline) {
            m_timeline->updateLayersFromCanvas();
        }

        updateFrameActions();
        showFrameTypeIndicator();
        m_statusLabel->setText(QString("Frame inserted at frame %1").arg(m_currentFrame));
        m_isModified = true;
    }
}


void MainWindow::insertBlankKeyframe()
{
    if (m_canvas) {
        m_canvas->createBlankKeyframe(m_currentFrame);

        if (m_timeline) {
            m_timeline->updateLayersFromCanvas();
        }

        updateFrameActions();
        showFrameTypeIndicator();
        m_statusLabel->setText(QString("Blank keyframe inserted at frame %1").arg(m_currentFrame));
        m_isModified = true;
    }
}


void MainWindow::clearCurrentFrame()
{
    if (m_canvas) {
        m_canvas->clearCurrentFrameContent();
        updateFrameActions();
        showFrameTypeIndicator();
        m_statusLabel->setText(QString("Frame %1 cleared").arg(m_currentFrame));
        m_isModified = true;
    }
}

void MainWindow::convertToKeyframe()
{
    if (m_canvas && m_canvas->getFrameType(m_currentFrame) == FrameType::ExtendedFrame) {
        // Convert extended frame to keyframe by creating keyframe with current content
        m_canvas->createKeyframe(m_currentFrame);

        if (m_timeline) {
            m_timeline->updateLayersFromCanvas();
        }

        updateFrameActions();
        showFrameTypeIndicator();
        m_statusLabel->setText(QString("Frame %1 converted to keyframe").arg(m_currentFrame));
        m_isModified = true;
    }
}


void MainWindow::updateFrameActions()
{
    if (!m_canvas) return;

    FrameType currentFrameType = m_canvas->getFrameType(m_currentFrame);
    bool hasContent = m_canvas->hasContent(m_currentFrame);
    bool isKeyframe = m_canvas->hasKeyframe(m_currentFrame);
    bool isFrameTweened = m_canvas->isFrameTweened(m_currentFrame);

    // Enable/disable actions based on frame state
    m_convertToKeyframeAction->setEnabled(currentFrameType == FrameType::ExtendedFrame && !isFrameTweened);

    // ENHANCED: Disable clear frame action on extended frames OR tweened frames
    m_clearFrameAction->setEnabled(hasContent && currentFrameType != FrameType::ExtendedFrame && !isFrameTweened);

    // Update navigation actions
    m_nextKeyframeAction->setEnabled(m_canvas->getNextKeyframeAfter(m_currentFrame) != -1);
    m_prevKeyframeAction->setEnabled(m_canvas->getLastKeyframeBefore(m_currentFrame) != -1);

    // Update tool availability based on tweening state
    updateToolAvailability();
}

void MainWindow::updateToolAvailability()
{
    if (!m_canvas) return;

    bool canDraw = m_canvas->canDrawOnCurrentFrame();

    // Disable drawing tools on tweened frames
    if (m_toolsPanel) {
        m_toolsPanel->setToolsEnabled(canDraw);
    }

    // Update tool actions - add null checks
    if (m_drawToolAction) m_drawToolAction->setEnabled(canDraw);
    if (m_lineToolAction) m_lineToolAction->setEnabled(canDraw);
    if (m_rectangleToolAction) m_rectangleToolAction->setEnabled(canDraw);
    if (m_ellipseToolAction) m_ellipseToolAction->setEnabled(canDraw);
    if (m_textToolAction) m_textToolAction->setEnabled(canDraw);
    if (m_bucketFillToolAction) m_bucketFillToolAction->setEnabled(canDraw);
    if (m_eraseToolAction) m_eraseToolAction->setEnabled(canDraw);

    // If current tool is disabled, switch to select tool
    if (!canDraw && m_currentTool != SelectTool) {
        setTool(SelectTool);
    }

    // Update status message - add null check
    if (!canDraw && m_canvas->isFrameTweened(m_currentFrame) && m_statusLabel) {
        m_statusLabel->setText("Drawing disabled on tweened frame. Right-click to remove tweening.");
    }
}

void MainWindow::showFrameTypeIndicator()
{
    if (!m_canvas) return;

    FrameType frameType = m_canvas->getFrameType(m_currentFrame);
    bool isFrameTweened = m_canvas->isFrameTweened(m_currentFrame);
    QString typeText;

    switch (frameType) {
    case FrameType::Empty:
        typeText = "Empty Frame";
        break;
    case FrameType::Keyframe:
        if (isFrameTweened) {
            int endFrame = m_canvas->getTweeningEndFrame(m_currentFrame);
            if (endFrame > 0) {
                typeText = QString("Tweened Keyframe (to %1)").arg(endFrame);
            }
            else {
                typeText = "Tweened Keyframe";
            }
        }
        else {
            typeText = "Keyframe";
        }
        break;
    case FrameType::ExtendedFrame:
        int sourceKeyframe = m_canvas->getSourceKeyframe(m_currentFrame);
        if (isFrameTweened) {
            typeText = QString("Tweened Frame (from %1)").arg(sourceKeyframe);
        }
        else {
            typeText = QString("Extended Frame (from %1)").arg(sourceKeyframe);
        }
        break;
    }

    // Update status label with frame type info
    QString statusText = QString("Frame: %1 (%2)").arg(m_currentFrame).arg(typeText);
    m_frameLabel->setText(statusText);
}

// NEW: Apply tweening between frames
void MainWindow::applyTweening()
{
    if (!m_canvas) return;

    // Get current frame as start frame
    int startFrame = m_currentFrame;

    // Find next keyframe as end frame
    int endFrame = m_canvas->getNextKeyframeAfter(startFrame);

    if (endFrame == -1) {
        m_statusLabel->setText("No next keyframe found for tweening");
        return;
    }

    // Apply tweening with default linear easing
    m_canvas->applyTweening(startFrame, endFrame, "linear");

    // Update UI
    updateFrameActions();
    showFrameTypeIndicator();

    if (m_timeline) {
        m_timeline->updateLayersFromCanvas();
    }

    m_statusLabel->setText(QString("Tweening applied from frame %1 to %2").arg(startFrame).arg(endFrame));
    m_isModified = true;
}

// NEW: Remove tweening from current frame
void MainWindow::removeTweening()
{
    if (!m_canvas) return;

    int currentFrame = m_currentFrame;

    // Find the start frame of the tweening sequence
    int startFrame = currentFrame;

    // If current frame is an extended tweened frame, find the source keyframe
    if (m_canvas->getFrameType(currentFrame) == FrameType::ExtendedFrame &&
        m_canvas->isFrameTweened(currentFrame)) {
        startFrame = m_canvas->getSourceKeyframe(currentFrame);
    }

    // Remove tweening
    m_canvas->removeTweening(startFrame);

    // Update UI
    updateFrameActions();
    showFrameTypeIndicator();

    if (m_timeline) {
        m_timeline->updateLayersFromCanvas();
    }

    m_statusLabel->setText(QString("Tweening removed from frame %1").arg(startFrame));
    m_isModified = true;
}

// NEW: Create actions for tweening
void MainWindow::createTweeningActions()
{
    m_applyTweeningAction = new QAction("Apply &Tweening", this);
    m_applyTweeningAction->setStatusTip("Apply tweening to frame span");
    connect(m_applyTweeningAction, &QAction::triggered, this, &MainWindow::applyTweening);

    m_removeTweeningAction = new QAction("Remove &Tweening", this);
    m_removeTweeningAction->setStatusTip("Remove tweening from frame span");
    connect(m_removeTweeningAction, &QAction::triggered, this, &MainWindow::removeTweening);
}

// ENHANCED: Modified onFrameChanged to update tool availability
void MainWindow::onFrameChanged(int frame)
{
    m_currentFrame = frame;

    if (m_canvas) {
        m_canvas->setCurrentFrame(frame);
    }

    // Update frame slider and spinbox
    if (m_timeline) {
        // Update timeline UI
        m_timeline->setCurrentFrame(frame);
    }

    // Update frame actions and tool availability
    updateFrameActions();
    showFrameTypeIndicator();

    // Update frame display
    QString frameText = QString("Frame: %1").arg(frame);
    m_frameLabel->setText(frameText);
}

// NEW: Connect canvas tweening signals
void MainWindow::connectTweeningSignals()
{
    if (m_canvas) {
        connect(m_canvas, &Canvas::tweeningApplied,
            this, [this](int startFrame, int endFrame) {
                m_statusLabel->setText(QString("Tweening applied: frames %1-%2").arg(startFrame).arg(endFrame));
            });

        connect(m_canvas, &Canvas::tweeningRemoved,
            this, [this](int frame) {
                m_statusLabel->setText(QString("Tweening removed from frame %1").arg(frame));
            });
    }
}

void MainWindow::copyCurrentFrame()
{
    if (!m_canvas) {
        m_statusLabel->setText("No canvas available");
        return;
    }

    // Clear previous clipboard data
    m_frameClipboard.clear();

    // Get current frame type and check if it has content
    FrameType frameType = m_canvas->getFrameType(m_currentFrame);
    if (frameType == FrameType::Empty) {
        m_statusLabel->setText("Current frame is empty - nothing to copy");
        return;
    }

    // Get items from current frame
    QList<QGraphicsItem*> currentFrameItems;

    // If it's an extended frame, get items from source keyframe
    if (frameType == FrameType::ExtendedFrame) {
        int sourceFrame = m_canvas->getSourceKeyframe(m_currentFrame);
        if (sourceFrame != -1) {
            currentFrameItems = m_canvas->getFrameItems(sourceFrame);
        }
    }
    else {
        // Get items directly from current frame
        currentFrameItems = m_canvas->getFrameItems(m_currentFrame);
    }

    if (currentFrameItems.isEmpty()) {
        m_statusLabel->setText("Current frame has no content to copy");
        return;
    }

    // Deep copy all items to clipboard
    for (QGraphicsItem* item : currentFrameItems) {
        if (!item || item == m_canvas->getBackgroundRect()) continue;

        QGraphicsItem* copiedItem = duplicateGraphicsItem(item);
        if (copiedItem) {
            m_frameClipboard.items.append(copiedItem);

            // Record the original layer of this item
            int layerIndex = m_canvas->getItemLayerIndex(item);
            m_frameClipboard.itemLayers[copiedItem] = layerIndex;

            // Store item state for positioning and properties
            QVariantMap state;
            state["position"] = item->pos();
            state["rotation"] = item->rotation();
            state["scale"] = item->scale();
            state["opacity"] = item->opacity();
            state["zValue"] = item->zValue();
            state["visible"] = item->isVisible();

            m_frameClipboard.itemStates[copiedItem] = state;
        }
    }

    m_frameClipboard.frameType = frameType;
    m_frameClipboard.hasData = !m_frameClipboard.items.isEmpty();

    // Enable paste action
    m_pasteFrameAction->setEnabled(m_frameClipboard.hasData);

    m_statusLabel->setText(QString("Copied frame %1 content (%2 items) to clipboard")
        .arg(m_currentFrame)
        .arg(m_frameClipboard.items.size()));

    qDebug() << "Frame" << m_currentFrame << "copied to clipboard with"
        << m_frameClipboard.items.size() << "items";
}

void MainWindow::pasteFrame()
{
    if (!m_canvas) {
        m_statusLabel->setText("No canvas available");
        return;
    }

    if (!m_frameClipboard.hasData || m_frameClipboard.items.isEmpty()) {
        m_statusLabel->setText("No frame data in clipboard to paste");
        return;
    }

    // Determine unique layers in clipboard
    QSet<int> targetLayers;
    for (QGraphicsItem* item : m_frameClipboard.items) {
        int layerIdx = m_frameClipboard.itemLayers.value(item, m_canvas->getCurrentLayer());
        targetLayers.insert(layerIdx);
    }

    int originalLayer = m_canvas->getCurrentLayer();

    // Clear content and create keyframe for each target layer
    for (int layerIdx : targetLayers) {
        m_canvas->setCurrentLayer(layerIdx);
        m_canvas->clearCurrentFrameContent();
        m_canvas->createKeyframe(m_currentFrame);
    }

    m_canvas->setCurrentLayer(originalLayer);

    m_undoStack->beginMacro("Paste Frame");

    QList<QGraphicsItem*> pastedItems;

    // Paste all items from clipboard into their original layers
    for (QGraphicsItem* clipboardItem : m_frameClipboard.items) {
        if (!clipboardItem) continue;

        int targetLayer = m_frameClipboard.itemLayers.value(clipboardItem, originalLayer);
        m_canvas->setCurrentLayer(targetLayer);

        // Create a new copy of the clipboard item
        QGraphicsItem* pastedItem = duplicateGraphicsItem(clipboardItem);
        if (!pastedItem) continue;

        // Restore item state
        if (m_frameClipboard.itemStates.contains(clipboardItem)) {
            QVariantMap state = m_frameClipboard.itemStates[clipboardItem];
            pastedItem->setPos(state["position"].toPointF());
            pastedItem->setRotation(state["rotation"].toDouble());
            pastedItem->setScale(state["scale"].toDouble());
            pastedItem->setOpacity(state["opacity"].toDouble());
            pastedItem->setZValue(state["zValue"].toDouble());
            pastedItem->setVisible(state["visible"].toBool());
        }

        // Add to scene and canvas
        m_canvas->scene()->addItem(pastedItem);
        m_canvas->addItemToCurrentLayer(pastedItem);
        pastedItems.append(pastedItem);
    }

    // Restore original layer
    m_canvas->setCurrentLayer(originalLayer);

    m_undoStack->endMacro();

    // Update UI
    if (m_timeline) {
        m_timeline->updateLayersFromCanvas();
    }

    updateFrameActions();
    showFrameTypeIndicator();

    m_statusLabel->setText(QString("Pasted %1 items to frame %2")
        .arg(pastedItems.size())
        .arg(m_currentFrame));

    m_isModified = true;

    qDebug() << "Pasted" << pastedItems.size() << "items to frame" << m_currentFrame;
}

// 6. Add helper method for deep copying graphics items:

QGraphicsItem* MainWindow::duplicateGraphicsItem(QGraphicsItem* item)
{
    if (!item) return nullptr;

    QGraphicsItem* copy = nullptr;

    // Handle different item types
    if (auto rectItem = qgraphicsitem_cast<QGraphicsRectItem*>(item)) {
        auto newRect = new QGraphicsRectItem(rectItem->rect());
        newRect->setPen(rectItem->pen());
        newRect->setBrush(rectItem->brush());
        copy = newRect;
    }
    else if (auto ellipseItem = qgraphicsitem_cast<QGraphicsEllipseItem*>(item)) {
        auto newEllipse = new QGraphicsEllipseItem(ellipseItem->rect());
        newEllipse->setPen(ellipseItem->pen());
        newEllipse->setBrush(ellipseItem->brush());
        copy = newEllipse;
    }
    else if (auto lineItem = qgraphicsitem_cast<QGraphicsLineItem*>(item)) {
        auto newLine = new QGraphicsLineItem(lineItem->line());
        newLine->setPen(lineItem->pen());
        copy = newLine;
    }
    else if (auto pathItem = qgraphicsitem_cast<QGraphicsPathItem*>(item)) {
        auto newPath = new QGraphicsPathItem(pathItem->path());
        newPath->setPen(pathItem->pen());
        newPath->setBrush(pathItem->brush());
        copy = newPath;
    }
    else if (auto textItem = qgraphicsitem_cast<QGraphicsTextItem*>(item)) {
        auto newText = new QGraphicsTextItem(textItem->toPlainText());
        newText->setFont(textItem->font());
        newText->setDefaultTextColor(textItem->defaultTextColor());
        copy = newText;
    }
    else if (auto simpleTextItem = qgraphicsitem_cast<QGraphicsSimpleTextItem*>(item)) {
        auto newSimpleText = new QGraphicsSimpleTextItem(simpleTextItem->text());
        newSimpleText->setFont(simpleTextItem->font());
        newSimpleText->setBrush(simpleTextItem->brush());
        newSimpleText->setPen(simpleTextItem->pen());
        copy = newSimpleText;
    }
    // FIX: Add support for QGraphicsPixmapItem (images)
    else if (auto pixmapItem = qgraphicsitem_cast<QGraphicsPixmapItem*>(item)) {
        auto newPixmap = new QGraphicsPixmapItem(pixmapItem->pixmap());
        newPixmap->setOffset(pixmapItem->offset());
        newPixmap->setTransformationMode(pixmapItem->transformationMode());
        copy = newPixmap;
    }
    // FIX: Add support for QGraphicsSvgItem (vectors)
    else if (auto svgItem = qgraphicsitem_cast<QGraphicsSvgItem*>(item)) {
        copy = deepCopySvgItem(svgItem);
    }
    // FIX: Add support for QGraphicsItemGroup (grouped items)
    else if (auto groupItem = qgraphicsitem_cast<QGraphicsItemGroup*>(item)) {
        // For groups, we need to recursively copy all child items
        auto newGroup = new QGraphicsItemGroup();
        QList<QGraphicsItem*> childItems = groupItem->childItems();
        for (QGraphicsItem* childItem : childItems) {
            QGraphicsItem* childCopy = duplicateGraphicsItem(childItem);
            if (childCopy) {
                newGroup->addToGroup(childCopy);
            }
        }
        copy = newGroup;
    }
    else if (auto vgItem = dynamic_cast<VectorGraphicsItem*>(item)) {
        copy = cloneVectorGraphicsItem(vgItem);
    }

    if (copy) {
        // Copy common properties (CRITICAL: Include opacity here)
        copy->setTransform(item->transform());
        copy->setFlags(item->flags());
        copy->setZValue(item->zValue());
        copy->setOpacity(item->opacity());  // FIX: Ensure opacity is preserved
        copy->setVisible(item->isVisible());
        copy->setEnabled(item->isEnabled());
        copy->setPos(item->pos());
        copy->setRotation(item->rotation());
        copy->setScale(item->scale());
    }

    return copy;
}

void MainWindow::setTimelineLength()
{
    bool ok;
    int currentLength = m_timeline ? m_timeline->getTotalFrames() : 100;

    int newLength = QInputDialog::getInt(this,
        "Set Timeline Length",
        "Enter the number of frames:",
        currentLength,
        1,
        100000, // Maximum
        1,
        &ok);

    if (ok && newLength > 0) {
        if (m_timeline) {
            m_timeline->setTotalFrames(newLength);
            m_statusLabel->setText(QString("Timeline length set to %1 frames").arg(newLength));
            m_isModified = true;
        }
    }
}

void MainWindow::createBlankKeyframe()
{
    if (m_canvas) {
        // FIXED: Use the new method to create truly blank keyframe
        m_canvas->createBlankKeyframe(m_currentFrame);

        if (m_timeline) {
            m_timeline->updateLayersFromCanvas();
        }

        m_statusLabel->setText(QString("Blank keyframe created at frame %1").arg(m_currentFrame));
        m_isModified = true;
    }
}


void MainWindow::removeKeyframe()
{
    if (m_canvas) {
        int layer = m_canvas->getCurrentLayer();
        if (m_undoStack) {
            m_undoStack->push(new RemoveKeyframeCommand(m_canvas, layer, m_currentFrame));
        }
        else {
            m_canvas->removeKeyframe(layer, m_currentFrame);
        }
        if (m_timeline) {
            m_timeline->updateLayersFromCanvas();
        }
        updateFrameActions();
        m_statusLabel->setText("Keyframe removed");
        m_isModified = true;
    }
}

void MainWindow::setFrameRate(int fps)
{
    m_frameRate = fps;
    m_playbackTimer->setInterval(1000 / m_frameRate);
    m_fpsLabel->setText(QString("FPS: %1").arg(fps));
    if (m_audioPlayer && m_audioPlayer->duration() > 0) {
        m_audioFrameLength = static_cast<int>((m_audioPlayer->duration() / 1000.0) * m_frameRate);
        if (m_timeline)
            m_timeline->setAudioTrack(m_audioFrameLength, m_audioWaveform, QFileInfo(m_audioFile).fileName());
    }
}

// Tool operations

void MainWindow::setTool(ToolType tool)
{
    qDebug() << "setTool called with:" << static_cast<int>(tool);

    if (m_currentTool != tool) {
        // FIXED: Clean up the previous tool before switching
        if (m_canvas && m_tools.find(m_currentTool) != m_tools.end()) {
            Tool* previousTool = m_tools[m_currentTool].get();
            cleanupIfPreviewing(previousTool);

            // Special cleanup for eraser tool
            if (m_currentTool == EraseTool) {
                ::EraseTool* eraserTool = dynamic_cast<::EraseTool*>(previousTool);
                if (eraserTool) {
                    eraserTool->cleanup();
                }
            }

            // Add cleanup for other tools as needed in the future
            // You can extend this pattern for any tool that needs cleanup
        }

        m_currentTool = tool;

        if (m_canvas && m_tools.find(tool) != m_tools.end()) {
            Tool* selectedTool = m_tools[tool].get();
            m_canvas->setCurrentTool(selectedTool);

            qDebug() << "Tool changed to:" << tool << "Tool object:" << selectedTool;
        }
        else {
            qDebug() << "ERROR: Tool not found or canvas is null. Tool type:" << static_cast<int>(tool);
            if (!m_canvas) qDebug() << "Canvas is null";
            if (m_tools.find(tool) == m_tools.end()) qDebug() << "Tool not found in tools map";
        }

        // Update tool panel
        if (m_toolsPanel) {
            m_toolsPanel->setActiveTool(tool);
            qDebug() << "Updated tools panel";
        }
        else {
            qDebug() << "Tools panel is null";
        }

        // Update menu actions
        switch (tool) {
        case SelectTool: if (m_selectToolAction) m_selectToolAction->setChecked(true); break;
        case DrawTool: if (m_drawToolAction) m_drawToolAction->setChecked(true); break;
        case LineTool: if (m_lineToolAction) m_lineToolAction->setChecked(true); break;
        case RectangleTool: if (m_rectangleToolAction) m_rectangleToolAction->setChecked(true); break;
        case EllipseTool: if (m_ellipseToolAction) m_ellipseToolAction->setChecked(true); break;
        case TextTool: if (m_textToolAction) m_textToolAction->setChecked(true); break;
        default: break;
        }

        onToolChanged(tool);
    }
}

void MainWindow::cleanupIfPreviewing(Tool* tool)
{
    if (!tool) return;

    // Hide BucketFill preview if present
    if (auto* bucket = dynamic_cast<::BucketFillTool*>(tool)) {
        bucket->hideFillPreview();  // removes and deletes preview item
    }
}

void MainWindow::setupComprehensiveUndo()
{
    // Ensure all operations use undo commands
    if (m_canvas) {
        // Connect canvas operations to undo system
        connect(m_canvas, &Canvas::selectionChanged, this, &MainWindow::updateUndoRedoActions);

        // Set up comprehensive undo for all canvas operations
        setupCanvasUndoOperations();
    }
}

void MainWindow::setupCanvasUndoOperations()
{
    if (!m_canvas) return;

    // Override Canvas methods to always use undo commands
    // This ensures every operation goes through the undo system
}

void MainWindow::updateUndoRedoActions()
{
    // Update undo/redo action states
    if (m_undoStack) {
        m_undoAction->setEnabled(m_undoStack->canUndo());
        m_redoAction->setEnabled(m_undoStack->canRedo());

        // Update action text with command descriptions
        if (m_undoStack->canUndo()) {
            m_undoAction->setText(QString("Undo %1").arg(m_undoStack->undoText()));
        }
        else {
            m_undoAction->setText("Undo");
        }

        if (m_undoStack->canRedo()) {
            m_redoAction->setText(QString("Redo %1").arg(m_undoStack->redoText()));
        }
        else {
            m_redoAction->setText("Redo");
        }
    }
}


void MainWindow::selectToolActivated() { setTool(SelectTool); }
void MainWindow::drawToolActivated() { setTool(DrawTool); }
void MainWindow::lineToolActivated() { setTool(LineTool); }
void MainWindow::rectangleToolActivated() { setTool(RectangleTool); }
void MainWindow::ellipseToolActivated() { setTool(EllipseTool); }
void MainWindow::textToolActivated() { setTool(TextTool); }
void MainWindow::bucketFillToolActivated() { setTool(BucketFillTool); }
void MainWindow::gradientFillToolActivated() { setTool(GradientFillTool); }
void MainWindow::eraseToolActivated() { setTool(EraseTool); }

// Alignment operations
void MainWindow::alignObjects(AlignmentType alignment)
{
    if (!m_canvas || !m_canvas->scene()) return;

    QList<QGraphicsItem*> selectedItems = m_canvas->scene()->selectedItems();
    if (selectedItems.size() < 2) return;

    // Store original positions
    QHash<QGraphicsItem*, QPointF> originalPositions;
    for (QGraphicsItem* item : selectedItems) {
        originalPositions[item] = item->pos();
    }

    // Perform alignment
    m_canvas->alignSelectedItems(alignment);

    // Calculate total movement and create compound undo command
    if (m_undoStack) {
        m_undoStack->beginMacro("Align Objects");

        for (QGraphicsItem* item : selectedItems) {
            QPointF delta = item->pos() - originalPositions[item];
            if (delta.manhattanLength() > 0.1) { // Only if item actually moved
                // Reset position and create move command
                item->setPos(originalPositions[item]);
                MoveCommand* moveCommand = new MoveCommand(m_canvas, { item }, delta);
                m_undoStack->push(moveCommand);
            }
        }

        m_undoStack->endMacro();
    }
}

// Enhanced transform operations with undo support
void MainWindow::bringToFront()
{
    if (!m_canvas || !m_canvas->scene()) return;

    QList<QGraphicsItem*> selectedItems = m_canvas->scene()->selectedItems();
    if (selectedItems.isEmpty()) return;

    // Store original Z-values
    QHash<QGraphicsItem*, qreal> originalZValues;
    for (QGraphicsItem* item : selectedItems) {
        originalZValues[item] = item->zValue();
    }

    // Find max Z-value
    qreal maxZ = 0;
    for (QGraphicsItem* item : m_canvas->scene()->items()) {
        if (item->zValue() > maxZ) maxZ = item->zValue();
    }

    // Create compound undo command for Z-value changes
    if (m_undoStack) {
        m_undoStack->beginMacro("Bring to Front");

        for (QGraphicsItem* item : selectedItems) {
            qreal newZ = maxZ + 1;
            // Create property change command for Z-value
            PropertyChangeCommand* zCommand = new PropertyChangeCommand(
                m_canvas, item, "zValue", originalZValues[item], newZ);
            m_undoStack->push(zCommand);
            maxZ += 1;
        }

        m_undoStack->endMacro();
    }

    if (m_canvas) {
        m_canvas->storeCurrentFrameState();
    }
}

void MainWindow::bringForward()
{
    if (!m_canvas || !m_canvas->scene()) return;

    QList<QGraphicsItem*> selectedItems = m_canvas->scene()->selectedItems();
    if (selectedItems.isEmpty()) return;

    if (m_undoStack) {
        m_undoStack->beginMacro("Bring Forward");

        for (QGraphicsItem* item : selectedItems) {
            qreal originalZ = item->zValue();
            qreal newZ = originalZ + 1;

            PropertyChangeCommand* zCommand = new PropertyChangeCommand(
                m_canvas, item, "zValue", originalZ, newZ);
            m_undoStack->push(zCommand);
        }

        m_undoStack->endMacro();
    }

    if (m_canvas) {
        m_canvas->storeCurrentFrameState();
    }
}

void MainWindow::sendBackward()
{
    if (!m_canvas || !m_canvas->scene()) return;

    QList<QGraphicsItem*> selectedItems = m_canvas->scene()->selectedItems();
    if (selectedItems.isEmpty()) return;

    if (m_undoStack) {
        m_undoStack->beginMacro("Send Backward");

        for (QGraphicsItem* item : selectedItems) {
            qreal originalZ = item->zValue();
            qreal newZ = originalZ - 1;

            PropertyChangeCommand* zCommand = new PropertyChangeCommand(
                m_canvas, item, "zValue", originalZ, newZ);
            m_undoStack->push(zCommand);
        }

        m_undoStack->endMacro();
    }

    if (m_canvas) {
        m_canvas->storeCurrentFrameState();
    }
}

void MainWindow::sendToBack()
{
    if (!m_canvas || !m_canvas->scene()) return;

    QList<QGraphicsItem*> selectedItems = m_canvas->scene()->selectedItems();
    if (selectedItems.isEmpty()) return;

    // Find min Z-value
    qreal minZ = 0;
    for (QGraphicsItem* item : m_canvas->scene()->items()) {
        if (item->zValue() < minZ) minZ = item->zValue();
    }

    if (m_undoStack) {
        m_undoStack->beginMacro("Send to Back");

        for (QGraphicsItem* item : selectedItems) {
            qreal originalZ = item->zValue();
            qreal newZ = minZ - 1;

            PropertyChangeCommand* zCommand = new PropertyChangeCommand(
                m_canvas, item, "zValue", originalZ, newZ);
            m_undoStack->push(zCommand);
            minZ -= 1;
        }

        m_undoStack->endMacro();
    }

    if (m_canvas) {
        m_canvas->storeCurrentFrameState();
    }
}

// Enhanced flip operations with undo support
void MainWindow::flipHorizontal()
{
    if (!m_canvas || !m_canvas->scene()) return;

    QList<QGraphicsItem*> selectedItems = m_canvas->scene()->selectedItems();
    if (selectedItems.isEmpty()) return;

    if (m_undoStack) {
        m_undoStack->beginMacro("Flip Horizontal");

        for (QGraphicsItem* item : selectedItems) {
            QTransform originalTransform = item->transform();
            QTransform newTransform = originalTransform;
            newTransform.scale(-1, 1);

            TransformCommand* transformCommand = new TransformCommand(
                m_canvas, item, originalTransform, newTransform);
            m_undoStack->push(transformCommand);
        }

        m_undoStack->endMacro();
    }

    if (m_canvas) {
        m_canvas->storeCurrentFrameState();
    }
}

void MainWindow::flipVertical()
{
    if (!m_canvas || !m_canvas->scene()) return;

    QList<QGraphicsItem*> selectedItems = m_canvas->scene()->selectedItems();
    if (selectedItems.isEmpty()) return;

    if (m_undoStack) {
        m_undoStack->beginMacro("Flip Vertical");

        for (QGraphicsItem* item : selectedItems) {
            QTransform originalTransform = item->transform();
            QTransform newTransform = originalTransform;
            newTransform.scale(1, -1);

            TransformCommand* transformCommand = new TransformCommand(
                m_canvas, item, originalTransform, newTransform);
            m_undoStack->push(transformCommand);
        }

        m_undoStack->endMacro();
    }

    if (m_canvas) {
        m_canvas->storeCurrentFrameState();
    }
}

void MainWindow::rotateClockwise()
{
    rotateSelected(90);
}

void MainWindow::rotateCounterClockwise()
{
    rotateSelected(-90);
}

void MainWindow::rotateSelected(double angle)
{
    if (!m_canvas || !m_canvas->scene()) return;

    QList<QGraphicsItem*> selectedItems = m_canvas->scene()->selectedItems();
    if (selectedItems.isEmpty()) return;

    if (m_undoStack) {
        m_undoStack->beginMacro(QString("Rotate %1°").arg(angle));

        for (QGraphicsItem* item : selectedItems) {
            QTransform originalTransform = item->transform();

            // Compute the item's visual center in scene coordinates
            QPointF sceneCenter = item->mapToScene(item->boundingRect().center());

            // Reposition and rotate around the item's center
            item->setTransformOriginPoint(item->boundingRect().center());
            item->setPos(sceneCenter - item->transformOriginPoint());
            item->setRotation(item->rotation() + angle);

            QTransform newTransform = item->transform();
            TransformCommand* transformCommand = new TransformCommand(
                m_canvas, item, originalTransform, newTransform);
            m_undoStack->push(transformCommand);
        }

        m_undoStack->endMacro();
    }

    if (m_canvas) {
        m_canvas->storeCurrentFrameState();
    }
}

// Layer management
void MainWindow::addLayer()
{
    auto layer = std::make_unique<AnimationLayer>(QString("Layer %1").arg(m_layers.size() + 1));
    m_layers.push_back(std::move(layer));
    m_currentLayerIndex = m_layers.size() - 1;

    if (m_layerManager) {
        m_layerManager->updateLayers();
    }
}

void MainWindow::removeLayer()
{
    if (m_layers.size() > 1 && m_currentLayerIndex < m_layers.size()) {
        m_layers.erase(m_layers.begin() + m_currentLayerIndex);
        if (m_currentLayerIndex >= m_layers.size()) {
            m_currentLayerIndex = m_layers.size() - 1;
        }

        if (m_layerManager) {
            m_layerManager->updateLayers();
        }
    }
}

void MainWindow::duplicateLayer()
{
    // Implementation for duplicating current layer
}

void MainWindow::moveLayerUp()
{
    // Implementation for moving layer up
}

void MainWindow::moveLayerDown()
{
    // Implementation for moving layer down
}

void MainWindow::toggleLayerVisibility()
{
    // Implementation for toggling layer visibility
}

void MainWindow::toggleLayerLock()
{
    // Implementation for toggling layer lock
}


void MainWindow::showDrawingToolSettings()
{
    // Find and show the drawing tool settings dialog
    auto it = m_tools.find(DrawTool);
    if (it != m_tools.end()) {
        DrawingTool* drawingTool = dynamic_cast<DrawingTool*>(it->second.get());
        if (drawingTool) {
            drawingTool->showSettingsDialog();
        }
    }
}

void MainWindow::setDrawingToolStrokeWidth(double width)
{
    auto it = m_tools.find(DrawTool);
    if (it != m_tools.end()) {
        DrawingTool* drawingTool = dynamic_cast<DrawingTool*>(it->second.get());
        if (drawingTool) {
            drawingTool->setStrokeWidth(width);
            m_statusLabel->setText(QString("Drawing tool stroke width set to %1px").arg(width));
        }
    }
}

void MainWindow::setDrawingToolColor(const QColor& color)
{
    auto it = m_tools.find(DrawTool);
    if (it != m_tools.end()) {
        DrawingTool* drawingTool = dynamic_cast<DrawingTool*>(it->second.get());
        if (drawingTool) {
            drawingTool->setStrokeColor(color);
            m_statusLabel->setText(QString("Drawing tool color set to %1").arg(color.name()));
        }
    }
}

// Color and style
void MainWindow::setStrokeColor()
{
    QColor color = QColorDialog::getColor(m_currentStrokeColor, this, "Select Stroke Color");
    if (color.isValid()) {
        m_currentStrokeColor = color;
        if (m_colorPanel) {
            m_colorPanel->setStrokeColor(color);
        }
        if (m_canvas) {
            m_canvas->setStrokeColor(color);
        }
    }
}

void MainWindow::setFillColor()
{
    QColor color = QColorDialog::getColor(m_currentFillColor, this, "Select Fill Color");
    if (color.isValid()) {
        m_currentFillColor = color;
        if (m_colorPanel) {
            m_colorPanel->setFillColor(color);
        }
        if (m_canvas) {
            m_canvas->setFillColor(color);
        }
    }
}

void MainWindow::setStrokeWidth(double width)
{
    m_currentStrokeWidth = width;
    if (m_canvas) {
        m_canvas->setStrokeWidth(width);
    }
}

void MainWindow::setOpacity(double opacity)
{
    m_currentOpacity = opacity;
}


void MainWindow::onZoomChanged(double zoom)
{
    m_currentZoom = zoom;
    m_zoomLabel->setText(QString("Zoom: %1%").arg(static_cast<int>(zoom * 100)));
}

void MainWindow::onSelectionChanged()
{
    // Update selection-dependent UI elements
    bool hasSelection = m_canvas && m_canvas->hasSelection();

    m_cutAction->setEnabled(hasSelection);
    m_copyAction->setEnabled(hasSelection);
    m_groupAction->setEnabled(hasSelection);
    m_ungroupAction->setEnabled(hasSelection);

    // Update alignment actions
    m_alignLeftAction->setEnabled(hasSelection);
    m_alignCenterAction->setEnabled(hasSelection);
    m_alignRightAction->setEnabled(hasSelection);
    m_alignTopAction->setEnabled(hasSelection);
    m_alignMiddleAction->setEnabled(hasSelection);
    m_alignBottomAction->setEnabled(hasSelection);

    // Update status bar
    if (hasSelection) {
        int selectionCount = m_canvas->getSelectionCount();
        m_selectionLabel->setText(QString("%1 item(s) selected").arg(selectionCount));
    }
    else {
        m_selectionLabel->setText("No selection");
    }
}

void MainWindow::onLayerSelectionChanged()
{
    // Update layer-dependent UI elements
}

void MainWindow::onToolChanged(ToolType tool)
{
    // Update tool-dependent UI elements
    QString toolName;
    switch (tool) {
    case SelectTool: toolName = "Select"; break;
    case DrawTool: toolName = "Draw"; break;
    case LineTool: toolName = "Line"; break;
    case RectangleTool: toolName = "Rectangle"; break;
    case EllipseTool: toolName = "Ellipse"; break;
    case TextTool: toolName = "Text"; break;
    case BucketFillTool: toolName = "Bucket Fill"; break;
	case GradientFillTool: toolName = "Gradient Fill"; break;
    case EraseTool: toolName = "Erase"; break;
    }
    m_statusLabel->setText(QString("%1 tool active").arg(toolName));
}

void MainWindow::onCanvasMouseMove(QPointF position)
{
    m_positionLabel->setText(QString("X: %1  Y: %2")
        .arg(static_cast<int>(position.x()))
        .arg(static_cast<int>(position.y())));
}

void MainWindow::onPlaybackTimer()
{
    if (m_currentFrame < m_totalFrames) {
        nextFrame();
    }
    else {
        firstFrame();
        if (!m_audioFile.isEmpty())
            m_audioPlayer->setPosition(0);
    }
}

void MainWindow::togglePanel(const QString& panelName)
{
    QDockWidget* dock = nullptr;

    if (panelName.toLower() == "tools") {
        dock = m_toolsDock;
    }
    else if (panelName.toLower() == "properties") {
        dock = m_propertiesDock;
    }
    else if (panelName.toLower() == "timeline") {
        dock = m_timelineDock;
    }

    if (dock) {
        if (dock->isVisible()) {
            dock->hide();
        }
        else {
            dock->show();
            dock->raise();
        }

        m_statusLabel->setText(QString("%1 panel %2")
            .arg(panelName)
            .arg(dock->isVisible() ? "shown" : "hidden"));
    }
    else {
        m_statusLabel->setText(QString("Panel '%1' not found").arg(panelName));
    }
}

void MainWindow::updatePlayback()
{
    if (m_playAction) {
        m_playAction->setText(m_isPlaying ? "Pause" : "Play");
        m_playAction->setToolTip(m_isPlaying ? "Pause animation" : "Play animation");
    }

    if (m_timeline) {
        m_timeline->setPlaying(m_isPlaying);
    }

    if (m_fpsLabel) {
        m_fpsLabel->setText(QString("FPS: %1").arg(m_frameRate));
    }

    if (m_frameLabel) {
        m_frameLabel->setText(QString("Frame: %1 / %2").arg(m_currentFrame).arg(m_totalFrames));
    }

    if (m_statusLabel) {
        if (m_isPlaying) {
            m_statusLabel->setText("Playing animation");
        }
        else {
            m_statusLabel->setText("Animation stopped");
        }
    }

    if (m_playbackTimer) {
        m_playbackTimer->setInterval(1000 / m_frameRate);
    }

    // Enable/disable frame navigation during playback
    if (m_nextFrameAction) m_nextFrameAction->setEnabled(!m_isPlaying);
    if (m_prevFrameAction) m_prevFrameAction->setEnabled(!m_isPlaying);
    if (m_firstFrameAction) m_firstFrameAction->setEnabled(!m_isPlaying);
    if (m_lastFrameAction) m_lastFrameAction->setEnabled(!m_isPlaying);
}

// Helper methods
void MainWindow::updateUI()
{
    updateStatusBar();
}

void MainWindow::updateStatusBar()
{
    m_frameLabel->setText(QString("Frame: %1").arg(m_currentFrame));
    m_zoomLabel->setText(QString("Zoom: %1%").arg(static_cast<int>(m_currentZoom * 100)));
    m_fpsLabel->setText(QString("FPS: %1").arg(m_frameRate));
}

bool MainWindow::maybeSave()
{
    if (m_isModified) {
        QMessageBox::StandardButton ret = QMessageBox::warning(this,
            "FrameDirector",
            "The document has been modified.\n"
            "Do you want to save your changes?",
            QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);

        if (ret == QMessageBox::Save) {
            return save(), true;
        }
        else if (ret == QMessageBox::Cancel) {
            return false;
        }
    }
    return true;
}

void MainWindow::loadFile(const QString& fileName)
{
    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly)) {
        QMessageBox::warning(this, "Error", "Unable to open file");
        return;
    }
    QByteArray data = file.readAll();
    file.close();

    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (!doc.isObject()) {
        QMessageBox::warning(this, "Error", "Invalid project file");
        return;
    }
    QJsonObject root = doc.object();

    if (m_canvas) {
        if (root.contains("canvas"))
            m_canvas->fromJson(root.value("canvas").toObject());
        else
            m_canvas->fromJson(root); // backward compatibility
        if (m_timeline) {
            m_timeline->updateLayersFromCanvas();
        }
    }

    // Restore audio settings if present
    m_audioFile = root.value("audioFile").toString();
    m_audioFrameLength = root.value("audioFrameLength").toInt(0);
    if (!m_audioFile.isEmpty()) {
        m_audioPlayer->setSource(QUrl::fromLocalFile(m_audioFile));
        if (m_audioFrameLength > 0) {
            m_audioWaveform = createAudioWaveform(m_audioFile, m_audioFrameLength);
            if (m_timeline)
                m_timeline->setAudioTrack(m_audioFrameLength, m_audioWaveform, QFileInfo(m_audioFile).fileName());
        }
    }

    setCurrentFile(fileName);
    m_isModified = false;
    m_statusLabel->setText("File loaded");
}

bool MainWindow::saveFile(const QString& fileName)
{
    if (!m_canvas)
        return false;

    QJsonObject root;
    root["canvas"] = m_canvas->toJson();
    if (!m_audioFile.isEmpty()) {
        root["audioFile"] = m_audioFile;
        root["audioFrameLength"] = m_audioFrameLength;
    }
    QJsonDocument doc(root);

    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly)) {
        QMessageBox::warning(this, "Error", "Unable to save file");
        return false;
    }
    file.write(doc.toJson());
    file.close();

    setCurrentFile(fileName);
    m_isModified = false;
    m_statusLabel->setText("File saved");
    return true;
}

void MainWindow::setCurrentFile(const QString& fileName)
{
    m_currentFile = fileName;
    setWindowTitle(QString("FrameDirector - %1").arg(strippedName(fileName)));
}

QString MainWindow::strippedName(const QString& fullFileName)
{
    return QFileInfo(fullFileName).fileName();
}

void MainWindow::readSettings()
{
    QSettings settings;
    restoreGeometry(settings.value("geometry").toByteArray());
    restoreState(settings.value("windowState").toByteArray());
}

void MainWindow::writeSettings()
{
    QSettings settings;
    settings.setValue("geometry", saveGeometry());
    settings.setValue("windowState", saveState());
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    qDebug() << "MainWindow::closeEvent called";

    if (maybeSave()) {
        // FIXED: Proper cleanup before closing

        // 1. Stop playback timer
        if (m_playbackTimer) {
            m_playbackTimer->stop();
        }

        // 2. Clean up all tools
        qDebug() << "Cleaning up tools before close...";
        for (auto& toolPair : m_tools) {
            Tool* tool = toolPair.second.get();
            if (tool) {
                // Special cleanup for eraser tool
                if (toolPair.first == EraseTool) {
                    ::EraseTool* eraserTool = dynamic_cast<::EraseTool*>(tool);
                    if (eraserTool) {
                        eraserTool->cleanup();
                    }
                }
                // Add cleanup for other tools as needed
            }
        }

        // 3. Clear undo stack
        if (m_undoStack) {
            qDebug() << "Clearing undo stack before close...";
            m_undoStack->clear();
            m_frameClipboard.clear();
        }

        // 4. Save settings
        writeSettings();

        qDebug() << "Accepting close event";
        event->accept();
    }
    else {
        qDebug() << "Close event ignored";
        event->ignore();
    }
}


void MainWindow::keyPressEvent(QKeyEvent* event)
{
    // Handle frame creation shortcuts
    switch (event->key()) {
    case Qt::Key_F5:
        if (event->modifiers() & Qt::ShiftModifier) {
            clearCurrentFrame();
        }
        else {
            //insertFrame();
        }
        break;
    case Qt::Key_F6:
        addKeyframe();  // Use existing method name
        break;
    case Qt::Key_F7:
        insertBlankKeyframe();
        break;
    case Qt::Key_C:
        if (event->modifiers() == (Qt::ControlModifier | Qt::ShiftModifier)) {
            copyCurrentFrame();
            return;
        }
        break;
    case Qt::Key_V:
        if (event->modifiers() == (Qt::ControlModifier | Qt::ShiftModifier)) {
            pasteFrame();
            return;
        }
        break;
    case Qt::Key_Left:
        if (event->modifiers() & Qt::ControlModifier) {
            previousKeyframe();  // Use existing method name
        }
        else {
            previousFrame();
        }
        break;
    case Qt::Key_Right:
        if (event->modifiers() & Qt::ControlModifier) {
            nextKeyframe();  // Use existing method name
        }
        else {
            nextFrame();
        }
        break;
    default:
        QMainWindow::keyPressEvent(event);
    }
}


void MainWindow::createStatusBar()
{
    m_statusLabel = new QLabel("Ready");
    m_positionLabel = new QLabel("X: 0  Y: 0");
    m_zoomLabel = new QLabel("Zoom: 100%");
    m_frameLabel = new QLabel("Frame: 1");
    m_selectionLabel = new QLabel("No selection");
    m_fpsLabel = new QLabel("FPS: 24");

    statusBar()->addWidget(m_statusLabel);
    statusBar()->addPermanentWidget(m_positionLabel);
    statusBar()->addPermanentWidget(m_zoomLabel);
    statusBar()->addPermanentWidget(m_frameLabel);
    statusBar()->addPermanentWidget(m_selectionLabel);
    statusBar()->addPermanentWidget(m_fpsLabel);
}

void MainWindow::createDockWindows()
{
    // Tools Panel
    m_toolsDock = new QDockWidget("Tools", this);
    m_toolsPanel = new ToolsPanel(this);
    m_toolsDock->setWidget(m_toolsPanel);
    m_toolsDock->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);
    addDockWidget(Qt::LeftDockWidgetArea, m_toolsDock);

    // Right panel tabs
    m_rightPanelTabs = new QTabWidget;

    // Properties Panel
    m_propertiesPanel = new PropertiesPanel(this);
    m_rightPanelTabs->addTab(m_propertiesPanel, "Properties");

    // Color Panel
    m_colorPanel = new ColorPanel(this);
    m_rightPanelTabs->addTab(m_colorPanel, "Colors");

    // Layers Panel
    //m_layerManager = new LayerManager(this);
    //m_rightPanelTabs->addTab(m_layerManager, "Layers");

    // Alignment Panel
    m_alignmentPanel = new AlignmentPanel(this);
    m_rightPanelTabs->addTab(m_alignmentPanel, "Align");

    // Right dock for panels
    m_propertiesDock = new QDockWidget("Properties", this);
    m_propertiesDock->setWidget(m_rightPanelTabs);
    m_propertiesDock->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);
    addDockWidget(Qt::RightDockWidgetArea, m_propertiesDock);

    // Connect panels
    connect(m_toolsPanel, &ToolsPanel::toolSelected, this, &MainWindow::setTool);
    connect(m_layerManager, &LayerManager::layerAdded, this, &MainWindow::addLayer);
    connect(m_layerManager, &LayerManager::layerRemoved, this, &MainWindow::removeLayer);
}