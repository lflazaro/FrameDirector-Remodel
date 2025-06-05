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
#include "Tools/EraseTool.h"
#include "Commands/UndoCommands.h"
#include "Animation/AnimationLayer.h"
#include "Animation/AnimationKeyframe.h"

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
#include <QFontDialog>
#include <QFileDialog>
#include <QMessageBox>
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
#include <QGraphicsPixmapItem>
#include <QGraphicsSvgItem>
#include <QImageReader>
#include <QSvgWidget>
#include <QDir>
#include <QStandardPaths>

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
    , m_totalFrames(100)
    , m_currentZoom(1.0)
    , m_frameRate(24)
    , m_isPlaying(false)
    , m_playbackTimer(new QTimer(this))
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

    connect(m_timeline, &Timeline::frameChanged, this, &MainWindow::onFrameChanged);
    connect(m_timeline, &Timeline::keyframeAdded, this, &MainWindow::addKeyframe);

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

        connect(m_canvas, &Canvas::frameAutoConverted, [this](int frame, int layer) {
            updateToolAvailability();
            updateFrameActions();
            m_statusLabel->setText(QString("Extended frame auto-converted to keyframe at frame %1").arg(frame));
            m_isModified = true;
            });

        // NEW: Connect tweening signals
        connect(m_canvas, &Canvas::tweeningApplied, [this](int layer, int startFrame, int endFrame, TweenType type) {
            updateToolAvailability();
            QString typeStr = (type == TweenType::Motion) ? "Motion" : "Classic";
            m_statusLabel->setText(QString("%1 tween applied to layer %2, frames %3-%4")
                .arg(typeStr).arg(layer).arg(startFrame).arg(endFrame));
            m_isModified = true;
            });

        connect(m_canvas, &Canvas::tweeningRemoved, [this](int layer, int startFrame, int endFrame) {
            updateToolAvailability();
            m_statusLabel->setText(QString("Tween removed from layer %1, frames %2-%3")
                .arg(layer).arg(startFrame).arg(endFrame));
            m_isModified = true;
            });
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

    if (m_layerManager) {
        connect(m_layerManager, &LayerManager::currentLayerChanged, this, &MainWindow::onCurrentLayerChanged);
    }

    // Enhanced timeline connections
    if (m_timeline) {
        connect(m_timeline, &Timeline::frameChanged, this, &MainWindow::onFrameChangedWithLayer);
        connect(m_timeline, &Timeline::layerSelected, this, &MainWindow::onCurrentLayerChanged);
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

    m_importVectorAction = new QAction("Import &Vector", this);
    m_importVectorAction->setIcon(QIcon(":/icons/import.png"));
    m_importVectorAction->setStatusTip("Import a vector file");
    connect(m_importVectorAction, &QAction::triggered, this, &MainWindow::importVector);

    m_exportAnimationAction = new QAction("Export &Animation", this);
    m_exportAnimationAction->setIcon(QIcon(":/icons/export.png"));
    m_exportAnimationAction->setStatusTip("Export as video/GIF");
    connect(m_exportAnimationAction, &QAction::triggered, this, &MainWindow::exportAnimation);

    m_exportFrameAction = new QAction("Export &Frame", this);
    m_exportFrameAction->setIcon(QIcon(":/icons/export.png"));
    m_exportFrameAction->setStatusTip("Export current frame as image");
    connect(m_exportFrameAction, &QAction::triggered, this, &MainWindow::exportFrame);

    m_exportSVGAction = new QAction("Export &SVG", this);
    m_exportSVGAction->setIcon(QIcon(":/icons/export.png"));
    m_exportSVGAction->setStatusTip("Export as SVG file");
    connect(m_exportSVGAction, &QAction::triggered, this, &MainWindow::exportSVG);

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
    // Create a left arrow icon by rotating the right arrow
    QPixmap leftArrow = QIcon(":/icons/arrow-right.png").pixmap(16, 16);
    QTransform transform;
    transform.rotate(180);
    leftArrow = leftArrow.transformed(transform);
    m_prevFrameAction->setIcon(QIcon(leftArrow));
    m_prevFrameAction->setShortcut(QKeySequence("Left"));
    connect(m_prevFrameAction, &QAction::triggered, this, &MainWindow::previousFrame);

    m_firstFrameAction = new QAction("&First Frame", this);
    // Create a double left arrow for first frame
    QPixmap firstFrame = QIcon(":/icons/arrow-right.png").pixmap(16, 16);
    QPixmap doubleLeft(32, 16);
    doubleLeft.fill(Qt::transparent);
    QPainter painter(&doubleLeft);
    painter.drawPixmap(0, 0, firstFrame.transformed(transform));
    painter.drawPixmap(10, 0, firstFrame.transformed(transform));
    m_firstFrameAction->setIcon(QIcon(doubleLeft));
    m_firstFrameAction->setShortcut(QKeySequence("Home"));
    connect(m_firstFrameAction, &QAction::triggered, this, &MainWindow::firstFrame);

    m_lastFrameAction = new QAction("&Last Frame", this);
    // Create a double right arrow for last frame
    QPixmap lastFrame = QIcon(":/icons/arrow-right.png").pixmap(16, 16);
    QPixmap doubleRight(32, 16);
    doubleRight.fill(Qt::transparent);
    QPainter painter2(&doubleRight);
    painter2.drawPixmap(0, 0, lastFrame);
    painter2.drawPixmap(10, 0, lastFrame);
    m_lastFrameAction->setIcon(QIcon(doubleRight));
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

    m_blankKeyframeAction = new QAction("Create &Blank Keyframe", this);
    m_blankKeyframeAction->setIcon(QIcon(":/icons/branch-closed.png"));
    m_blankKeyframeAction->setShortcut(QKeySequence("Ctrl+Shift+K"));
    m_blankKeyframeAction->setStatusTip("Create blank keyframe (clear current frame)");
    connect(m_blankKeyframeAction, &QAction::triggered, this, &MainWindow::createBlankKeyframe);

    // NEW: Enhanced frame creation actions
    m_insertFrameAction = new QAction("Insert Extended &Frame", this);
    m_insertFrameAction->setIcon(QIcon(":/icons/arrow-right.png"));  // Reuse existing icon
    m_insertFrameAction->setShortcut(QKeySequence("F5"));
    m_insertFrameAction->setStatusTip("Insert frame extending from previous keyframe");
    connect(m_insertFrameAction, &QAction::triggered, this, &MainWindow::insertFrame);

    m_insertBlankKeyframeAction = new QAction("Insert &Blank Keyframe", this);
    m_insertBlankKeyframeAction->setIcon(QIcon(":/icons/branch-closed.png"));  // Reuse existing icon
    m_insertBlankKeyframeAction->setShortcut(QKeySequence("F7"));
    m_insertBlankKeyframeAction->setStatusTip("Insert blank keyframe (clears content)");
    connect(m_insertBlankKeyframeAction, &QAction::triggered, this, &MainWindow::createBlankKeyframe);

    m_clearFrameAction = new QAction("&Clear Frame", this);
    m_clearFrameAction->setIcon(QIcon(":/icons/stop.png"));  // Reuse stop icon for "clear"
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
    // Create a right arrow with a small diamond to indicate keyframe
    QPixmap nextKeyframePixmap = QIcon(":/icons/arrow-right.png").pixmap(16, 16);
    m_nextKeyframeAction->setIcon(QIcon(nextKeyframePixmap));
    m_nextKeyframeAction->setShortcut(QKeySequence("Ctrl+Right"));
    m_nextKeyframeAction->setStatusTip("Go to next keyframe");
    connect(m_nextKeyframeAction, &QAction::triggered, this, &MainWindow::nextKeyframe);

    m_prevKeyframeAction = new QAction("Previous &Keyframe", this);
    // Create a left arrow with a small diamond to indicate keyframe
    QPixmap prevKeyframePixmap = QIcon(":/icons/arrow-right.png").pixmap(16, 16);
    prevKeyframePixmap = prevKeyframePixmap.transformed(transform);  // Use the transform from above
    m_prevKeyframeAction->setIcon(QIcon(prevKeyframePixmap));
    m_prevKeyframeAction->setShortcut(QKeySequence("Ctrl+Left"));
    m_prevKeyframeAction->setStatusTip("Go to previous keyframe");
    connect(m_prevKeyframeAction, &QAction::triggered, this, &MainWindow::previousKeyframe);

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
    m_alignLeftAction->setIcon(QIcon(":/icons/arrow-right.png")); // We'll rotate this
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
    m_importMenu->addAction(m_importVectorAction);

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
    frameMenu->addAction(m_insertFrameAction);         // NEW: F5 - Insert extended frame
    frameMenu->addAction(m_addKeyframeAction);         // ENHANCED: F6 - Insert keyframe (existing)
    frameMenu->addAction(m_insertBlankKeyframeAction); // NEW: F7 - Insert blank keyframe
    frameMenu->addSeparator();
    frameMenu->addAction(m_clearFrameAction);          // NEW: Shift+F5 - Clear frame
    frameMenu->addAction(m_convertToKeyframeAction);   // NEW: F8 - Convert to keyframe
    frameMenu->addSeparator();
    frameMenu->addAction(m_copyFrameAction);           // Existing: Copy frame content

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
    m_animationToolBar->addAction(m_insertFrameAction);         // NEW: F5 - Insert extended frame
    m_animationToolBar->addAction(m_addKeyframeAction);         // ENHANCED: F6 - Insert keyframe (existing)
    m_animationToolBar->addAction(m_insertBlankKeyframeAction); // NEW: F7 - Insert blank keyframe

    m_animationToolBar->addSeparator();

    // NEW: Additional frame operations
    m_animationToolBar->addAction(m_convertToKeyframeAction);   // NEW: F8 - Convert to keyframe
    m_animationToolBar->addAction(m_clearFrameAction);          // NEW: Shift+F5 - Clear frame
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
    m_layerManager = new LayerManager(this);
    m_rightPanelTabs->addTab(m_layerManager, "Layers");

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
    if (maybeSave()) {
        m_canvas->clear();
        m_layers.clear();
        m_keyframes.clear();
        m_currentFrame = 1;
        m_totalFrames = 100;
        m_currentFile.clear();
        m_isModified = false;
        addLayer();
        updateUI();
        setWindowTitle("FrameDirector - Untitled");
    }
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

        // Test if the SVG can be loaded
        QSvgRenderer* renderer = new QSvgRenderer(fileName);
        if (!renderer->isValid()) {
            QMessageBox::warning(this, "Import Error",
                QString("Could not load SVG file:\n%1\nThe file may be corrupted or use unsupported features.").arg(fileName));
            delete renderer;
            return;
        }

        // Create SVG graphics item
        QGraphicsSvgItem* svgItem = new QGraphicsSvgItem(fileName);
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

        delete renderer;
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
                QSvgRenderer* renderer = new QSvgRenderer(fileName);
                if (renderer->isValid()) {
                    QGraphicsSvgItem* svgItem = new QGraphicsSvgItem(fileName);
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
                delete renderer;
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
        m_statusLabel->setText(QString("Audio imported: %1").arg(QFileInfo(fileName).fileName()));

        QMessageBox::information(this, "Audio Import",
            QString("Audio file '%1' imported successfully.\n"
                "Audio track functionality will be implemented in a future version.")
            .arg(QFileInfo(fileName).fileName()));
    }
}

void MainWindow::exportAnimation()
{
    QString fileName = QFileDialog::getSaveFileName(this,
        "Export Animation", "", "Video Files (*.mp4 *.avi);;GIF Files (*.gif)");
    if (!fileName.isEmpty()) {
        // Implementation for exporting animation
        m_statusLabel->setText("Animation exported");
    }
}

void MainWindow::exportFrame()
{
    QString fileName = QFileDialog::getSaveFileName(this,
        "Export Frame", "", "Image Files (*.png *.jpg *.jpeg)");
    if (!fileName.isEmpty()) {
        // Implementation for exporting current frame
        m_statusLabel->setText("Frame exported");
    }
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
}

void MainWindow::lastFrame()
{
    onFrameChanged(m_totalFrames);
}

void MainWindow::addKeyframe()
{
    if (m_canvas) {
        // Use new enhanced createKeyframe method
        m_canvas->createKeyframe(m_currentFrame);

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

void MainWindow::copyCurrentFrame()
{
    if (m_canvas && m_currentFrame > 1) {
        // Check if there's content in the current frame
        if (m_canvas->hasKeyframe(m_currentFrame)) {
            // Copy current frame content to create a new keyframe
            m_canvas->storeCurrentFrameState();
            m_statusLabel->setText(QString("Frame %1 content saved").arg(m_currentFrame));
        }
        else {
            m_statusLabel->setText("No content to copy in current frame");
        }
    }
}

void MainWindow::createBlankKeyframe()
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
    if (m_canvas && m_canvas->getFrameType(m_currentFrame, m_currentLayerIndex) == FrameType::ExtendedFrame) {
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

    FrameType currentFrameType = m_canvas->getFrameType(m_currentFrame, m_currentLayerIndex);
    bool hasContent = m_canvas->hasContent(m_currentFrame, m_currentLayerIndex);
    bool isKeyframe = m_canvas->hasKeyframe(m_currentFrame);
    bool isExtended = m_canvas->isExtendedFrame(m_currentFrame, m_currentLayerIndex);
    bool hasTweening = m_canvas->hasTweening(m_currentLayerIndex, m_currentFrame);

    // Convert to keyframe action: only enabled for extended frames
    m_convertToKeyframeAction->setEnabled(isExtended && !hasTweening);

    // Clear frame action: disabled for extended frames and tweened frames
    m_clearFrameAction->setEnabled(hasContent && !isExtended && !hasTweening);

    // Insert frame action: disabled if would create gap in tweened span
    m_insertFrameAction->setEnabled(!hasTweening);

    // Navigation actions
    m_nextKeyframeAction->setEnabled(m_canvas->getNextKeyframeAfter(m_currentFrame) != -1);
    m_prevKeyframeAction->setEnabled(m_canvas->getLastKeyframeBefore(m_currentFrame) != -1);

    qDebug() << "Frame actions updated - Extended:" << isExtended << "Tweened:" << hasTweening;
}

// NEW: Show frame type in status bar
void MainWindow::showFrameTypeIndicator()
{
    if (!m_canvas) return;

    FrameType frameType = m_canvas->getFrameType(m_currentFrame, m_currentLayerIndex);
    QString typeText;

    switch (frameType) {
    case FrameType::Empty:
        typeText = "Empty Frame";
        break;
    case FrameType::Keyframe:
        typeText = "Keyframe";
        break;
    case FrameType::ExtendedFrame:
        int sourceKeyframe = m_canvas->getSourceKeyframe(m_currentFrame);
        typeText = QString("Extended Frame (from %1)").arg(sourceKeyframe);
        break;
    }

    // Update status label with frame type info
    QString statusText = QString("Frame: %1 (%2)").arg(m_currentFrame).arg(typeText);
    m_frameLabel->setText(statusText);
}

void MainWindow::removeKeyframe()
{
    // Implementation for removing keyframe at current frame
    m_statusLabel->setText("Keyframe removed");
}

void MainWindow::setFrameRate(int fps)
{
    m_frameRate = fps;
    m_playbackTimer->setInterval(1000 / m_frameRate);
    m_fpsLabel->setText(QString("FPS: %1").arg(fps));
}

// Tool operations

void MainWindow::setTool(ToolType tool)
{
    qDebug() << "setTool called with:" << static_cast<int>(tool);

    if (m_currentTool != tool) {
        // FIXED: Clean up the previous tool before switching
        if (m_canvas && m_tools.find(m_currentTool) != m_tools.end()) {
            Tool* previousTool = m_tools[m_currentTool].get();

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

            // Set rotation origin to item center
            QPointF center = item->boundingRect().center();
            item->setTransformOriginPoint(center);

            // Create new transform with rotation
            QTransform newTransform = originalTransform;
            newTransform.translate(center.x(), center.y());
            newTransform.rotate(angle);
            newTransform.translate(-center.x(), -center.y());

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

// Event handlers
void MainWindow::onFrameChanged(int frame)
{
    m_currentFrame = frame;

    if (m_timeline) {
        m_timeline->setCurrentFrame(frame);
    }

    if (m_canvas) {
        m_canvas->setCurrentFrame(frame);
    }

    // NEW: Update frame-dependent UI
    updateFrameActions();
    showFrameTypeIndicator();
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
    // Implementation for loading project file
    setCurrentFile(fileName);
    m_statusLabel->setText("File loaded");
}

bool MainWindow::saveFile(const QString& fileName)
{
    // Implementation for saving project file
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

void MainWindow::onCurrentLayerChanged(int layer)
{
    m_currentLayerIndex = layer;
    updateToolAvailability();

    // Update canvas current layer
    if (m_canvas) {
        m_canvas->setCurrentLayer(layer);
    }

    qDebug() << "Current layer changed to:" << layer;
}

void MainWindow::onFrameChangedWithLayer(int frame)
{
    onFrameChanged(frame);  // Call existing method
    updateToolAvailability();
}

void MainWindow::updateToolAvailability()
{
    if (!m_canvas) return;

    bool canDraw = m_canvas->canDrawOnFrame(m_currentFrame, m_currentLayerIndex);
    bool isExtended = m_canvas->isExtendedFrame(m_currentFrame, m_currentLayerIndex);
    bool hasTweening = m_canvas->hasTweening(m_currentLayerIndex, m_currentFrame);

    if (canDraw && !hasTweening) {
        enableDrawingTools();
    }
    else {
        disableDrawingTools();
    }

    // Update frame actions based on extended frame state
    updateFrameActions();

    // Update status bar with current state
    QString statusText = QString("Frame: %1, Layer: %2").arg(m_currentFrame).arg(m_currentLayerIndex);

    if (hasTweening) {
        statusText += " (Tweened - Drawing Disabled)";
    }
    else if (isExtended) {
        statusText += " (Extended Frame)";
    }

    m_statusLabel->setText(statusText);
}

void MainWindow::disableDrawingTools()
{
    if (m_drawingToolsEnabled) {
        m_drawingToolsEnabled = false;

        // Disable drawing tool actions
        m_drawToolAction->setEnabled(false);
        m_lineToolAction->setEnabled(false);
        m_rectangleToolAction->setEnabled(false);
        m_ellipseToolAction->setEnabled(false);
        m_textToolAction->setEnabled(false);

        // Disable bucket fill and erase tools
        if (m_bucketFillToolAction) m_bucketFillToolAction->setEnabled(false);
        if (m_eraseToolAction) m_eraseToolAction->setEnabled(false);

        // Switch to select tool if a drawing tool is active
        if (m_currentTool != SelectTool) {
            setTool(SelectTool);
        }

        // Update tools panel
        if (m_toolsPanel) {
            m_toolsPanel->setDrawingToolsEnabled(false);
        }

        qDebug() << "Drawing tools disabled - tweening active";
    }
}

void MainWindow::onTweeningStateChanged()
{
    // Update tool availability based on current tweening state
    updateToolAvailability();

    // Update frame actions based on tweening state
    updateFrameActions();

    // Update timeline display
    if (m_timeline && m_timeline->m_drawingArea) {
        m_timeline->m_drawingArea->update();
    }

    // Update status bar with tweening information
    if (m_canvas && m_canvas->hasTweening(m_currentLayerIndex, m_currentFrame)) {
        TweenType tweenType = m_canvas->getTweenType(m_currentLayerIndex, m_currentFrame);
        QString typeStr = (tweenType == TweenType::Motion) ? "Motion" : "Classic";
        m_statusLabel->setText(QString("Frame %1 - %2 Tween Active (Drawing Disabled)")
            .arg(m_currentFrame).arg(typeStr));
    }
    else {
        m_statusLabel->setText(QString("Frame %1").arg(m_currentFrame));
    }

    // Mark as modified if tweening state changed
    m_isModified = true;

    qDebug() << "Tweening state changed - UI updated";
}
void MainWindow::enableDrawingTools()
{
    if (!m_drawingToolsEnabled) {
        m_drawingToolsEnabled = true;

        // Enable drawing tool actions
        m_drawToolAction->setEnabled(true);
        m_lineToolAction->setEnabled(true);
        m_rectangleToolAction->setEnabled(true);
        m_ellipseToolAction->setEnabled(true);
        m_textToolAction->setEnabled(true);

        // Enable bucket fill and erase tools
        if (m_bucketFillToolAction) m_bucketFillToolAction->setEnabled(true);
        if (m_eraseToolAction) m_eraseToolAction->setEnabled(true);

        // Update tools panel
        if (m_toolsPanel) {
            m_toolsPanel->setDrawingToolsEnabled(true);
        }

        qDebug() << "Drawing tools enabled";
    }
}

void MainWindow::keyPressEvent(QKeyEvent* event)
{
    // Handle global key shortcuts
    switch (event->key()) {
    case Qt::Key_Delete:
        if (m_canvas && m_canvas->hasSelection()) {
            m_canvas->deleteSelected();
        }
        break;
    case Qt::Key_Escape:
        if (m_canvas) {
            m_canvas->clearSelection();
        }
        break;

    case Qt::Key_F5:
        if (event->modifiers() & Qt::ShiftModifier) {
            clearCurrentFrame();
        }
        else {
            insertFrame();
        }
        break;
    case Qt::Key_F6:
        addKeyframe();  // Use existing method name
        break;
    case Qt::Key_F7:
        createBlankKeyframe();
        break;
    case Qt::Key_F8:
        convertToKeyframe();
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