#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QGraphicsItem>
#include <QDockWidget>
#include <QListWidget>
#include <QTreeWidget>
#include <QSlider>
#include <QSpinBox>
#include <QComboBox>
#include <QTabWidget>
#include <QStatusBar>
#include <QToolBar>
#include <QMenuBar>
#include <QAction>
#include <QActionGroup>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QSplitter>
#include <QColorDialog>
#include <QFontDialog>
#include <QTimer>
#include <QPropertyAnimation>
#include <QGraphicsEffect>
#include <QUndoStack>
#include <QUndoCommand>
#include <memory>
#include <vector>
#include <map>

QT_BEGIN_NAMESPACE
class QGraphicsEllipseItem;
class QGraphicsRectItem;
class QGraphicsLineItem;
class QGraphicsTextItem;
class QGraphicsPathItem;
QT_END_NAMESPACE

// Forward declarations
class Canvas;
class Timeline;
class LayerManager;
class PropertiesPanel;
class ToolsPanel;
class ColorPanel;
class AlignmentPanel;
class Tool;
class DrawingTool;
class SelectionTool;
class VectorGraphicsItem;
class AnimationKeyframe;
class AnimationLayer;

class MainWindow : public QMainWindow
{
    Q_OBJECT



public:
    MainWindow(QWidget* parent = nullptr);
    ~MainWindow();

    enum ToolType {
        SelectTool,
        DrawTool,
        LineTool,
        RectangleTool,
        EllipseTool,
        TextTool,
        BucketFillTool,
        EraseTool
    };

    enum AlignmentType {
        AlignLeft,
        AlignCenter,
        AlignRight,
        AlignTop,
        AlignMiddle,
        AlignBottom,
        DistributeHorizontally,
        DistributeVertically
    };

public slots:
    void alignObjects(AlignmentType alignment);
    void bringToFront();
    void bringForward();
    void sendBackward();
    void sendToBack();

protected:
    void closeEvent(QCloseEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;

private slots:
    // File menu actions
    void newFile();
    void open();
    void save();
    void saveAs();
    void importImage();
    void importVector();
    void importAudio();
    void exportAnimation();
    void exportFrame();
    void exportSVG();

    // Edit menu actions
    void undo();
    void redo();
    void cut();
    void copy();
    void paste();
    void selectAll();
    void group();
    void ungroup();

    // View actions
    void zoomIn();
    void zoomOut();
    void zoomToFit();
    void toggleGrid();
    void toggleSnapToGrid();
    void toggleRulers();
    void togglePanel(const QString& panelName);

    // Animation actions
    void play();
    void stop();
    void nextFrame();
    void previousFrame();
    void nextKeyframe();
    void previousKeyframe();
    void firstFrame();
    void lastFrame();
    void addKeyframe();
    void removeKeyframe();
    void setFrameRate(int fps);
    void copyCurrentFrame();
    void createBlankKeyframe();

    // Tool actions
    void setTool(ToolType tool);
    void selectToolActivated();
    void drawToolActivated();
    void lineToolActivated();
    void rectangleToolActivated();
    void ellipseToolActivated();
    void textToolActivated();
    void bucketFillToolActivated();
    void eraseToolActivated();

    // Transform and alignment actions
    void alignLeft() { alignObjects(AlignLeft); }
    void alignCenter() { alignObjects(AlignCenter); }
    void alignRight() { alignObjects(AlignRight); }
    void alignTop() { alignObjects(AlignTop); }
    void alignMiddle() { alignObjects(AlignMiddle); }
    void alignBottom() { alignObjects(AlignBottom); }
    void distributeHorizontally() { alignObjects(DistributeHorizontally); }
    void distributeVertically() { alignObjects(DistributeVertically); }

    void flipHorizontal();
    void flipVertical();
    void rotateClockwise();
    void rotateCounterClockwise();

    // Layer management
    void addLayer();
    void removeLayer();
    void duplicateLayer();
    void moveLayerUp();
    void moveLayerDown();
    void toggleLayerVisibility();
    void toggleLayerLock();

    // Color and style
    void setStrokeColor();
    void setFillColor();
    void setStrokeWidth(double width);
    void setOpacity(double opacity);

    // Timeline and animation
    void onFrameChanged(int frame);
    void onZoomChanged(double zoom);
    void onSelectionChanged();
    void onLayerSelectionChanged();
    void onToolChanged(ToolType tool);
    void onCanvasMouseMove(QPointF position);

    // Playback
    void updatePlayback();
    void onPlaybackTimer();

private:
    void createActions();
    void createMenus();
    void createToolBars();
    void createDockWindows();
    void createStatusBar();
    void setupAnimationSystem();
    void setupStyleSheet();
    void readSettings();
    void writeSettings();
    bool maybeSave();
    void loadFile(const QString& fileName);
    bool saveFile(const QString& fileName);
    void setCurrentFile(const QString& fileName);
    void updateRecentFileActions();
    QString strippedName(const QString& fullFileName);
    void updateUI();
    void updateStatusBar();

    // Tool management
    void setupTools();
    void activateTool(ToolType tool);
    Tool* getCurrentTool() const;
    void connectToolsAndCanvas();        // <-- ADD THIS
    void setupColorConnections();        // <-- ADD THIS
    void connectLayerManager();
    void createTestShape();              // <-- ADD THIS
    void updateSelectedItemsStroke(const QColor& color);
    void updateSelectedItemsFill(const QColor& color);


    // Animation helpers
    void createKeyframeAtCurrentFrame();
    void interpolateFrames();
    void updateTimelineDisplay();

    // Central widgets and panels
    QSplitter* m_mainSplitter;
    QSplitter* m_leftSplitter;
    QSplitter* m_rightSplitter;

    Canvas* m_canvas;
    Timeline* m_timeline;
    LayerManager* m_layerManager;

    // Dock widgets
    QDockWidget* m_toolsDock;
    QDockWidget* m_propertiesDock;
    QDockWidget* m_layersDock;
    QDockWidget* m_colorDock;
    QDockWidget* m_alignmentDock;
    QDockWidget* m_timelineDock;

    // Panels
    ToolsPanel* m_toolsPanel;
    PropertiesPanel* m_propertiesPanel;
    ColorPanel* m_colorPanel;
    AlignmentPanel* m_alignmentPanel;
    QTabWidget* m_rightPanelTabs;

    // Tools
    std::map<ToolType, std::unique_ptr<Tool>> m_tools;
    ToolType m_currentTool;
    QActionGroup* m_toolActionGroup;

    // Current state
    QString m_currentFile;
    bool m_isModified;
    int m_currentFrame;
    int m_totalFrames;
    double m_currentZoom;
    int m_frameRate;
    bool m_isPlaying;
    QTimer* m_playbackTimer;

    // Undo/Redo system
    QUndoStack* m_undoStack;

    // Animation and layers
    std::vector<std::unique_ptr<AnimationLayer>> m_layers;
    int m_currentLayerIndex;
    std::map<int, std::vector<std::unique_ptr<AnimationKeyframe>>> m_keyframes;
    QAction* m_copyFrameAction;
    QAction* m_blankKeyframeAction;

    // UI Colors and style
    QColor m_currentStrokeColor;
    QColor m_currentFillColor;
    double m_currentStrokeWidth;
    double m_currentOpacity;

    // Actions - File Menu
    QAction* m_newAction;
    QAction* m_openAction;
    QAction* m_saveAction;
    QAction* m_saveAsAction;
    QAction* m_importImageAction;
    QAction* m_importVectorAction;
    QAction* m_importAudioAction;
    QAction* m_exportAnimationAction;
    QAction* m_exportFrameAction;
    QAction* m_exportSVGAction;
    QAction* m_exitAction;

    // Actions - Edit Menu
    QAction* m_undoAction;
    QAction* m_redoAction;
    QAction* m_cutAction;
    QAction* m_copyAction;
    QAction* m_pasteAction;
    QAction* m_selectAllAction;
    QAction* m_groupAction;
    QAction* m_ungroupAction;

    // Actions - View Menu
    QAction* m_zoomInAction;
    QAction* m_zoomOutAction;
    QAction* m_zoomToFitAction;
    QAction* m_toggleGridAction;
    QAction* m_toggleSnapAction;
    QAction* m_toggleRulersAction;

    // Actions - Animation Menu
    QAction* m_playAction;
    QAction* m_stopAction;
    QAction* m_nextFrameAction;
    QAction* m_prevFrameAction;
    QAction* m_nextKeyframeAction;
    QAction* m_prevKeyframeAction;
    QAction* m_firstFrameAction;
    QAction* m_lastFrameAction;
    QAction* m_addKeyframeAction;
    QAction* m_removeKeyframeAction;

    // Actions - Tool Menu
    QAction* m_selectToolAction;
    QAction* m_drawToolAction;
    QAction* m_lineToolAction;
    QAction* m_rectangleToolAction;
    QAction* m_ellipseToolAction;
    QAction* m_textToolAction;
    QAction* m_bucketFillToolAction;
    QAction* m_eraseToolAction;

    // Actions - Object Menu
    QAction* m_alignLeftAction;
    QAction* m_alignCenterAction;
    QAction* m_alignRightAction;
    QAction* m_alignTopAction;
    QAction* m_alignMiddleAction;
    QAction* m_alignBottomAction;
    QAction* m_distributeHorizontallyAction;
    QAction* m_distributeVerticallyAction;
    QAction* m_bringToFrontAction;
    QAction* m_bringForwardAction;
    QAction* m_sendBackwardAction;
    QAction* m_sendToBackAction;
    QAction* m_flipHorizontalAction;
    QAction* m_flipVerticalAction;
    QAction* m_rotateClockwiseAction;
    QAction* m_rotateCounterClockwiseAction;

    // Menus
    QMenu* m_fileMenu;
    QMenu* m_editMenu;
    QMenu* m_objectMenu;
    QMenu* m_viewMenu;
    QMenu* m_animationMenu;
    QMenu* m_helpMenu;
    QMenu* m_importMenu;
    QMenu* m_exportMenu;
    QMenu* m_alignMenu;
    QMenu* m_arrangeMenu;
    QMenu* m_transformMenu;
    QMenu* m_panelsMenu;

    // Toolbars
    QToolBar* m_fileToolBar;
    QToolBar* m_editToolBar;
    QToolBar* m_toolsToolBar;
    QToolBar* m_animationToolBar;
    QToolBar* m_viewToolBar;

    // Status bar widgets
    QLabel* m_statusLabel;
    QLabel* m_positionLabel;
    QLabel* m_zoomLabel;
    QLabel* m_frameLabel;
    QLabel* m_selectionLabel;
    QLabel* m_fpsLabel;

    // Recent files
    enum { MaxRecentFiles = 5 };
    QAction* m_recentFileActions[MaxRecentFiles];
    QAction* m_separatorAction;
};

#endif // MAINWINDOW_H
