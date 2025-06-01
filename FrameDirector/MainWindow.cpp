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
#include "Tools/LineTool.h"          // ← Make sure this exists!
#include "Tools/RectangleTool.h"     // ← Make sure this exists!
#include "Tools/EllipseTool.h"       // ← Make sure this exists!
#include "Tools/TextTool.h"          // ← Make sure this exists!
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
    setupTools();
    setupAnimationSystem();

    // Setup central widget layout
    QWidget* centralWidget = new QWidget;
    setCentralWidget(centralWidget);

    m_mainSplitter = new QSplitter(Qt::Horizontal, this);

    // Create canvas
    m_canvas = new Canvas(this);
    m_canvas->setMinimumSize(400, 300);
    connect(m_canvas, &Canvas::selectionChanged, this, &MainWindow::onSelectionChanged);
    connect(m_canvas, &Canvas::mousePositionChanged, this, &MainWindow::onCanvasMouseMove);

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
    setTool(SelectTool);
}

MainWindow::~MainWindow()
{
}

void MainWindow::createActions()
{
    // File Menu Actions
    m_newAction = new QAction("&New", this);
    m_newAction->setShortcut(QKeySequence::New);
    m_newAction->setStatusTip("Create a new animation project");
    connect(m_newAction, &QAction::triggered, this, &MainWindow::newFile);

    m_openAction = new QAction("&Open", this);
    m_openAction->setShortcut(QKeySequence::Open);
    m_openAction->setStatusTip("Open an existing project");
    connect(m_openAction, &QAction::triggered, this, &MainWindow::open);

    m_saveAction = new QAction("&Save", this);
    m_saveAction->setShortcut(QKeySequence::Save);
    m_saveAction->setStatusTip("Save the current project");
    connect(m_saveAction, &QAction::triggered, this, &MainWindow::save);

    m_saveAsAction = new QAction("Save &As...", this);
    m_saveAsAction->setShortcut(QKeySequence::SaveAs);
    m_saveAsAction->setStatusTip("Save the project with a new name");
    connect(m_saveAsAction, &QAction::triggered, this, &MainWindow::saveAs);

    m_importImageAction = new QAction("Import &Image", this);
    m_importImageAction->setStatusTip("Import an image file");
    connect(m_importImageAction, &QAction::triggered, this, &MainWindow::importImage);

    m_importVectorAction = new QAction("Import &Vector", this);
    m_importVectorAction->setStatusTip("Import a vector file");
    connect(m_importVectorAction, &QAction::triggered, this, &MainWindow::importVector);

    m_exportAnimationAction = new QAction("Export &Animation", this);
    m_exportAnimationAction->setStatusTip("Export as video/GIF");
    connect(m_exportAnimationAction, &QAction::triggered, this, &MainWindow::exportAnimation);

    m_exportFrameAction = new QAction("Export &Frame", this);
    m_exportFrameAction->setStatusTip("Export current frame as image");
    connect(m_exportFrameAction, &QAction::triggered, this, &MainWindow::exportFrame);

    m_exportSVGAction = new QAction("Export &SVG", this);
    m_exportSVGAction->setStatusTip("Export as SVG file");
    connect(m_exportSVGAction, &QAction::triggered, this, &MainWindow::exportSVG);

    m_exitAction = new QAction("E&xit", this);
    m_exitAction->setShortcut(QKeySequence::Quit);
    m_exitAction->setStatusTip("Exit FrameDirector");
    connect(m_exitAction, &QAction::triggered, this, &QWidget::close);

    // Edit Menu Actions
    m_undoAction = new QAction("&Undo", this);
    m_undoAction->setShortcut(QKeySequence::Undo);
    m_undoAction->setEnabled(false);
    connect(m_undoAction, &QAction::triggered, this, &MainWindow::undo);

    m_redoAction = new QAction("&Redo", this);
    m_redoAction->setShortcut(QKeySequence::Redo);
    m_redoAction->setEnabled(false);
    connect(m_redoAction, &QAction::triggered, this, &MainWindow::redo);

    m_cutAction = new QAction("Cu&t", this);
    m_cutAction->setShortcut(QKeySequence::Cut);
    connect(m_cutAction, &QAction::triggered, this, &MainWindow::cut);

    m_copyAction = new QAction("&Copy", this);
    m_copyAction->setShortcut(QKeySequence::Copy);
    connect(m_copyAction, &QAction::triggered, this, &MainWindow::copy);

    m_pasteAction = new QAction("&Paste", this);
    m_pasteAction->setShortcut(QKeySequence::Paste);
    connect(m_pasteAction, &QAction::triggered, this, &MainWindow::paste);

    m_selectAllAction = new QAction("Select &All", this);
    m_selectAllAction->setShortcut(QKeySequence::SelectAll);
    connect(m_selectAllAction, &QAction::triggered, this, &MainWindow::selectAll);

    m_groupAction = new QAction("&Group", this);
    m_groupAction->setShortcut(QKeySequence("Ctrl+G"));
    connect(m_groupAction, &QAction::triggered, this, &MainWindow::group);

    m_ungroupAction = new QAction("&Ungroup", this);
    m_ungroupAction->setShortcut(QKeySequence("Ctrl+Shift+G"));
    connect(m_ungroupAction, &QAction::triggered, this, &MainWindow::ungroup);

    // View Menu Actions
    m_zoomInAction = new QAction("Zoom &In", this);
    m_zoomInAction->setShortcut(QKeySequence::ZoomIn);
    connect(m_zoomInAction, &QAction::triggered, this, &MainWindow::zoomIn);

    m_zoomOutAction = new QAction("Zoom &Out", this);
    m_zoomOutAction->setShortcut(QKeySequence::ZoomOut);
    connect(m_zoomOutAction, &QAction::triggered, this, &MainWindow::zoomOut);

    m_zoomToFitAction = new QAction("Zoom to &Fit", this);
    m_zoomToFitAction->setShortcut(QKeySequence("Ctrl+0"));
    connect(m_zoomToFitAction, &QAction::triggered, this, &MainWindow::zoomToFit);

    m_toggleGridAction = new QAction("Show &Grid", this);
    m_toggleGridAction->setCheckable(true);
    m_toggleGridAction->setChecked(true);
    connect(m_toggleGridAction, &QAction::triggered, this, &MainWindow::toggleGrid);

    m_toggleSnapAction = new QAction("&Snap to Grid", this);
    m_toggleSnapAction->setCheckable(true);
    connect(m_toggleSnapAction, &QAction::triggered, this, &MainWindow::toggleSnapToGrid);

    m_toggleRulersAction = new QAction("Show &Rulers", this);
    m_toggleRulersAction->setCheckable(true);
    connect(m_toggleRulersAction, &QAction::triggered, this, &MainWindow::toggleRulers);

    // Animation Menu Actions
    m_playAction = new QAction("&Play", this);
    m_playAction->setShortcut(QKeySequence("Space"));
    m_playAction->setStatusTip("Play animation");
    connect(m_playAction, &QAction::triggered, this, &MainWindow::play);

    m_stopAction = new QAction("&Stop", this);
    m_stopAction->setShortcut(QKeySequence("Shift+Space"));
    m_stopAction->setStatusTip("Stop animation");
    connect(m_stopAction, &QAction::triggered, this, &MainWindow::stop);

    m_nextFrameAction = new QAction("&Next Frame", this);
    m_nextFrameAction->setShortcut(QKeySequence("Right"));
    connect(m_nextFrameAction, &QAction::triggered, this, &MainWindow::nextFrame);

    m_prevFrameAction = new QAction("&Previous Frame", this);
    m_prevFrameAction->setShortcut(QKeySequence("Left"));
    connect(m_prevFrameAction, &QAction::triggered, this, &MainWindow::previousFrame);

    m_firstFrameAction = new QAction("&First Frame", this);
    m_firstFrameAction->setShortcut(QKeySequence("Home"));
    connect(m_firstFrameAction, &QAction::triggered, this, &MainWindow::firstFrame);

    m_lastFrameAction = new QAction("&Last Frame", this);
    m_lastFrameAction->setShortcut(QKeySequence("End"));
    connect(m_lastFrameAction, &QAction::triggered, this, &MainWindow::lastFrame);

    m_addKeyframeAction = new QAction("Add &Keyframe", this);
    m_addKeyframeAction->setShortcut(QKeySequence("Ctrl+K"));
    connect(m_addKeyframeAction, &QAction::triggered, this, &MainWindow::addKeyframe);

    // Tool Actions
    m_toolActionGroup = new QActionGroup(this);

    m_selectToolAction = new QAction("&Select Tool", this);
    m_selectToolAction->setShortcut(QKeySequence("V"));
    m_selectToolAction->setCheckable(true);
    m_selectToolAction->setChecked(true);
    m_selectToolAction->setData(static_cast<int>(SelectTool));
    m_toolActionGroup->addAction(m_selectToolAction);

    m_drawToolAction = new QAction("&Draw Tool", this);
    m_drawToolAction->setShortcut(QKeySequence("P"));
    m_drawToolAction->setCheckable(true);
    m_drawToolAction->setData(static_cast<int>(DrawTool));
    m_toolActionGroup->addAction(m_drawToolAction);

    m_lineToolAction = new QAction("&Line Tool", this);
    m_lineToolAction->setShortcut(QKeySequence("L"));
    m_lineToolAction->setCheckable(true);
    m_lineToolAction->setData(static_cast<int>(LineTool));
    m_toolActionGroup->addAction(m_lineToolAction);

    m_rectangleToolAction = new QAction("&Rectangle Tool", this);
    m_rectangleToolAction->setShortcut(QKeySequence("R"));
    m_rectangleToolAction->setCheckable(true);
    m_rectangleToolAction->setData(static_cast<int>(RectangleTool));
    m_toolActionGroup->addAction(m_rectangleToolAction);

    m_ellipseToolAction = new QAction("&Ellipse Tool", this);
    m_ellipseToolAction->setShortcut(QKeySequence("O"));
    m_ellipseToolAction->setCheckable(true);
    m_ellipseToolAction->setData(static_cast<int>(EllipseTool));
    m_toolActionGroup->addAction(m_ellipseToolAction);

    m_textToolAction = new QAction("&Text Tool", this);
    m_textToolAction->setShortcut(QKeySequence("T"));
    m_textToolAction->setCheckable(true);
    m_textToolAction->setData(static_cast<int>(TextTool));
    m_toolActionGroup->addAction(m_textToolAction);

    // Alignment Actions
    m_alignLeftAction = new QAction("Align &Left", this);
    connect(m_alignLeftAction, &QAction::triggered, this, &MainWindow::alignLeft);

    m_alignCenterAction = new QAction("Align &Center", this);
    connect(m_alignCenterAction, &QAction::triggered, this, &MainWindow::alignCenter);

    m_alignRightAction = new QAction("Align &Right", this);
    connect(m_alignRightAction, &QAction::triggered, this, &MainWindow::alignRight);

    m_alignTopAction = new QAction("Align &Top", this);
    connect(m_alignTopAction, &QAction::triggered, this, &MainWindow::alignTop);

    m_alignMiddleAction = new QAction("Align &Middle", this);
    connect(m_alignMiddleAction, &QAction::triggered, this, &MainWindow::alignMiddle);

    m_alignBottomAction = new QAction("Align &Bottom", this);
    connect(m_alignBottomAction, &QAction::triggered, this, &MainWindow::alignBottom);

    m_distributeHorizontallyAction = new QAction("Distribute &Horizontally", this);
    connect(m_distributeHorizontallyAction, &QAction::triggered, this, &MainWindow::distributeHorizontally);

    m_distributeVerticallyAction = new QAction("Distribute &Vertically", this);
    connect(m_distributeVerticallyAction, &QAction::triggered, this, &MainWindow::distributeVertically);

    // Transform Actions
    m_bringToFrontAction = new QAction("Bring to &Front", this);
    m_bringToFrontAction->setShortcut(QKeySequence("Ctrl+Shift+]"));
    connect(m_bringToFrontAction, &QAction::triggered, this, &MainWindow::bringToFront);

    m_sendToBackAction = new QAction("Send to &Back", this);
    m_sendToBackAction->setShortcut(QKeySequence("Ctrl+Shift+["));
    connect(m_sendToBackAction, &QAction::triggered, this, &MainWindow::sendToBack);

    m_flipHorizontalAction = new QAction("Flip &Horizontal", this);
    connect(m_flipHorizontalAction, &QAction::triggered, this, &MainWindow::flipHorizontal);

    m_flipVerticalAction = new QAction("Flip &Vertical", this);
    connect(m_flipVerticalAction, &QAction::triggered, this, &MainWindow::flipVertical);
}

void MainWindow::createMenus()
{
    // File Menu
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
    m_arrangeMenu->addAction(m_sendToBackAction);

    m_transformMenu = m_objectMenu->addMenu("&Transform");
    m_transformMenu->addAction(m_flipHorizontalAction);
    m_transformMenu->addAction(m_flipVerticalAction);

    // View Menu
    m_viewMenu = menuBar()->addMenu("&View");
    m_viewMenu->addAction(m_zoomInAction);
    m_viewMenu->addAction(m_zoomOutAction);
    m_viewMenu->addAction(m_zoomToFitAction);
    m_viewMenu->addSeparator();
    m_viewMenu->addAction(m_toggleGridAction);
    m_viewMenu->addAction(m_toggleSnapAction);
    m_viewMenu->addAction(m_toggleRulersAction);

    // Animation Menu
    m_animationMenu = menuBar()->addMenu("&Animation");
    m_animationMenu->addAction(m_playAction);
    m_animationMenu->addAction(m_stopAction);
    m_animationMenu->addSeparator();
    m_animationMenu->addAction(m_nextFrameAction);
    m_animationMenu->addAction(m_prevFrameAction);
    m_animationMenu->addSeparator();
    m_animationMenu->addAction(m_firstFrameAction);
    m_animationMenu->addAction(m_lastFrameAction);
    m_animationMenu->addSeparator();
    m_animationMenu->addAction(m_addKeyframeAction);

    // Help Menu
    m_helpMenu = menuBar()->addMenu("&Help");
    m_helpMenu->addAction("&About", this, [this]() {
        QMessageBox::about(this, "About FrameDirector",
            "FrameDirector v1.0\n\n"
            "A professional vector animation tool\n"
            "Built with Qt and C++");
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

    // Animation Toolbar
    m_animationToolBar = addToolBar("Animation");
    m_animationToolBar->addAction(m_firstFrameAction);
    m_animationToolBar->addAction(m_prevFrameAction);
    m_animationToolBar->addAction(m_playAction);
    m_animationToolBar->addAction(m_stopAction);
    m_animationToolBar->addAction(m_nextFrameAction);
    m_animationToolBar->addAction(m_lastFrameAction);
    m_animationToolBar->addSeparator();
    m_animationToolBar->addAction(m_addKeyframeAction);
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
    connect(m_colorPanel, &ColorPanel::strokeColorChanged, this, &MainWindow::setStrokeColor);
    connect(m_colorPanel, &ColorPanel::fillColorChanged, this, &MainWindow::setFillColor);
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
    m_tools[SelectTool] = std::make_unique<SelectionTool>(this);
    m_tools[DrawTool] = std::make_unique<DrawingTool>(this);
    m_tools[LineTool] = std::make_unique<::LineTool>(this);
    m_tools[RectangleTool] = std::make_unique<::RectangleTool>(this);
    m_tools[EllipseTool] = std::make_unique<::EllipseTool>(this);
    m_tools[TextTool] = std::make_unique<::TextTool>(this);
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
    QString fileName = QFileDialog::getOpenFileName(this,
        "Import Image", "", "Image Files (*.png *.jpg *.jpeg *.bmp *.gif)");
    if (!fileName.isEmpty()) {
        // Implementation for importing images
        m_statusLabel->setText("Image imported");
    }
}

void MainWindow::importVector()
{
    QString fileName = QFileDialog::getOpenFileName(this,
        "Import Vector", "", "Vector Files (*.svg)");
    if (!fileName.isEmpty()) {
        // Implementation for importing vector files
        m_statusLabel->setText("Vector imported");
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
    copy();
    // Delete selected items
}

void MainWindow::copy()
{
    // Implementation for copying selected items
}

void MainWindow::paste()
{
    // Implementation for pasting items
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
    // Implementation for adding keyframe at current frame
    m_statusLabel->setText("Keyframe added");
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
    if (m_currentTool != tool) {
        m_currentTool = tool;
        if (m_canvas && m_tools.find(tool) != m_tools.end()) {
            m_canvas->setCurrentTool(m_tools[tool].get());
        }
        onToolChanged(tool);
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
    if (m_canvas) {
        m_canvas->alignSelectedItems(alignment);
    }
}

// Transform operations
void MainWindow::bringToFront()
{
    if (m_canvas) {
        m_canvas->bringSelectedToFront();
    }
}

void MainWindow::bringForward()
{
    if (m_canvas) {
        m_canvas->bringSelectedForward();
    }
}

void MainWindow::sendBackward()
{
    if (m_canvas) {
        m_canvas->sendSelectedBackward();
    }
}

void MainWindow::sendToBack()
{
    if (m_canvas) {
        m_canvas->sendSelectedToBack();
    }
}

void MainWindow::flipHorizontal()
{
    if (m_canvas) {
        m_canvas->flipSelectedHorizontal();
    }
}

void MainWindow::flipVertical()
{
    if (m_canvas) {
        m_canvas->flipSelectedVertical();
    }
}

void MainWindow::rotateClockwise()
{
    if (m_canvas) {
        m_canvas->rotateSelected(90);
    }
}

void MainWindow::rotateCounterClockwise()
{
    if (m_canvas) {
        m_canvas->rotateSelected(-90);
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

// Color and style
void MainWindow::setStrokeColor()
{
    QColor color = QColorDialog::getColor(m_currentStrokeColor, this, "Select Stroke Color");
    if (color.isValid()) {
        m_currentStrokeColor = color;
        if (m_colorPanel) {
            m_colorPanel->setStrokeColor(color);
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
    }
}

void MainWindow::setStrokeWidth(double width)
{
    m_currentStrokeWidth = width;
}

void MainWindow::setOpacity(double opacity)
{
    m_currentOpacity = opacity;
}

// Event handlers
void MainWindow::onFrameChanged(int frame)
{
    m_currentFrame = frame;
    m_frameLabel->setText(QString("Frame: %1").arg(frame));

    if (m_timeline) {
        m_timeline->setCurrentFrame(frame);
    }

    if (m_canvas) {
        m_canvas->setCurrentFrame(frame);
    }
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
    if (maybeSave()) {
        writeSettings();
        event->accept();
    }
    else {
        event->ignore();
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
    default:
        QMainWindow::keyPressEvent(event);
    }
}